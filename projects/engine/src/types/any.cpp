#include "engine/pch/pch.hpp"
#include "engine/types/any.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/types/string.hpp"
#include "engine/types/json.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/loader/loader.hpp"
#include "engine/project/project.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <imgui.h>
#include <print>

namespace bubble
{
    
bool IsClass( const Table& tbl )
{
    if ( not tbl.valid() )
        return false;
    sol::object meta = tbl[sol::metatable_key];
    if ( meta == sol::nil )
        return false;
    return meta.as<sol::table>()["__name"] != sol::nil;
}

bool IsArray( const Table& tbl )
{
    if ( tbl.empty() )
        return false;

    for ( auto& [key, value] : tbl )
    {
        if ( not key.is<int>() or key.as<int>() < 1 )
            return false;
    }
    return true;
}

string AnyValueToString( const Any& value )
{
    if ( value.is<sol::nil_t>() )
        return "nil";
    else if ( value.is<Table>() )
    {
        const auto& table = value.as<Table>();
        if ( IsClass( table ) )
        {
            auto metatable = table[sol::metatable_key];
            sol::optional<sol::function> tostring_fn = metatable["__tostring"];
            if ( tostring_fn != sol::nil )
            {
                std::string className = metatable["__name"].get_or( std::string( "user type" ) );
                std::string objectString = ( *tostring_fn )( table );
                return std::format( "({}){}", className, objectString );
            }
            else
                return "(user class) no __tostring";
        }
        else if ( IsArray( table ) ) // array
        {
            string result = "[";
            for ( auto& [idx, val] : table )
                result += AnyValueToString( val ) + ", ";
            result += "]";
            return result;
        }
        else // map
        {
            string result = "{";
            for ( auto& [key, val] : table )
                result += AnyValueToString( key ) + " : " + AnyValueToString( val ) + "; ";
            result += "}";
            return result;
        }
    }
    else if ( value.is<int>() )
        return std::format( "(int)'{}'", value.as<int>() );
    else if ( value.is<float>() )
        return std::format( "(float)'{}'", value.as<float>() );
    else if ( value.is<std::string>() )
        return std::format( "(string)'{}'", value.as<std::string>() );
    else if ( value.is<bool>() )
        return std::format( "(bool)'{}'", value.as<bool>() );
    else if ( value.is<Entity>() )
        return std::format( "(Entity)'{}'", (size_t)value.as<Entity>() );
    else if ( value.is<Ref<Texture2D>>() )
    {
        const auto& texture = value.as<Ref<Texture2D>>();
        return std::format( "(Texture2D)'{}'", texture ? texture->mPath.string() : "null" );
    }
    else
        return "(unknown)";
}

void PrintAnyValue( const Any& value )
{
    std::println( "{}", AnyValueToString( value ) );
}

json SaveAnyValue( const Project& project, const Any& v )
{
    if ( v.is<int>() )
        return v.as<int>();
    else if ( v.is<float>() )
        return v.as<float>();
    else if ( v.is<string>() )
        return v.as<string>();
    else if ( v.is<bool>() )
        return v.as<bool>();
    else if ( v.is<Entity>() )
    {
        json j;
        j["__type"] = "Entity";
        j["id"] = (size_t)v.as<Entity>();
        return j;
    }
    else if ( v.is<Ref<Texture2D>>() )
    {
        json j;
        j["__type"] = "Texture2D";
        auto& tex = v.as<Ref<Texture2D>>();
        j["path"] = tex ? tex->mPath.string() : "";
        return j;
    }
    else if ( v.is<Table>() and IsArray( v.as<Table>() ) )
    {
        json j = json::array();
        auto table = v.as<Table>();
        for ( const auto& [k, val] : table )
            j.push_back( SaveAnyValue( project, val ) );
        return j;
    }
    else if ( v.is<Table>() )
    {
        json j = json::object();
        auto table = v.as<Table>();
        for ( const auto& [k, val] : table )
            j[k.as<string>()] = SaveAnyValue( project, val );
        return j;
    }
    else
    {
        PrintAnyValue( v );
        throw std::runtime_error( "Value of not supported type" );
    }
}


Any LoadAnyValue( Project& project, const json& j )
{
    auto& lua = *project.mScriptingEngine.mLua;
    LogInfo( "LoadAnyValue lua_State* {}", (i64)lua.lua_state() );

    if ( j.is_number_integer() )
        return Any( lua, j.get<int>() );
    else if ( j.is_number_float() )
        return Any( lua, j.get<float>() );
    else if ( j.is_string() )
        return Any( lua, j.get<string>() );
    else if ( j.is_boolean() )
        return Any( lua, j.get<bool>() );
    else if ( j.is_object() and j.contains( "__type" ) )
    {
        auto type = j["__type"].get<string>();
        if ( type == "Entity" )
        {
            auto id = j["id"].get<size_t>();
            auto entity = project.mScene.GetEntityById( id );
            return Any( lua, entity );
        }
        else if ( type == "Texture2D" )
        {
            auto texPath = j["path"].get<string>();
            if ( texPath.empty() )
                return Any( lua, Ref<Texture2D>{} );
            return Any( lua, LoadTexture2D( texPath ) );
        }
    }
    else if ( j.is_array() )
    {
        auto table = project.mScriptingEngine.CreateTable();
        int i = 1;
        for ( const auto& v : j )
            table[i++] = LoadAnyValue( project, v );
        return table;
    }
    else if ( j.is_object() )
    {
        auto table = project.mScriptingEngine.CreateTable();
        for ( const auto& [k, v] : j.items() )
            table[k] = LoadAnyValue( project, v );
        return table;
    }
    throw std::runtime_error( std::format( "Value of not supported type: {}", string( j ) ) );
}


Any AnyDeepCopy( const Any& any )
{
    if ( any.is<Table>() )
    {
        auto table = any.as<Table>();
        sol::state_view lua = table.lua_state();
        auto newTable = lua.create_table();
        for ( auto& [k, v] : table )
            newTable[k] = v.is<Table>() ? AnyDeepCopy( v ) : v;
        return newTable;
    }
    return any;
}

Scope<Any> AnyDeepCopy( const Scope<Any>& any )
{
    BUBBLE_ASSERT( any, "Empty pointer copy" );
    return CreateScope<Any>( AnyDeepCopy( *any ) );
}


void DrawFieldsAdding( Project& project, Table& table )
{
    auto isEmpty = table.empty();
    auto isArray = IsArray( table );
    int newId = (int)table.size() + 1;

    bool addValue = ImGui::Button( "+", ImVec2( 20, 20 ) );
    ImGui::SameLine();

    static string fieldName;
    if ( not isArray )
    {
        ImGui::SetNextItemWidth( 100.0f );
        ImGui::InputText( "##state", fieldName );
        ImGui::SameLine();
    }
    if ( auto val = TryParseInt( fieldName );
         isEmpty and val and val >= 1 )
    {
        isArray = true;
        newId = *val;
    }

    ImGui::SetNextItemWidth( 100.0f );
    constexpr string_view types = "Int\0Float\0String\0Bool\0Table\0Texture2D\0Entity\0"sv;
    static enum class Types { Int, Float, String, Bool, Table, Texture2D, Entity } selectedType;
    ImGui::Combo( "##type", (int*)&selectedType, types.data() );

    if ( addValue )
    {
        if ( fieldName.empty() )
            return;

        sol::state_view lua = table.lua_state();
        auto key = isArray ? sol::object( lua, sol::in_place, newId )
            : sol::object( lua, sol::in_place, fieldName );

        switch ( selectedType )
        {
            case Types::Int:
                table[key] = Any( lua, 0 );
                break;
            case Types::Float:
                table[key] = Any( lua, 0.0f );
                break;
            case Types::String:
                table[key] = Any( lua, ""s );
                break;
            case Types::Bool:
                table[key] = Any( lua, false );
                break;
            case Types::Table:
                table[key] = lua.create_table();
                break;
            case Types::Texture2D:
                table[key] = Any( lua, Ref<Texture2D>{} );
                break;
            case Types::Entity:
            {
                auto entity = project.mScene.CreateEntity();
                table[key] = Any( lua, entity );
                break;
            }
        }
    }
}

Any DrawAnyValue( Project& project, string_view name, Any any )
{
    constexpr auto TABLE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen |
                                 ImGuiTreeNodeFlags_SpanAllColumns |
                                 ImGuiTreeNodeFlags_Framed;

    auto& lua = *project.mScriptingEngine.mLua;

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
        return Any( lua, value );
    }
    else if ( any.is<float>() )
    {
        auto value = any.as<float>();
        ImGui::DragFloat( name.data(), &value );
        ImGui::SameLine();
        ImGui::Text( "(float)" );
        return Any( lua, value );
    }
    else if ( any.is<std::string>() )
    {
        auto value = any.as<string>();
        ImGui::InputText( name.data(), value );
        ImGui::SameLine();
        ImGui::Text( "(string)" );
        return Any( lua, value );
    }
    else if ( any.is<bool>() )
    {
        auto value = any.as<bool>();
        ImGui::Checkbox( name.data(), &value );
        ImGui::SameLine();
        ImGui::Text( "(bool)" );
        return Any( lua, value );
    }
    else if ( any.is<Entity>() )
    {
        auto current = any.as<Entity>();

        // Build list of all entities that have a tag
        vector<Entity> entities;
        vector<string> entityNames;
        project.mScene.ForEach<TagComponent>( [&]( Entity entity, const TagComponent& tag )
        {
            entities.push_back( entity );
            entityNames.push_back( std::format( "[{}] {}", (size_t)entity, tag.mName ) );
        } );

        // Find current selection index
        int selectedIdx = -1;
        for ( int i = 0; i < (int)entities.size(); i++ )
            if ( entities[i] == current )
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
                    current = entities[i];
                if ( isSelected )
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::Text( "(Entity)" );
        return Any( lua, current );
    }
    else if ( any.is<Ref<Texture2D>>() )
    {
        auto current = any.as<Ref<Texture2D>>();

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
            if ( textures[i] == current )
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
                current = nullptr;

            for ( int i = 0; i < (int)textures.size(); i++ )
            {
                bool isSelected = ( i == selectedIdx );
                ImGui::Image( (ImTextureID)(u64)textures[i]->RendererID(), ImVec2( 24, 24 ) );
                ImGui::SameLine();
                if ( ImGui::Selectable( textureNames[i].c_str(), isSelected ) )
                    current = textures[i];
                if ( isSelected )
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::Text( "(Texture2D)" );
        if ( current )
            ImGui::Image( (ImTextureID)(u64)current->RendererID(), ImVec2( 64, 64 ) );
        return Any( lua, current );
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

                if ( ImGui::Button( "-" ) )
                {
                    table[k] = sol::nil;
                    ImGui::PopID();
                    continue;
                }
                ImGui::SameLine();

                auto entryName = std::to_string( k.as<int>() );
                table[k] = DrawAnyValue( project, entryName, v.as<Any>() );

                ImGui::Separator();
                ImGui::PopID();
                i++;
            }
            DrawFieldsAdding( project, table );
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

                if ( ImGui::Button( "-" ) )
                {
                    table[k] = sol::nil;
                    ImGui::PopID();
                    continue;
                }
                ImGui::SameLine();

                auto entryName = k.as<string>();
                table[k] = DrawAnyValue( project, entryName, v.as<Any>() );

                ImGui::Separator();
                ImGui::PopID();
                i++;
            }
            DrawFieldsAdding( project, table );
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

