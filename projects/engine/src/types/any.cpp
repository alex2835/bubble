#include "engine/pch/pch.hpp"
#include "engine/types/any.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/types/string.hpp"
#include "engine/types/json.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/loader/loader.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <imgui.h>
#include <print>

namespace bubble
{
constexpr auto TABLE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen |
                             ImGuiTreeNodeFlags_SpanAllColumns |
                             ImGuiTreeNodeFlags_Framed;

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

void PrintAnyValue( const Any& value )
{
    if ( value.is<sol::nil_t>() )
        std::print( "nil" );
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
                std::print( "({}){}", className, objectString );
            }
            else
                std::print( "(user class) no __tostring" );
        }
        else if ( IsArray( table ) ) // array
        {
            std::print( "[" );
            for ( auto& [idx, value] : table )
            {
                PrintAnyValue( value );
                std::print( ", " );
            }
            std::print( "]" );
        }
        else // map
        {
            std::print( "{{" );
            for ( auto& [key, value] : table )
            {
                PrintAnyValue( key );
                std::print( " : " );
                PrintAnyValue( value );
                std::print( "; " );
            }
            std::print( "}}" );
        }
    }
    else
    {
        if ( value.is<int>() )
            std::print( "(int)'{}'", value.as<int>() );
        else if ( value.is<float>() )
            std::print( "(float)'{}'", value.as<float>() );
        else if ( value.is<std::string>() )
            std::print( "(string)'{}'", value.as<std::string>() );
        else if ( value.is<bool>() )
            std::print( "(bool)'{}'", value.as<bool>() );
        else if ( value.is<Entity>() )
            std::print( "(Entity)'{}'", (size_t)value.as<Entity>() );
        else if ( value.is<Ref<Texture2D>>() )
        {
            const auto& texture = value.as<Ref<Texture2D>>();
            std::print( "(Texture2D)'{}'", texture ? texture->mPath.string() : "null" );
        }
        else
            std::print( "(unknown)" );
    }
}


void DrawFieldsAdding( Table& table )
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
    constexpr string_view types = "Int\0Float\0String\0Bool\0Table\0"sv;
    static enum class Types { Int, Float, String, Bool, Table } selectedType;
    ImGui::Combo( "##type", (int*)&selectedType, types.data() );

    if ( addValue )
    {
        if ( fieldName.empty() )
            return;

        switch ( selectedType )
        {
            case Types::Int:
                if ( isArray )
                    table[newId] = 0;
                else
                    table[fieldName] = 0;
                break;
            case Types::Float:
                if ( isArray )
                    table[newId] = 0.0f;
                else
                    table[fieldName] = 0.0f;
                break;
            case Types::String:
                if ( isArray )
                    table[newId] = ""s;
                else
                    table[fieldName] = ""s;
                break;
            case Types::Bool:
                if ( isArray )
                    table[newId] = false;
                else
                    table[fieldName] = false;
                break;
            case Types::Table:
            {
                sol::state_view lua = table.lua_state();
                table[fieldName] = lua.create_table();
                break;
            }
        }
    }
}

opt<Any> DrawAnyValue( const string& name, Any any )
{
    ImGui::SetNextItemWidth( 100.0f );
    if ( any.is<int>() )
    {
        auto value = any.as<int>();
        ImGui::DragInt( name.c_str(), &value );
        ImGui::SameLine();
        ImGui::Text( "(int)" );
        return value;
    }
    else if ( any.is<float>() )
    {
        auto value = any.as<float>();
        ImGui::DragFloat( name.c_str(), &value );
        ImGui::SameLine();
        ImGui::Text( "(float)" );
        return value;
    }
    else if ( any.is<std::string>() )
    {
        auto value = any.as<string>();
        ImGui::InputText( name.c_str(), value );
        ImGui::SameLine();
        ImGui::Text( "(string)" );
        return value;
    }
    else if ( any.is<bool>() )
    {
        auto value = any.as<bool>();
        ImGui::Checkbox( name.c_str(), &value );
        ImGui::SameLine();
        ImGui::Text( "(bool)" );
        return value;
    }
    else if ( any.is<Entity>() )
    {
        auto value = any.as<Entity>();
        int id = (int)(size_t)value;
        ImGui::DragInt( name.c_str(), &id, 1.0f, 0, INT_MAX );
        ImGui::SameLine();
        ImGui::Text( "(Entity)" );
        return Entity{}; // Entity can only be created by registry, return as-is
    }
    else if ( any.is<Ref<Texture2D>>() )
    {
        auto texture = any.as<Ref<Texture2D>>();
        string texPath = texture ? texture->mPath.string() : "null";
        ImGui::Text( "%s: %s", name.c_str(), texPath.c_str() );
        ImGui::SameLine();
        ImGui::Text( "(Texture2D)" );
        if ( texture )
        {
            ImGui::Image( (ImTextureID)(u64)texture->RendererID(), ImVec2( 64, 64 ) );
        }
        return texture;
    }
    else if ( any.is<Table>() and IsArray( any.as<Table>() ) )
    {
        int i = 0;
        auto table = any.as<Table>();
        if ( ImGui::TreeNodeEx( "##array", TABLE_FLAGS, "%s (array)", name.c_str() ) )
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

                auto name = std::to_string( k.as<int>() );
                table[k] = DrawAnyValue( name, v );

                ImGui::Separator();
                ImGui::PopID();
                i++;
            }
            DrawFieldsAdding( table );
            ImGui::TreePop();
        }
        return table;
    }
    else if ( any.is<Table>() )
    {
        int i = 0;
        auto table = any.as<Table>();
        if ( ImGui::TreeNodeEx( "##table", TABLE_FLAGS, "%s (table)", name.c_str() ) )
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

                auto name = k.as<string>();
                table[k] = DrawAnyValue( name, v );

                ImGui::Separator();
                ImGui::PopID();
                i++;
            }
            DrawFieldsAdding( table );
            ImGui::TreePop();
        }
        return table;
    }
    throw std::runtime_error( "Invalid Any value type" );
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
    else if ( v.is<Table>() and IsArray( v.as<Table>() ) )
    {
        json j = json::array();
        auto table = v.as<Table>();
        for ( auto& [k, v] : table )
            j.push_back( SaveAnyValue( v ) );
        return j;
    }
    else if ( v.is<Table>() )
    {
        json j = json::object();
        auto table = v.as<Table>();
        for ( auto& [k, v] : table )
            j[k.as<string>()] = SaveAnyValue( v );
        return j;
    }
    else
    {
        PrintAnyValue( v );
        throw std::runtime_error( "Value of not supported type" );
    }
}


Any LoadAnyValue( ScriptingEngine& scripting, const json& j )
{
    auto& lua = *scripting.mLua;
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
            // Note: Entity loaded this way won't be valid in the registry
            // It's just storing the ID for reference
            return Any( lua, Entity{} );
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
        auto table = scripting.CreateTable();
        int i = 1;
        for ( const auto& v : j )
            table[i++] = LoadAnyValue( scripting, v );
        return table;
    }
    else if ( j.is_object() )
    {
        auto table = scripting.CreateTable();
        for ( const auto& [k, v] : j.items() )
            table[k] = LoadAnyValue( scripting, v );
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

}

