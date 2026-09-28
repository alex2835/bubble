#include "engine/pch/pch.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/map.hpp"
#include <nlohmann/json.hpp>
#include <deque>

namespace bubble
{
namespace reflection_detail
{
const char* Intern( string name )
{
    // A deque never moves what it holds.
    static std::deque<string> names;
    return names.emplace_back( std::move( name ) ).c_str();
}
}

namespace
{
entt::id_type Hash( string_view text )
{
    return entt::hashed_string::value( text.data(), text.size() );
}

/// Codecs

struct Codec
{
    ToJsonFn mTo = nullptr;
    FromJsonFn mFrom = nullptr;
};

template <typename T>
void Plain( hash_map<entt::id_type, Codec>& codecs )
{
    codecs[entt::type_hash<T>::value()] = {
        []( const entt::meta_any& v, const ReflectionContext& ) -> json { return v.cast<const T&>(); },
        []( const json& j, const ReflectionContext& ) -> entt::meta_any { return j.get<T>(); } };
}

template <typename T>
void Vector( hash_map<entt::id_type, Codec>& codecs )
{
    codecs[entt::type_hash<T>::value()] = {
        []( const entt::meta_any& v, const ReflectionContext& ) -> json
        {
            const T& vec = v.cast<const T&>();
            json j = json::array();
            for ( int i = 0; i < T::length(); i++ )
                j.push_back( vec[i] );
            return j;
        },
        []( const json& j, const ReflectionContext& ) -> entt::meta_any
        {
            if ( not j.is_array() or j.size() != (size_t)T::length() )
                throw std::runtime_error( std::format( "{} is not {} numbers", j.dump(), T::length() ) );
            T vec;
            for ( int i = 0; i < T::length(); i++ )
                vec[i] = j[i].get<typename T::value_type>();
            return vec;
        } };
}

hash_map<entt::id_type, Codec>& Codecs()
{
    static hash_map<entt::id_type, Codec> codecs = []
    {
        hash_map<entt::id_type, Codec> made;
        Plain<bool>( made );
        Plain<i32>( made );
        Plain<u32>( made );
        Plain<i64>( made );
        Plain<u64>( made );
        Plain<f32>( made );
        Plain<f64>( made );
        Plain<string>( made );
        Vector<vec2>( made );
        Vector<vec3>( made );
        Vector<vec4>( made );
        Vector<ivec2>( made );
        Vector<ivec3>( made );
        // x, y, z, w - the order a level file has always had.
        made[entt::type_hash<quat>::value()] = {
            []( const entt::meta_any& v, const ReflectionContext& ) -> json
            {
                const quat& q = v.cast<const quat&>();
                return json{ q.x, q.y, q.z, q.w };
            },
            []( const json& j, const ReflectionContext& ) -> entt::meta_any
            {
                if ( not j.is_array() or j.size() != 4 )
                    throw std::runtime_error( std::format( "{} is not a quaternion [x, y, z, w]", j.dump() ) );
                return glm::normalize( quat( j[3].get<f32>(), j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>() ) );
            } };
        return made;
    }();
    return codecs;
}

const Codec* FindCodec( const entt::meta_type& type )
{
    const auto& codecs = Codecs();
    const auto it = codecs.find( type.info().hash() );
    return it != codecs.end() ? &it->second : nullptr;
}

// Whether the value is its own copy rather than a reference into something.
bool Owns( const entt::meta_any& value )
{
    return value.base().owner();
}

void CallOnChanged( entt::meta_any& owner )
{
    if ( const entt::meta_func hook = owner.type().func( cOnChangedId ) )
        hook.invoke( owner );
}

/// Paths

struct Step
{
    string_view mName;
    std::optional<size_t> mIndex;
};

vector<Step> ParsePath( string_view path )
{
    vector<Step> steps;
    size_t start = 0;
    while ( true )
    {
        const size_t dot = path.find( '.', start );
        string_view part = path.substr( start, dot == string_view::npos ? string_view::npos : dot - start );
        Step step;
        if ( const size_t open = part.find( '[' ); open != string_view::npos )
        {
            size_t index = 0;
            const string_view digits = part.substr( open + 1, part.size() - open - 2 );
            const auto [end, error] = std::from_chars( digits.data(), digits.data() + digits.size(), index );
            if ( part.back() != ']' or digits.empty() or error != std::errc() or end != digits.data() + digits.size() )
                throw std::runtime_error( std::format( "'{}': '{}' is not name[index]", path, part ) );
            step.mIndex = index;
            part = part.substr( 0, open );
        }
        if ( part.empty() )
            throw std::runtime_error( std::format( "'{}': a name is missing", path ) );
        step.mName = part;
        steps.push_back( step );
        if ( dot == string_view::npos )
            return steps;
        start = dot + 1;
    }
}

string FieldNames( const entt::meta_type& type )
{
    string names;
    for ( const auto [id, field] : type.data() )
        names += names.empty() ? field.name() : std::format( ", {}", field.name() );
    return names.empty() ? "none" : names;
}

entt::meta_data FieldOf( const entt::meta_any& owner, string_view name, string_view path )
{
    const entt::meta_type type = owner.type();
    const entt::meta_data field = type.data( Hash( name ) );
    if ( not field )
        throw std::runtime_error( std::format( "'{}': {} has no field '{}'. Its fields: {}", path, TypeName( type ), name,
                                               FieldNames( type ) ) );
    return field;
}

// The element of a sequence, by reference into it.
entt::meta_any ElementOf( entt::meta_any& sequence, const Step& step, string_view path )
{
    auto container = sequence.as_sequence_container();
    if ( not container )
        throw std::runtime_error( std::format( "'{}': {} is a {}, not a sequence", path, step.mName, TypeName( sequence.type() ) ) );
    if ( *step.mIndex >= container.size() )
        throw std::runtime_error( std::format( "'{}': {} has {} elements", path, step.mName, container.size() ) );
    return container[*step.mIndex];
}

void SetAt( entt::meta_any& owner, std::span<const Step> steps, entt::meta_any& value, string_view path )
{
    const Step& step = steps.front();
    const entt::meta_data field = FieldOf( owner, step.mName, path );
    if ( const FieldInfo& info = FieldInfoOf( field ); info.Has( FieldInfo::ReadOnly ) )
        throw std::runtime_error( info.mTooltip ? std::format( "'{}': {} is read only - {}", path, step.mName, info.mTooltip )
                                                : std::format( "'{}': {} is read only", path, step.mName ) );

    if ( steps.size() == 1 and not step.mIndex )
    {
        if ( not field.set( owner, value ) )
            throw std::runtime_error( std::format( "'{}': a {} cannot be set from a {}", path, TypeName( field.type() ),
                                                   TypeName( value.type() ) ) );
    }
    else
    {
        // A reference into the owner for a Field, a copy for a Property.
        entt::meta_any child = field.get( owner );
        if ( step.mIndex )
        {
            entt::meta_any element = ElementOf( child, step, path );
            if ( steps.size() == 1 )
            {
                if ( not element.assign( value ) )
                    throw std::runtime_error( std::format( "'{}': an element of {} cannot be set from a {}", path,
                                                           step.mName, TypeName( value.type() ) ) );
            }
            else
                SetAt( element, steps.subspan( 1 ), value, path );
        }
        else
            SetAt( child, steps.subspan( 1 ), value, path );

        if ( Owns( child ) and not field.set( owner, child ) )
            throw std::runtime_error( std::format( "'{}': {} cannot be set back", path, step.mName ) );
    }
    CallOnChanged( owner );
}
}

/// Descriptions

void NotifyChanged( entt::meta_any& object )
{
    entt::meta_any target = object.as_ref();
    CallOnChanged( target );
}

const FieldInfo& FieldInfoOf( const entt::meta_data& field )
{
    static const FieldInfo none;
    const FieldInfo* info = field.custom();
    return info ? *info : none;
}

const TypeInfo& TypeInfoOf( const entt::meta_type& type )
{
    static const TypeInfo none;
    const TypeInfo* info = type.custom();
    return info ? *info : none;
}

string TypeName( const entt::meta_type& type )
{
    if ( not type )
        return "nothing";
    if ( const char* name = type.name() )
        return name;
    return string( type.info().name() );
}

/// Fields by path

entt::meta_any GetField( entt::meta_any& object, string_view path )
{
    entt::meta_any current = object.as_ref();
    // Copies made on the way, through a Property, kept alive until the end.
    vector<entt::meta_any> held;
    for ( const Step& step : ParsePath( path ) )
    {
        entt::meta_any next = FieldOf( current, step.mName, path ).get( current );
        if ( step.mIndex )
        {
            if ( Owns( next ) )
                held.push_back( std::move( next ) ), next = held.back().as_ref();
            next = ElementOf( next, step, path );
        }
        if ( Owns( next ) )
            held.push_back( std::move( next ) ), next = held.back().as_ref();
        current = std::move( next );
    }
    // Past a Property the value lives in a copy that goes with this call.
    if ( not held.empty() )
    {
        entt::meta_any copy = std::as_const( current );
        return copy;
    }
    return current;
}

void SetField( entt::meta_any& object, string_view path, entt::meta_any value )
{
    const vector<Step> steps = ParsePath( path );
    entt::meta_any target = object.as_ref();
    SetAt( target, steps, value, path );
}

void SetField( entt::meta_any& object, string_view path, const json& value, const ReflectionContext& ctx )
{
    const entt::meta_type type = GetField( object, path ).type();
    entt::meta_any converted;
    try
    {
        converted = FromJson( value, type, ctx );
    }
    catch ( const std::exception& e )
    {
        throw std::runtime_error( std::format( "'{}': {}", path, e.what() ) );
    }
    SetField( object, path, std::move( converted ) );
}

/// JSON

bool IsDescribed( const entt::meta_type& type )
{
    // A TypeBuilder names the type; a type meta only met as a field's does not
    // have a name. Enums are named too, but are values, not objects.
    return not type.is_enum() and
           ( type.data().begin() != type.data().end() or ( type.name() != nullptr and not FindCodec( type ) ) );
}

json ToJson( const entt::meta_any& value, const ReflectionContext& ctx )
{
    const entt::meta_type type = value.type();
    if ( not type )
        throw std::runtime_error( "no JSON for an empty value" );
    if ( const Codec* codec = FindCodec( type ) )
        return codec->mTo( value, ctx );

    if ( type.is_enum() )
    {
        for ( const auto [id, constant] : type.data() )
            if ( constant.get( {} ) == value )
                return constant.name();
        return value.allow_cast<i64>().cast<i64>();
    }

    if ( type.is_sequence_container() )
    {
        json array = json::array();
        for ( const entt::meta_any element : value.as_sequence_container() )
            array.push_back( ToJson( element, ctx ) );
        return array;
    }

    if ( IsDescribed( type ) )
    {
        json object = json::object();
        for ( const auto [id, field] : type.data() )
            if ( not FieldInfoOf( field ).Has( FieldInfo::Transient ) )
                object[field.name()] = ToJson( field.get( value ), ctx );
        return object;
    }

    throw std::runtime_error( std::format( "no JSON for a {}", TypeName( type ) ) );
}

entt::meta_any FromJson( const json& j, const entt::meta_type& type, const ReflectionContext& ctx )
{
    if ( const Codec* codec = FindCodec( type ) )
        return codec->mFrom( j, ctx );

    if ( type.is_enum() )
    {
        if ( j.is_string() )
        {
            const string name = j.get<string>();
            const entt::meta_data constant = type.data( Hash( name ) );
            if ( not constant )
                throw std::runtime_error( std::format( "{} has no value '{}'. Its values: {}", TypeName( type ), name,
                                                       FieldNames( type ) ) );
            return constant.get( {} );
        }
        entt::meta_any number = j.get<i64>();
        if ( not number.allow_cast( type ) )
            throw std::runtime_error( std::format( "{} is not a {}", j.dump(), TypeName( type ) ) );
        return number;
    }

    if ( type.is_sequence_container() )
    {
        if ( not j.is_array() )
            throw std::runtime_error( std::format( "{} is not an array", j.dump() ) );
        entt::meta_any made = type.construct();
        auto container = made.as_sequence_container();
        if ( not container or not container.resize( j.size() ) )
            throw std::runtime_error( std::format( "a {} cannot hold {} elements", TypeName( type ), j.size() ) );
        for ( size_t i = 0; i < j.size(); i++ )
        {
            entt::meta_any element = container[i];
            if ( not element.assign( FromJson( j[i], container.value_type(), ctx ) ) )
                throw std::runtime_error( std::format( "element {} of {} cannot be set", i, TypeName( type ) ) );
        }
        return made;
    }

    if ( IsDescribed( type ) )
    {
        entt::meta_any made = type.construct();
        if ( not made )
            throw std::runtime_error( std::format( "a {} cannot be made without arguments", TypeName( type ) ) );
        FromJson( j, made, ctx );
        return made;
    }

    throw std::runtime_error( std::format( "no {} from JSON", TypeName( type ) ) );
}

void FromJson( const json& j, entt::meta_any& object, const ReflectionContext& ctx )
{
    const entt::meta_type type = object.type();
    if ( not j.is_object() )
        throw std::runtime_error( std::format( "a {} is an object, not {}", TypeName( type ), j.dump() ) );
    entt::meta_any target = object.as_ref();
    for ( const auto [id, field] : type.data() )
    {
        const char* name = field.name();
        if ( FieldInfoOf( field ).Has( FieldInfo::Transient ) or not j.contains( name ) )
            continue;
        entt::meta_any value;
        try
        {
            value = FromJson( j[name], field.type(), ctx );
        }
        catch ( const std::exception& e )
        {
            throw std::runtime_error( std::format( "{}.{}: {}", TypeName( type ), name, e.what() ) );
        }
        if ( not field.set( target, value ) )
            throw std::runtime_error( std::format( "{}.{} cannot be set", TypeName( type ), name ) );
    }
    CallOnChanged( target );
}

void RegisterJsonCodec( const entt::meta_type& type, ToJsonFn to, FromJsonFn from )
{
    Codecs()[type.info().hash()] = { to, from };
}

}
