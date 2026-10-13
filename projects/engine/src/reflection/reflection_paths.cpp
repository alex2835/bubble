// Reflection by path, and the bridge between a field's own type and Value.
#include "bubble/reflection/reflection.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
// By C++ type, as in reflection.cpp: a value made without our context has
// the same type from another one.
template <typename T>
bool Is( const MetaType& type )
{
    return type.info() == entt::type_id<T>();
}

bool Whole( f64 number )
{
    constexpr f64 cExact = 9007199254740992.0; // 2^53
    return number >= -cExact and number <= cExact and number == static_cast<f64>( static_cast<i64>( number ) );
}

string ValueFields( const Value::Record& record )
{
    string names;
    for ( const ValueField& field : record )
        names += ( names.empty() ? "" : ", " ) + field.mName.ToString();
    return names;
}

// One step into a Value - an element of a list, a field of a record - as a
// reference: const when the Value is.
template <typename V>
expected<MetaAny, string> IntoValue( const entt::meta_ctx& context,
                                     V& value,
                                     const FieldPath::Step& step,
                                     const string& where )
{
    if ( const size_t* position = get_if<size_t>( &step ) )
    {
        auto list = value.template As<Value::List>();
        if ( not list )
            return unexpected( format( "{}: not a list, it is {}", where, value.Describe() ) );
        if ( *position > list->size() )
            return unexpected( format( "{}: no element {}, there are {}", where, *position, list->size() ) );
        return entt::forward_as_meta( context, ( *list )[*position - 1] );
    }
    const Name name = get<Name>( step );
    auto record = value.template As<Value::Record>();
    if ( not record )
        return unexpected( format( "{}: not a record, it is {}", where, value.Describe() ) );
    for ( auto& field : *record )
        if ( field.mName == name )
            return entt::forward_as_meta( context, field.mValue );
    return unexpected( format( "{}: no field {} (fields: {})", where, name.ToString(), ValueFields( *record ) ) );
}
}

expected<MetaAny, string> Reflection::StepInto( MetaAny& current,
                                                const FieldPath::Step& step,
                                                const string& where ) const
{
    if ( Value* value = current.try_cast<Value>() )
        return IntoValue( mContext, *value, step, where );
    if ( const Value* value = static_cast<const MetaAny&>( current ).try_cast<const Value>() )
        return IntoValue( mContext, *value, step, where );

    if ( const size_t* position = get_if<size_t>( &step ) )
    {
        auto sequence = current.as_sequence_container();
        if ( not sequence )
            return unexpected( format( "{}: not a list, it is a {}", where, NameOf( current.type() ) ) );
        if ( *position > sequence.size() )
            return unexpected( format( "{}: no element {}, there are {}", where, *position, sequence.size() ) );
        return sequence[*position - 1];
    }
    const Name name = get<Name>( step );
    const MetaField field = current.type().data( name.Hash() );
    if ( not field )
        return unexpected( format( "{}: {} has no field {} (fields: {})", where, NameOf( current.type() ),
                                   name.ToString(), FieldNames( current.type() ) ) );
    return field.get( current );
}

expected<void, string> Reflection::Assign( MetaAny& current,
                                           const FieldPath::Step& step,
                                           MetaAny value,
                                           const string& where )
{
    // Inside a Value: the slot keeps its kind.
    if ( Value* parent = current.try_cast<Value>() )
    {
        auto slot = IntoValue( mContext, *parent, step, where );
        if ( not slot )
            return unexpected( std::move( slot.error() ) );
        auto given = ToValueAt( value, where );
        if ( not given )
            return unexpected( std::move( given.error() ) );
        Value& target = slot->cast<Value&>();
        if ( given->Kind() != target.Kind() )
            return unexpected( format( "{}: {} expected, got {}", where, target.Kind(), given->Describe() ) );
        target = std::move( *given );
        return {};
    }
    if ( current.base().policy() == entt::any_policy::cref )
        return unexpected( format( "{} cannot be set here: the object is const", where ) );

    // The type the slot takes, and a Value made into it.
    const auto fit = [&]( const MetaType& type ) -> expected<void, string> {
        if ( const Value* given = value.try_cast<const Value>(); given and not Is<Value>( type ) )
        {
            auto made = FromValueAt( *given, type, where );
            if ( not made )
                return unexpected( std::move( made.error() ) );
            value = std::move( *made );
        }
        if ( not value.allow_cast( type ) )
            return unexpected( format( "{}: {} expected, got {}", where, NameOf( type ), NameOf( value.type() ) ) );
        return {};
    };

    if ( const size_t* position = get_if<size_t>( &step ) )
    {
        auto sequence = current.as_sequence_container();
        if ( not sequence )
            return unexpected( format( "{}: not a list, it is a {}", where, NameOf( current.type() ) ) );
        if ( *position > sequence.size() )
            return unexpected( format( "{}: no element {}, there are {}", where, *position, sequence.size() ) );
        if ( auto fits = fit( sequence.value_type() ); not fits )
            return fits;
        if ( not sequence[*position - 1].assign( value ) )
            return unexpected( format( "{} cannot be set here", where ) );
        return {};
    }

    const Name name = get<Name>( step );
    const MetaField field = current.type().data( name.Hash() );
    if ( not field )
        return unexpected( format( "{}: {} has no field {} (fields: {})", where, NameOf( current.type() ),
                                   name.ToString(), FieldNames( current.type() ) ) );
    if ( auto fits = fit( field.type() ); not fits )
        return fits;
    if ( not field.set( current, value ) )
        return unexpected( format( "{} cannot be set here", where ) );
    NotifyChanged( current, name );
    return {};
}

