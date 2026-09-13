#include "engine/pch/pch.hpp"
#include "engine/types/any_draw.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/loader/loader.hpp"
#include "engine/project/project.hpp"
#include <sol/sol.hpp>
#include <imgui.h>

namespace bubble
{
void DrawFieldsAdding( Project& project, Table& table, string_view scopeName, bool frozen )
{
    if ( frozen )
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

    if ( addValue )
    {
        if ( fieldName.empty() )
            return;

        sol::state_view lua = table.lua_state();
        auto entryKey = isArray ? sol::object( lua, sol::in_place, newId )
            : sol::object( lua, sol::in_place, fieldName );

        enum Types { Int, Float, String, Bool, Vec2, Vec3, Vec4, Mat3, Mat4, Table, Texture2D, Entity };
        switch ( selectedType )
        {
            case Int:       table[entryKey] = 0; break;
            case Float:     table[entryKey] = 0.0f; break;
            case String:    table[entryKey] = ""s; break;
            case Bool:      table[entryKey] = false; break;
            case Vec2:      table[entryKey] = vec2( 0 ); break;
            case Vec3:      table[entryKey] = vec3( 0 ); break;
            case Vec4:      table[entryKey] = vec4( 0 ); break;
            case Mat3:      table[entryKey] = mat3( 1 ); break;
            case Mat4:      table[entryKey] = mat4( 1 ); break;
            case Table:     table[entryKey] = lua.create_table(); break;
            case Texture2D: table[entryKey] = Ref<bubble::Texture2D>{}; break;
            case Entity:
            {
                auto entity = project.mLevel.mScene.CreateEntity();
                table[entryKey] = entity;
                break;
            }
        }
    }
}

Any DrawAnyValue( Project& project, string_view name, Any any, bool frozen )
{
    constexpr auto TABLE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen |
                                 ImGuiTreeNodeFlags_SpanAllColumns |
                                 ImGuiTreeNodeFlags_Framed;

    auto& lua = *project.mScriptingEngine.mLua;

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
        return any;
    }
    else if ( any.is<int>() )
    {
        auto value = any.as<int>();
        ImGui::DragInt( name.data(), &value );
        ImGui::SameLine();
        ImGui::Text( "(int)" );
        return value;
    }
    else if ( any.is<float>() )
    {
        auto value = any.as<float>();
        ImGui::DragFloat( name.data(), &value );
        ImGui::SameLine();
        ImGui::Text( "(float)" );
        return value;
    }
    else if ( any.is<std::string>() )
    {
        auto value = any.as<string>();
        ImGui::InputText( name.data(), value );
        ImGui::SameLine();
        ImGui::Text( "(string)" );
        return value;
    }
    else if ( any.is<bool>() )
    {
        auto value = any.as<bool>();
        ImGui::Checkbox( name.data(), &value );
        ImGui::SameLine();
        ImGui::Text( "(bool)" );
        return value;
    }
    else if ( any.is<vec2>() )
    {
        auto value = any.as<vec2>();
        ImGui::DragFloat2( name.data(), &value.x );
        ImGui::SameLine();
        ImGui::Text( "(vec2)" );
        return value;
    }
    else if ( any.is<vec3>() )
    {
        auto value = any.as<vec3>();
        ImGui::DragFloat3( name.data(), &value.x );
        ImGui::SameLine();
        ImGui::Text( "(vec3)" );
        return value;
    }
    else if ( any.is<vec4>() )
    {
        auto value = any.as<vec4>();
        ImGui::DragFloat4( name.data(), &value.x );
        ImGui::SameLine();
        ImGui::Text( "(vec4)" );
        return value;
    }
    else if ( any.is<mat3>() )
    {
        auto value = any.as<mat3>();
        ImGui::Text( "%s (mat3)", name.data() );
        for ( int i = 0; i < 3; i++ )
        {
            auto label = std::format( "{}[{}]", name, i );
            ImGui::DragFloat3( label.c_str(), &value[i].x );
        }
        return value;
    }
    else if ( any.is<mat4>() )
    {
        auto value = any.as<mat4>();
        ImGui::Text( "%s (mat4)", name.data() );
        for ( int i = 0; i < 4; i++ )
        {
            auto label = std::format( "{}[{}]", name, i );
            ImGui::DragFloat4( label.c_str(), &value[i].x );
        }
        return value;
    }
    else if ( any.is<Entity>() )
    {
        auto currentEntity = any.as<Entity>();

        // Build list of all entities that have a tag
        vector<Entity> entities;
        vector<string> entityNames;
        project.mLevel.mScene.ForEach<TagComponent>( [&]( Entity entity, const TagComponent& tag )
        {
            entities.push_back( entity );
            entityNames.push_back( std::format( "[{}] {}", (size_t)entity, tag.mName ) );
        } );

        // Find current selection index
        int selectedIdx = -1;
        for ( int i = 0; i < (int)entities.size(); i++ )
            if ( entities[i] == currentEntity )
            {
                selectedIdx = i;
                break;
            }

        // Build c-string array for combo
        vector<const char*> items;
        items.reserve( entityNames.size() );
        for ( const auto& n : entityNames )
            items.push_back( n.c_str() );

        ImGui::SetNextItemWidth( 150.0f );
        string preview = selectedIdx >= 0 ? entityNames[selectedIdx] : "None";
        if ( ImGui::BeginCombo( name.data(), preview.c_str() ) )
        {
            for ( int i = 0; i < (int)entities.size(); i++ )
            {
                bool isSelected = ( i == selectedIdx );
                if ( ImGui::Selectable( items[i], isSelected ) )
                    currentEntity = entities[i];
                if ( isSelected )
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::Text( "(Entity)" );
        return currentEntity;
    }
    else if ( any.is<Ref<Texture2D>>() )
    {
        auto currentTexture = any.as<Ref<Texture2D>>();

        // Build list from loader
        vector<Ref<Texture2D>> textures;
        vector<string> textureNames;
        for ( const auto& [texPath, tex] : project.mLoader.mTextures )
        {
            textures.push_back( tex );
            textureNames.push_back( texPath.filename().string() );
        }

        // Find current selection index
        int selectedIdx = -1;
        for ( int i = 0; i < (int)textures.size(); i++ )
        {
            if ( textures[i] == currentTexture )
            {
                selectedIdx = i;
                break;
            }
        }

        string preview = selectedIdx >= 0 ? textureNames[selectedIdx] : "None";
        ImGui::SetNextItemWidth( 150.0f );
        if ( ImGui::BeginCombo( name.data(), preview.c_str() ) )
        {
            if ( ImGui::Selectable( "None", selectedIdx == -1 ) )
                currentTexture = nullptr;

            for ( int i = 0; i < (int)textures.size(); i++ )
            {
                bool isSelected = ( i == selectedIdx );
                ImGui::Image( (ImTextureID)textures[i]->ImTextureId(), ImVec2( 24, 24 ) );
                ImGui::SameLine();
                if ( ImGui::Selectable( textureNames[i].c_str(), isSelected ) )
                    currentTexture = textures[i];
                if ( isSelected )
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::Text( "(Texture2D)" );
        if ( currentTexture )
            ImGui::Image( (ImTextureID)currentTexture->ImTextureId(), ImVec2( 64, 64 ) );
        return currentTexture;
    }
    else if ( any.is<Table>() and IsArray( any.as<Table>() ) )
    {
        int i = 0;
        auto table = any.as<Table>();
        if ( ImGui::TreeNodeEx( "##array", TABLE_FLAGS, "%s (array)", name.data() ) )
        {
            for ( auto& [k, v] : table )
            {
                ImGui::PushID( ( i32 )reinterpret_cast<i64>( table.pointer() ) + i );

                if ( !frozen && ImGui::Button( "-" ) )
                {
                    table[k] = sol::nil;
                    ImGui::PopID();
                    continue;
                }
                if ( !frozen ) ImGui::SameLine();

                auto entryName = std::to_string( k.as<int>() );
                table[k] = DrawAnyValue( project, entryName, v.as<Any>(), frozen );

                ImGui::Separator();
                ImGui::PopID();
                i++;
            }
            DrawFieldsAdding( project, table, name, frozen );
            ImGui::TreePop();
        }
    }
    else if ( any.is<Table>() )
    {
        int i = 0;
        auto table = any.as<Table>();
        if ( ImGui::TreeNodeEx( "##table", TABLE_FLAGS, "%s (table)", name.data() ) )
        {
            for ( auto& [k, v] : table )
            {
                ImGui::PushID( ( i32 )reinterpret_cast<i64>( table.pointer() ) + i );

                if ( !frozen && ImGui::Button( "-" ) )
                {
                    table[k] = sol::nil;
                    ImGui::PopID();
                    continue;
                }
                if ( !frozen ) ImGui::SameLine();

                auto entryName = k.as<string>();
                table[k] = DrawAnyValue( project, entryName, v.as<Any>(), frozen );

                ImGui::Separator();
                ImGui::PopID();
                i++;
            }
            DrawFieldsAdding( project, table, name, frozen );
            ImGui::TreePop();
        }
    }
    else
    {
        BUBBLE_ASSERT( false, "Invalid any value" );
        throw std::runtime_error( "DrawAny(): Invalid Any value type" );
    }
    return any;
}

} // namespace bubble
