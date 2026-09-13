#include "engine/pch/pch.hpp"
#include "engine/serialization/any_serialization.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/loader/loader.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
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

} // namespace bubble
