#include "bubble/reflection/reflection.hpp"
#include "bubble/core/text.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/math.hpp"

namespace bubble
{
namespace detail
{
Name DeclareName( string_view name )
{
    auto declaredMaybe = Name::TryFrom( name );
    if ( not declaredMaybe )
        throw logic_error( declaredMaybe.error() );
    return *declaredMaybe;
}
}

namespace
{
const FieldInfo cNoFieldInfo{};
const TypeInfo cNoTypeInfo{};

// By C++ type, not by meta type: a value made without our context - a
// MetaAny( 1.0f ) - has the same type from another one.
template <typename T>
bool Is( const MetaType& type )
{
    return type.info() == entt::type_id<T>();
}

string_view Arguments( size_t count )
{
    return count == 1 ? "argument" : "arguments";
}
}

Reflection::Reflection() = default;
Reflection::~Reflection() = default;

opt<MetaType> Reflection::TryFind( Name type ) const
{
    const MetaType found = entt::resolve( mContext, type.Hash() );
    if ( not found or not mDescribed.contains( type ) )
        return nullopt;
    return found;
}

string Reflection::NameOf( const MetaType& type ) const
{
    if ( not type )
        return "nothing";
    if ( mDescribed.contains( Name::FromHash( type.id() ) ) )
        return Name::FromHash( type.id() ).ToString();
    if ( Is<bool>( type ) )
        return "bool";
    if ( type.is_arithmetic() )
        return type.is_integral() ? "integer" : "number";
    if ( Is<string>( type ) or Is<Text>( type ) )
        return "text";
    if ( Is<Name>( type ) )
        return "name";
    if ( Is<vec3>( type ) )
        return "vec3";
    return string( type.info().name() );
}

void Reflection::ForEachField( const MetaType& type, const function<void( Name, const MetaField& )>& fn ) const
{
    for ( const auto [id, field] : type.data() )
        fn( Name::FromHash( id ), field );
}

const FieldInfo& Reflection::InfoOf( const MetaField& field )
{
    const FieldInfo* info = field.custom();
    return info ? *info : cNoFieldInfo;
}

const TypeInfo& Reflection::InfoOf( const MetaType& type )
{
    const TypeInfo* info = type.custom();
    return info ? *info : cNoTypeInfo;
}

string Reflection::FieldNames( const MetaType& type ) const
{
    string names;
    for ( const auto [id, field] : type.data() )
        names += ( names.empty() ? "" : ", " ) + Name::FromHash( id ).ToString();
    return names;
}

string Reflection::MethodNames( const MetaType& type ) const
{
    string names;
    for ( const auto [id, method] : type.func() )
        names += ( names.empty() ? "" : ", " ) + Name::FromHash( id ).ToString();
    return names;
}

expected<MetaAny, string> Reflection::TryGet( const MetaAny& object, Name field ) const
{
    const MetaType type = object.type();
    const MetaField data = type.data( field.Hash() );
    if ( not data )
        return unexpected(
            format( "{} has no field {} (fields: {})", NameOf( type ), field.ToString(), FieldNames( type ) ) );
    return data.get( object );
}

expected<void, string> Reflection::TrySet( MetaAny& object, Name field, MetaAny value )
{
    const MetaType type = object.type();
    const MetaField data = type.data( field.Hash() );
    if ( not data )
        return unexpected(
            format( "{} has no field {} (fields: {})", NameOf( type ), field.ToString(), FieldNames( type ) ) );
    const string where = format( "{}.{}", NameOf( type ), field.ToString() );
    if ( not value.allow_cast( data.type() ) )
        return unexpected( format( "{}: {} expected, got {}", where, NameOf( data.type() ), NameOf( value.type() ) ) );
    if ( not data.set( object, value ) )
        return unexpected( format( "{} cannot be set here: the object is const", where ) );
    NotifyChanged( object, field );
    return {};
}

expected<MetaAny, string> Reflection::TryCall( MetaAny& object, Name method, span<MetaAny> args ) const
{
    const MetaType type = object.type();
    const MetaMethod function = type.func( method.Hash() );
    if ( not function )
        return unexpected(
            format( "{} has no method {} (methods: {})", NameOf( type ), method.ToString(), MethodNames( type ) ) );
    const string where = format( "{}.{}", NameOf( type ), method.ToString() );
    if ( function.arity() != args.size() )
        return unexpected(
            format( "{} takes {} {}, got {}", where, function.arity(), Arguments( function.arity() ), args.size() ) );
    for ( size_t i = 0; i < args.size(); ++i )
        if ( not args[i].allow_cast( function.arg( i ) ) )
            return unexpected( format( "{}: argument {}: {} expected, got {}", where, i + 1,
                                       NameOf( function.arg( i ) ), NameOf( args[i].type() ) ) );
    MetaAny result = function.invoke( object, args.data(), args.size() );
    if ( not result )
        return unexpected( format( "{} cannot be called here: the object is const", where ) );
    return result;
}

FieldListenerHandle Reflection::OnFieldChanged( const MetaType& type, Name field, FieldListener listener )
{
    return mListeners.Add( Listener{ type.id(), field, std::move( listener ) } );
}

void Reflection::RemoveListener( FieldListenerHandle listener )
{
    mListeners.Remove( listener );
}

void Reflection::NotifyChanged( MetaAny& owner, Name field )
{
    const u32 type = owner.type().id();
    for ( const FieldListenerHandle handle : mListeners.Handles() )
    {
        const auto listenerMaybe = mListeners.TryGet( handle );
        if ( not listenerMaybe or listenerMaybe->mType != type or listenerMaybe->mField != field )
            continue;
        // A copy: a listener may take itself off while it runs.
        const FieldListener fn = listenerMaybe->mFn;
        fn( owner );
    }
}
}