expected<void, string> Reflection::SetAt( MetaAny& current,
                                          const FieldPath& path,
                                          size_t at,
                                          MetaAny& value,
                                          const string& root )
{
    const FieldPath::Step& step = path.Steps()[at];
    const string where = format( "{}.{}", root, path.Prefix( at + 1 ) );
    if ( at + 1 == path.Steps().size() )
        return Assign( current, step, std::move( value ), where );

    auto child = StepInto( current, step, where );
    if ( not child )
        return unexpected( std::move( child.error() ) );
    if ( auto done = SetAt( *child, path, at + 1, value, root ); not done )
        return done;

    // A copy - a Property's value - goes back through its setter, which
    // reports the field; a reference changed in place, so report it here.
    if ( child->base().policy() != entt::any_policy::ref )
        return Assign( current, step, std::move( *child ), where );
    if ( const Name* name = get_if<Name>( &step ); name and not current.try_cast<Value>() )
        NotifyChanged( current, *name );
    return {};
}

expected<MetaAny, string> Reflection::Walk( MetaAny current, const FieldPath& path ) const
{
    const string root = NameOf( current.type() );
    for ( size_t at = 0; at < path.Steps().size(); ++at )
    {
        auto next = StepInto( current, path.Steps()[at], format( "{}.{}", root, path.Prefix( at + 1 ) ) );
        if ( not next )
            return next;
        current = std::move( *next );
    }
    return current;
}

expected<MetaAny, string> Reflection::GetField( const MetaAny& object, const FieldPath& path ) const
{
    return Walk( object.as_ref(), path );
}

expected<MetaAny, string> Reflection::GetField( MetaAny& object, const FieldPath& path ) const
{
    return Walk( object.as_ref(), path );
}

expected<void, string> Reflection::SetField( MetaAny& object, const FieldPath& path, MetaAny value )
{
    if ( path.Steps().empty() )
        return unexpected( "an empty field path"s );
    MetaAny current = object.as_ref();
    return SetAt( current, path, 0, value, NameOf( object.type() ) );
}

// ---- Value -----------------------------------------------------------------

expected<Value, string> Reflection::ToValue( const MetaAny& value ) const
{
    return ToValueAt( value, NameOf( value.type() ) );
}

expected<MetaAny, string> Reflection::FromValue( const Value& value, const MetaType& type ) const
{
    return FromValueAt( value, type, NameOf( type ) );
}

