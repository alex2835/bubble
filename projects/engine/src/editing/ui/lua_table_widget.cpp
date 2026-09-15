#include "engine/pch/pch.hpp"
#include "engine/editing/ui/lua_table_widget.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/editing/ui/interaction.hpp"
#include "engine/editing/history.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/loader/loader.hpp"
#include "engine/project/project.hpp"
#include <sol/sol.hpp>
#include <imgui.h>
#include "engine/scene/components/tag_component.hpp"

namespace bubble
{
namespace
{
constexpr auto TABLE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen |
                             ImGuiTreeNodeFlags_SpanAllColumns |
                             ImGuiTreeNodeFlags_Framed;

struct DrawCtx
{
    InspectorContext& mCtx;
    const LuaTableRoot& mRoot;
    bool mFixedKeys;

    Project& Project() const { return mCtx.mProject; }
    sol::state& Lua() const { return *mCtx.mProject.mScriptingEngine.mLua; }
};

Any Nil() { return Any( sol::lua_nil ); }

LuaPath Append( LuaPath path, LuaKey key )
{
    path.push_back( std::move( key ) );
    return path;
}

LuaKey KeyOf( const sol::object& k )
{
    if ( k.is<int>() )
        return k.as<int>();
    return k.as<string>();
}

// One value with a widget over a copy: the live table follows the widget at
// once, the undo step is recorded once per interaction.
template <typename T, typename Widget>
void Leaf( const DrawCtx& c, const LuaPath& path, const T& current, Widget&& widget )
{
    T value = current;
    const T before = value;
    const bool changed = widget( value );
    if ( changed )
        SetLuaValue( c.mRoot, path, Any( value ) );

    edit_detail::TrackEdit( changed, before, value, [&]( const T& from, const T& to )
    {
        c.mCtx.mHistory.Record( CreateScope<SetLuaValueCommand>( c.mRoot, path, Any( from ), Any( to ) ) );
    } );
}

// A one-shot structural change: a key added or removed.
void Set( const DrawCtx& c, const LuaPath& path, const Any& from, const Any& to )
{
    c.mCtx.mHistory.Execute( CreateScope<SetLuaValueCommand>( c.mRoot, path, from, to ) );
}

void DrawValue( const DrawCtx& c, const LuaPath& path, string_view name, const Any& any );

void DrawFieldsAdding( const DrawCtx& c, const LuaPath& path, Table& table, string_view scopeName )
{
    if ( c.mFixedKeys )
        return;

    const auto isEmpty = table.empty();
    bool isArray = IsArray( table );
    int newId = (int)table.size() + 1;

    const bool addValue = ImGui::Button( "+", ImVec2( 20, 20 ) );
    ImGui::SameLine();

    // Per-scope statics keyed by table pointer so different callers don't share state
    static std::unordered_map<const void*, string> sFieldNames;
    static std::unordered_map<const void*, int>    sSelectedTypes;
    const void* key = table.pointer();
    string& fieldName   = sFieldNames[key];
    int&    selectedType = sSelectedTypes[key];

    if ( not isArray )
    {
        ImGui::SetNextItemWidth( 100.0f );
        auto fieldLabel = std::format( "##field_{}", scopeName );
        ImGui::InputText( fieldLabel.c_str(), fieldName );
        ImGui::SameLine();
    }
    if ( auto val = TryParseInt( fieldName );
         isEmpty and val and val >= 1 )
    {
        isArray = true;
        newId = *val;
    }

    ImGui::SetNextItemWidth( 100.0f );
    constexpr string_view types = "Int\0Float\0String\0Bool\0Vec2\0Vec3\0Vec4\0Mat3\0Mat4\0Table\0Texture2D\0Entity\0"sv;
    auto typeLabel = std::format( "##type_{}", scopeName );
    ImGui::Combo( typeLabel.c_str(), &selectedType, types.data() );

    if ( not addValue or fieldName.empty() )
        return;

    const LuaKey entryKey = isArray ? LuaKey( newId ) : LuaKey( fieldName );

    enum Types { Int, Float, String, Bool, Vec2, Vec3, Vec4, Mat3, Mat4, TableT, Texture2D, EntityT };
    Any value = Nil();
    switch ( selectedType )
    {
        case Int:       value = 0; break;
        case Float:     value = 0.0f; break;
        case String:    value = ""s; break;
        case Bool:      value = false; break;
        case Vec2:      value = vec2( 0 ); break;
        case Vec3:      value = vec3( 0 ); break;
        case Vec4:      value = vec4( 0 ); break;
        case Mat3:      value = mat3( 1 ); break;
        case Mat4:      value = mat4( 1 ); break;
        case TableT:    value = c.Lua().create_table(); break;
        case Texture2D: value = Ref<bubble::Texture2D>{}; break;
        // Nothing until picked from the combo. This used to create a bare
        // entity in the scene as a side effect of adding a field.
        case EntityT:   value = INVALID_ENTITY; break;
    }
    Set( c, Append( path, entryKey ), Nil(), value );
}

void DrawTable( const DrawCtx& c, const LuaPath& path, string_view name, Table table, bool isArray )
{
    const char* fmt = isArray ? "%s (array)" : "%s (table)";
    if ( not ImGui::TreeNodeEx( isArray ? "##array" : "##table", TABLE_FLAGS, fmt, name.data() ) )
        return;

    int i = 0;
    for ( auto& [k, v] : table )
    {
        ImGui::PushID( ( i32 )reinterpret_cast<i64>( table.pointer() ) + i );
        const LuaKey key = KeyOf( k );
        const LuaPath entryPath = Append( path, key );

        if ( not c.mFixedKeys and ImGui::Button( "-" ) )
        {
            // Setting an existing key to nil is allowed mid-traversal.
            Set( c, entryPath, v.as<Any>(), Nil() );
            ImGui::PopID();
            continue;
        }
        if ( not c.mFixedKeys )
            ImGui::SameLine();

        const string entryName = std::visit( []( const auto& kk ) { return std::format( "{}", kk ); }, key );
        DrawValue( c, entryPath, entryName, v.as<Any>() );

        ImGui::Separator();
        ImGui::PopID();
        i++;
    }
    DrawFieldsAdding( c, path, table, name );
    ImGui::TreePop();
}

void DrawValue( const DrawCtx& c, const LuaPath& path, string_view name, const Any& any )
{
    // Scope all widget IDs under `name` so identical field names in different
    // components (State vs ShaderUniforms) don't collide.
    struct IDGuard
    {
        IDGuard( string_view id ) { ImGui::PushID( id.data(), id.data() + id.size() ); }
        ~IDGuard() { ImGui::PopID(); }
    } idGuard( name );

    ImGui::SetNextItemWidth( 100.0f );
    if ( any.is<sol::nil_t>() )
    {
        ImGui::SameLine();
        ImGui::Text( "(nill)" );
    }
    else if ( any.is<int>() )
    {
        Leaf( c, path, any.as<int>(), [&]( int& v ) { return ImGui::DragInt( name.data(), &v ); } );
        ImGui::SameLine();
        ImGui::Text( "(int)" );
    }
    else if ( any.is<float>() )
    {
        Leaf( c, path, any.as<float>(), [&]( float& v ) { return ImGui::DragFloat( name.data(), &v ); } );
        ImGui::SameLine();
        ImGui::Text( "(float)" );
    }
    else if ( any.is<std::string>() )
    {
        Leaf( c, path, any.as<string>(), [&]( string& v ) { return ImGui::InputText( name.data(), v ); } );
        ImGui::SameLine();
        ImGui::Text( "(string)" );
    }
    else if ( any.is<bool>() )
    {
        Leaf( c, path, any.as<bool>(), [&]( bool& v ) { return ImGui::Checkbox( name.data(), &v ); } );
        ImGui::SameLine();
        ImGui::Text( "(bool)" );
    }
    else if ( any.is<vec2>() )
    {
        Leaf( c, path, any.as<vec2>(), [&]( vec2& v ) { return ImGui::DragFloat2( name.data(), &v.x ); } );
        ImGui::SameLine();
        ImGui::Text( "(vec2)" );
    }
    else if ( any.is<vec3>() )
    {
        Leaf( c, path, any.as<vec3>(), [&]( vec3& v ) { return ImGui::DragFloat3( name.data(), &v.x ); } );
        ImGui::SameLine();
        ImGui::Text( "(vec3)" );
    }
    else if ( any.is<vec4>() )
    {
        Leaf( c, path, any.as<vec4>(), [&]( vec4& v ) { return ImGui::DragFloat4( name.data(), &v.x ); } );
        ImGui::SameLine();
        ImGui::Text( "(vec4)" );
    }
    else if ( any.is<mat3>() )
    {
        ImGui::Text( "%s (mat3)", name.data() );
        // One group, so the rows count as one item for the interaction
        // tracking: a drag on any row is one step.
        Leaf( c, path, any.as<mat3>(), [&]( mat3& v )
        {
            bool changed = false;
            ImGui::BeginGroup();
            for ( int i = 0; i < 3; i++ )
            {
                auto label = std::format( "{}[{}]", name, i );
                changed |= ImGui::DragFloat3( label.c_str(), &v[i].x );
            }
            ImGui::EndGroup();
            return changed;
        } );
    }
    else if ( any.is<mat4>() )
    {
        ImGui::Text( "%s (mat4)", name.data() );
        Leaf( c, path, any.as<mat4>(), [&]( mat4& v )
        {
            bool changed = false;
            ImGui::BeginGroup();
            for ( int i = 0; i < 4; i++ )
            {
                auto label = std::format( "{}[{}]", name, i );
                changed |= ImGui::DragFloat4( label.c_str(), &v[i].x );
            }
            ImGui::EndGroup();
            return changed;
        } );
    }
    else if ( any.is<Entity>() )
    {
        // Build list of all entities that have a tag
        vector<Entity> entities;
        vector<string> entityNames;
        c.Project().mLevel.mScene.ForEach<TagComponent>( [&]( Entity entity, const TagComponent& tag )
        {
            entities.push_back( entity );
            entityNames.push_back( std::format( "[{}] {}", (size_t)entity, tag.mName ) );
        } );

        Leaf( c, path, any.as<Entity>(), [&]( Entity& current )
        {
            int selectedIdx = -1;
            for ( int i = 0; i < (int)entities.size(); i++ )
                if ( entities[i] == current )
                {
                    selectedIdx = i;
                    break;
                }

            bool changed = false;
            ImGui::SetNextItemWidth( 150.0f );
            const string preview = selectedIdx >= 0 ? entityNames[selectedIdx] : "None";
            if ( ImGui::BeginCombo( name.data(), preview.c_str() ) )
            {
                for ( int i = 0; i < (int)entities.size(); i++ )
                {
                    const bool isSelected = ( i == selectedIdx );
                    if ( ImGui::Selectable( entityNames[i].c_str(), isSelected ) and not isSelected )
                    {
                        current = entities[i];
                        changed = true;
                    }
                    if ( isSelected )
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            return changed;
        } );
        ImGui::SameLine();
        ImGui::Text( "(Entity)" );
    }
    else if ( any.is<Ref<Texture2D>>() )
    {
        // Build list from loader
        vector<Ref<Texture2D>> textures;
        vector<string> textureNames;
        for ( const auto& [texPath, tex] : c.Project().mLoader.mTextures )
        {
            textures.push_back( tex );
            textureNames.push_back( texPath.filename().string() );
        }

        const auto current = any.as<Ref<Texture2D>>();
        Leaf( c, path, current, [&]( Ref<Texture2D>& picked )
        {
            int selectedIdx = -1;
            for ( int i = 0; i < (int)textures.size(); i++ )
                if ( textures[i] == picked )
                {
                    selectedIdx = i;
                    break;
                }

            bool changed = false;
            const string preview = selectedIdx >= 0 ? textureNames[selectedIdx] : "None";
            ImGui::SetNextItemWidth( 150.0f );
            if ( ImGui::BeginCombo( name.data(), preview.c_str() ) )
            {
                if ( ImGui::Selectable( "None", selectedIdx == -1 ) and picked )
                {
                    picked = nullptr;
                    changed = true;
                }
                for ( int i = 0; i < (int)textures.size(); i++ )
                {
                    const bool isSelected = ( i == selectedIdx );
                    ImGui::Image( (ImTextureID)textures[i]->ImTextureId(), ImVec2( 24, 24 ) );
                    ImGui::SameLine();
                    if ( ImGui::Selectable( textureNames[i].c_str(), isSelected ) and not isSelected )
                    {
                        picked = textures[i];
                        changed = true;
                    }
                    if ( isSelected )
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            return changed;
        } );
        ImGui::SameLine();
        ImGui::Text( "(Texture2D)" );
        if ( current )
            ImGui::Image( (ImTextureID)current->ImTextureId(), ImVec2( 64, 64 ) );
    }
    else if ( any.is<Table>() )
    {
        const auto table = any.as<Table>();
        DrawTable( c, path, name, table, IsArray( table ) );
    }
    else
    {
        BUBBLE_ASSERT( false, "Invalid any value" );
        throw std::runtime_error( "DrawLuaTable: invalid value type" );
    }
}
}

void DrawLuaTable( InspectorContext& ctx, const LuaTableRoot& root, bool fixedKeys )
{
    const auto table = root.Get();
    if ( not table )
        return;

    const DrawCtx c{ ctx, root, fixedKeys };
    ImGui::PushID( root.mName.c_str() );
    DrawTable( c, {}, root.mName, *table, IsArray( *table ) );
    ImGui::PopID();
}

} // namespace bubble
