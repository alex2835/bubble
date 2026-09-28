#include "engine/pch/pch.hpp"
#include "engine/scripting/reflection_lua.hpp"
#include "engine/renderer/transform.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/set.hpp"
#include "engine/types/map.hpp"

namespace bubble
{
namespace
{
string EnumName( const entt::meta_any& value )
{
    for ( const auto [id, constant] : value.type().data() )
        if ( constant.get( {} ) == value )
            return constant.name();
    return std::to_string( value.allow_cast<i64>().cast<i64>() );
}

string LuaTypeName( const sol::object& value )
{
    return sol::type_name( value.lua_state(), value.get_type() );
}

[[noreturn]] void Expected( const char* what, const entt::meta_type& type, const sol::object& got )
{
    throw std::runtime_error( std::format( "a {} takes {}, not a {}", TypeName( type ), what, LuaTypeName( got ) ) );
}

struct LuaValue
{
    ToLuaFn mTo = nullptr;
    FromLuaFn mFrom = nullptr;
};

hash_map<entt::id_type, LuaValue>& LuaValues()
{
    static hash_map<entt::id_type, LuaValue> values;
    return values;
}

const LuaValue* FindLuaValue( const entt::meta_type& type )
{
    const auto it = LuaValues().find( type.info().hash() );
    return it != LuaValues().end() ? &it->second : nullptr;
}

template <typename Number>
entt::meta_any NumberFrom( const sol::object& value, const entt::meta_type& type )
{
    if ( value.get_type() != sol::type::number )
        Expected( "a number", type, value );
    return static_cast<Number>( value.as<double>() );
}
}

sol::object ToLua( sol::state_view lua, const entt::meta_any& value )
{
    if ( not value )
        return sol::make_object( lua, sol::lua_nil );
    if ( auto* v = value.try_cast<f32>() ) return sol::make_object( lua, *v );
    if ( auto* v = value.try_cast<f64>() ) return sol::make_object( lua, *v );
    if ( auto* v = value.try_cast<i32>() ) return sol::make_object( lua, *v );
    if ( auto* v = value.try_cast<u32>() ) return sol::make_object( lua, *v );
    if ( auto* v = value.try_cast<i64>() ) return sol::make_object( lua, *v );
    if ( auto* v = value.try_cast<u64>() ) return sol::make_object( lua, *v );
    if ( auto* v = value.try_cast<bool>() ) return sol::make_object( lua, *v );
    if ( auto* v = value.try_cast<string>() ) return sol::make_object( lua, *v );
    if ( auto* v = value.try_cast<vec2>() ) return sol::make_object( lua, *v );
    if ( auto* v = value.try_cast<vec3>() ) return sol::make_object( lua, *v );
    if ( auto* v = value.try_cast<vec4>() ) return sol::make_object( lua, *v );
    if ( auto* v = value.try_cast<quat>() ) return sol::make_object( lua, Transform::ToEuler( *v ) );

    const entt::meta_type type = value.type();
    if ( const LuaValue* registered = FindLuaValue( type ) )
        return registered->mTo( lua, value );
    if ( type.is_enum() )
        return sol::make_object( lua, EnumName( value ) );
    if ( type.is_sequence_container() )
    {
        sol::table table = lua.create_table();
        int i = 1;
        for ( const entt::meta_any element : value.as_sequence_container() )
            table[i++] = ToLua( lua, element );
        return table;
    }
    if ( IsDescribed( type ) )
    {
        sol::table table = lua.create_table();
        for ( const auto [id, field] : type.data() )
            table[field.name()] = ToLua( lua, field.get( value ) );
        return table;
    }
    throw std::runtime_error( std::format( "a {} has no Lua value", TypeName( type ) ) );
}

entt::meta_any FromLua( const sol::object& value, const entt::meta_type& type )
{
    if ( const LuaValue* registered = FindLuaValue( type ) )
        return registered->mFrom( value );
    if ( type == entt::resolve<f32>() ) return NumberFrom<f32>( value, type );
    if ( type == entt::resolve<f64>() ) return NumberFrom<f64>( value, type );
    if ( type == entt::resolve<i32>() ) return NumberFrom<i32>( value, type );
    if ( type == entt::resolve<u32>() ) return NumberFrom<u32>( value, type );
    if ( type == entt::resolve<i64>() ) return NumberFrom<i64>( value, type );
    if ( type == entt::resolve<u64>() ) return NumberFrom<u64>( value, type );
    if ( type == entt::resolve<bool>() )
    {
        if ( not value.is<bool>() )
            Expected( "true or false", type, value );
        return value.as<bool>();
    }
    if ( type == entt::resolve<string>() )
    {
        if ( value.get_type() != sol::type::string )
            Expected( "a string", type, value );
        return value.as<string>();
    }
    if ( type == entt::resolve<vec2>() )
    {
        if ( not value.is<vec2>() )
            Expected( "a vec2", type, value );
        return value.as<vec2>();
    }
    if ( type == entt::resolve<vec3>() )
    {
        if ( not value.is<vec3>() )
            Expected( "a vec3", type, value );
        return value.as<vec3>();
    }
    if ( type == entt::resolve<vec4>() )
    {
        if ( not value.is<vec4>() )
            Expected( "a vec4", type, value );
        return value.as<vec4>();
    }
    if ( type == entt::resolve<quat>() )
    {
        if ( not value.is<vec3>() )
            Expected( "Euler radians as a vec3", type, value );
        return Transform::FromEuler( value.as<vec3>() );
    }

    if ( type.is_enum() )
    {
        if ( value.get_type() == sol::type::string )
        {
            const string name = value.as<string>();
            const entt::meta_data constant = type.data( entt::hashed_string::value( name.data(), name.size() ) );
            if ( not constant )
            {
                string values;
                for ( const auto [id, c] : type.data() )
                    values += values.empty() ? c.name() : std::format( ", {}", c.name() );
                throw std::runtime_error( std::format( "{} has no value '{}'. Its values: {}", TypeName( type ), name, values ) );
            }
            return constant.get( {} );
        }
        entt::meta_any number = NumberFrom<i64>( value, type );
        if ( not number.allow_cast( type ) )
            Expected( "one of its value names", type, value );
        return number;
    }

    if ( type.is_sequence_container() or IsDescribed( type ) )
    {
        if ( value.get_type() != sol::type::table )
            Expected( "a table", type, value );
        const sol::table table = value.as<sol::table>();
        entt::meta_any made = type.construct();
        if ( not made )
            throw std::runtime_error( std::format( "a {} cannot be made without arguments", TypeName( type ) ) );
        if ( type.is_sequence_container() )
        {
            auto container = made.as_sequence_container();
            const size_t size = table.size();
            if ( not container.resize( size ) )
                throw std::runtime_error( std::format( "a {} cannot hold {} elements", TypeName( type ), size ) );
            for ( size_t i = 0; i < size; i++ )
            {
                entt::meta_any element = container[i];
                element.assign( FromLua( table[i + 1], container.value_type() ) );
            }
        }
        else
        {
            for ( const auto [id, field] : type.data() )
            {
                const sol::object given = table[field.name()];
                if ( given.valid() and given.get_type() != sol::type::lua_nil )
                    field.set( made, FromLua( given, field.type() ) );
            }
        }
        return made;
    }

    throw std::runtime_error( std::format( "a {} cannot be set from Lua", TypeName( type ) ) );
}

void RegisterLuaValue( const entt::meta_type& type, ToLuaFn to, FromLuaFn from )
{
    LuaValues()[type.info().hash()] = { to, from };
}

void BindReflectedEnum( sol::state& lua, const entt::meta_type& type )
{
    const string name = TypeName( type );
    if ( lua[name].valid() )
        return;
    sol::table values = lua.create_named_table( name );
    for ( const auto [id, constant] : type.data() )
        values[constant.name()] = constant.name();
}

}