expected<Value, string> Reflection::ToValueAt( const MetaAny& value, const string& where ) const
{
    const MetaType type = value.type();
    if ( const Value* given = value.try_cast<const Value>() )
        return *given;
    if ( Is<bool>( type ) )
        return Value( value.cast<bool>() );
    if ( type.is_arithmetic() )
        return Value( value.allow_cast<f64>().cast<f64>() );
    if ( Is<string>( type ) )
    {
        auto text = Text::From( value.cast<const string&>() );
        if ( not text )
            return unexpected( format( "{}: {}", where, text.error() ) );
        return Value( std::move( *text ) );
    }
    if ( Is<Text>( type ) )
        return Value( value.cast<const Text&>() );
    if ( Is<vec3>( type ) )
        return Value( value.cast<vec3>() );
    if ( Is<Color>( type ) )
        return Value( value.cast<Color>() );
    if ( Is<Name>( type ) )
        return Value( value.cast<Name>() );
    if ( Is<AssetId>( type ) )
        return Value( value.cast<AssetId>() );
    if ( Is<EntityPath>( type ) )
        return Value( value.cast<const EntityPath&>() );

    if ( auto sequence = value.as_sequence_container() )
    {
        Value::List list;
        for ( size_t i = 0; i < sequence.size(); ++i )
        {
            auto element = ToValueAt( sequence[i], format( "{}[{}]", where, i + 1 ) );
            if ( not element )
                return element;
            list.push_back( std::move( *element ) );
        }
        return Value( std::move( list ) );
    }
    if ( mDescribed.contains( Name::FromHash( type.id() ) ) )
    {
        Value::Record record;
        for ( const auto [id, field] : type.data() )
        {
            if ( InfoOf( field ).Has( FieldInfo::Transient ) )
                continue;
            const Name name = Name::FromHash( id );
            auto fieldValue = ToValueAt( field.get( value ), format( "{}.{}", where, name.ToString() ) );
            if ( not fieldValue )
                return fieldValue;
            record.push_back( ValueField{ name, std::move( *fieldValue ) } );
        }
        return Value( std::move( record ) );
    }
    return unexpected( format( "{}: a {} cannot be kept as a value", where, NameOf( type ) ) );
}

expected<MetaAny, string> Reflection::FromValueAt( const Value& value, const MetaType& type, const string& where ) const
{
    const auto mismatch = [&] {
        return unexpected( format( "{}: {} expected, got {}", where, NameOf( type ), value.Describe() ) );
    };
    // The same C++ type on both sides.
    const auto exact = [&]<typename T>( OptRef<const T> given ) -> expected<MetaAny, string> {
        if ( not given )
            return mismatch();
        return MetaAny( mContext, in_place_type<T>, *given );
    };

    if ( Is<Value>( type ) )
        return MetaAny( mContext, in_place_type<Value>, value );
    if ( Is<bool>( type ) )
        return exact( value.As<bool>() );
    if ( type.is_arithmetic() )
    {
        const auto number = value.As<f64>();
        if ( not number or ( type.is_integral() and not Whole( *number ) ) )
            return mismatch();
        MetaAny converted( mContext, in_place_type<f64>, *number );
        if ( not converted.allow_cast( type ) )
            return mismatch();
        return converted;
    }
    if ( Is<string>( type ) )
    {
        const auto text = value.As<Text>();
        if ( not text )
            return mismatch();
        return MetaAny( mContext, in_place_type<string>, text->String() );
    }
    if ( Is<Text>( type ) )
        return exact( value.As<Text>() );
    if ( Is<vec3>( type ) )
        return exact( value.As<vec3>() );
    if ( Is<Color>( type ) )
        return exact( value.As<Color>() );
    if ( Is<Name>( type ) )
        return exact( value.As<Name>() );
    if ( Is<AssetId>( type ) )
        return exact( value.As<AssetId>() );
    if ( Is<EntityPath>( type ) )
        return exact( value.As<EntityPath>() );

    MetaAny made = type.construct();
    if ( not made )
        return unexpected( format( "{}: a {} cannot be made from a value", where, NameOf( type ) ) );
    if ( auto sequence = made.as_sequence_container() )
    {
        const auto list = value.As<Value::List>();
        if ( not list )
            return mismatch();
        for ( size_t i = 0; i < list->size(); ++i )
        {
            auto element = FromValueAt( ( *list )[i], sequence.value_type(), format( "{}[{}]", where, i + 1 ) );
            if ( not element )
                return element;
            if ( not sequence.insert( sequence.end(), std::move( *element ) ) )
                return unexpected( format( "{}[{}] cannot be added", where, i + 1 ) );
        }
        return made;
    }
    if ( mDescribed.contains( Name::FromHash( type.id() ) ) )
    {
        const auto record = value.As<Value::Record>();
        if ( not record )
            return mismatch();
        for ( const ValueField& field : *record )
        {
            const string fieldWhere = format( "{}.{}", where, field.mName.ToString() );
            const MetaField data = type.data( field.mName.Hash() );
            if ( not data )
                return unexpected( format( "{}: {} has no field {} (fields: {})", fieldWhere, NameOf( type ),
                                           field.mName.ToString(), FieldNames( type ) ) );
            auto fieldValue = FromValueAt( field.mValue, data.type(), fieldWhere );
            if ( not fieldValue )
                return fieldValue;
            if ( not data.set( made, *fieldValue ) )
                return unexpected( format( "{} cannot be set", fieldWhere ) );
        }
        return made;
    }
    return unexpected( format( "{}: a {} cannot be made from a value", where, NameOf( type ) ) );
}
}
