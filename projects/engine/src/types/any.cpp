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
    else if ( value.is<vec2>() )
    {
        const auto& v = value.as<vec2>();
        return std::format( "(vec2)'[{},{}]'", v.x, v.y );
    }
    else if ( value.is<vec3>() )
    {
        const auto& v = value.as<vec3>();
        return std::format( "(vec3)'[{},{},{}]'", v.x, v.y, v.z );
    }
    else if ( value.is<vec4>() )
    {
        const auto& v = value.as<vec4>();
        return std::format( "(vec4)'[{},{},{},{}]'", v.x, v.y, v.z, v.w );
    }
    else if ( value.is<mat3>() )
        return "(mat3)";
    else if ( value.is<mat4>() )
        return "(mat4)";
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

json SaveAnyValue( const Any& v )
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
    else if ( v.is<vec2>() )
    {
        const auto& val = v.as<vec2>();
        return json{ { "__type", "vec2" }, { "x", val.x }, { "y", val.y } };
    }
    else if ( v.is<vec3>() )
    {
        const auto& val = v.as<vec3>();
        return json{ { "__type", "vec3" }, { "x", val.x }, { "y", val.y }, { "z", val.z } };
    }
    else if ( v.is<vec4>() )
    {
        const auto& val = v.as<vec4>();
        return json{ { "__type", "vec4" }, { "x", val.x }, { "y", val.y }, { "z", val.z }, { "w", val.w } };
    }
    else if ( v.is<mat3>() )
    {
        const auto& val = v.as<mat3>();
        json j;
        j["__type"] = "mat3";
        for ( int i = 0; i < 3; i++ )
            j["cols"].push_back( { val[i].x, val[i].y, val[i].z } );
        return j;
    }
    else if ( v.is<mat4>() )
    {
        const auto& val = v.as<mat4>();
        json j;
        j["__type"] = "mat4";
        for ( int i = 0; i < 4; i++ )
            j["cols"].push_back( { val[i].x, val[i].y, val[i].z, val[i].w } );
        return j;
    }
    else if ( v.is<Table>() and IsArray( v.as<Table>() ) )
    {
        json j = json::array();
        auto table = v.as<Table>();
        for ( const auto& [k, val] : table )
            j.push_back( SaveAnyValue( val ) );
        return j;
    }
    else if ( v.is<Table>() )
    {
        json j = json::object();
        auto table = v.as<Table>();
        for ( const auto& [k, val] : table )
            j[k.as<string>()] = SaveAnyValue( val );
        return j;
    }
    else
    {
        PrintAnyValue( v );
        throw std::runtime_error( "Value of not supported type" );
    }
}


Any LoadAnyValue( ScriptingEngine& se, const json& j )
{
    if ( j.is_number_integer() )
        return j.get<int>();
    else if ( j.is_number_float() )
        return j.get<float>();
    else if ( j.is_string() )
        return j.get<string>();
    else if ( j.is_boolean() )
        return j.get<bool>();
    else if ( j.is_object() and j.contains( "__type" ) )
    {
        auto type = j["__type"].get<string>();
        if ( type == "Entity" )
        {
            auto id = j["id"].get<size_t>();
            return *(Entity*)&id;
        }
        else if ( type == "Texture2D" )
        {
            auto texPath = j["path"].get<string>();
            if ( texPath.empty() )
                return Ref<Texture2D>{};
            return LoadTexture2D( texPath );
        }
        else if ( type == "vec2" )
            return vec2( j["x"], j["y"] );
        else if ( type == "vec3" )
            return vec3( j["x"], j["y"], j["z"] );
        else if ( type == "vec4" )
            return vec4( j["x"], j["y"], j["z"], j["w"] );
        else if ( type == "mat3" )
        {
            mat3 m;
            for ( int i = 0; i < 3; i++ )
                m[i] = vec3( j["cols"][i][0], j["cols"][i][1], j["cols"][i][2] );
            return m;
        }
        else if ( type == "mat4" )
        {
            mat4 m;
            for ( int i = 0; i < 4; i++ )
                m[i] = vec4( j["cols"][i][0], j["cols"][i][1], j["cols"][i][2], j["cols"][i][3] );
            return m;
        }
    }
    else if ( j.is_array() )
    {
        auto table = se.CreateTable();
        int i = 1;
        for ( const auto& v : j )
            table[i++] = LoadAnyValue( se, v );
        return table;
    }
    else if ( j.is_object() )
    {
        auto table = se.CreateTable();
        for ( const auto& [k, v] : j.items() )
            table[k] = LoadAnyValue( se, v );
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
    const auto isEmpty = table.empty();
    bool isArray = IsArray( table );
    int newId = (int)table.size() + 1;

    const bool addValue = ImGui::Button( "+", ImVec2( 20, 20 ) );
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
    constexpr string_view types = "Int\0Float\0String\0Bool\0Vec2\0Vec3\0Vec4\0Mat3\0Mat4\0Table\0Texture2D\0Entity\0"sv;
    static enum class Types { Int, Float, String, Bool, Vec2, Vec3, Vec4, Mat3, Mat4, Table, Texture2D, Entity } selectedType;
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
            case Types::Int:       table[key] = 0; break;
            case Types::Float:     table[key] = 0.0f; break;
            case Types::String:    table[key] = ""s; break;
            case Types::Bool:      table[key] = false; break;
            case Types::Vec2:      table[key] = vec2( 0 ); break;
            case Types::Vec3:      table[key] = vec3( 0 ); break;
            case Types::Vec4:      table[key] = vec4( 0 ); break;
            case Types::Mat3:      table[key] = mat3( 1 ); break;
            case Types::Mat4:      table[key] = mat4( 1 ); break;
            case Types::Table:     table[key] = lua.create_table(); break;
            case Types::Texture2D: table[key] = Ref<Texture2D>{}; break;
            case Types::Entity:
            {
                auto entity = project.mScene.CreateEntity();
                table[key] = entity;
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
        project.mScene.ForEach<TagComponent>( [&]( Entity entity, const TagComponent& tag )
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
                ImGui::Image( (ImTextureID)(u64)textures[i]->RendererID(), ImVec2( 24, 24 ) );
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
            ImGui::Image( (ImTextureID)(u64)currentTexture->RendererID(), ImVec2( 64, 64 ) );
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

