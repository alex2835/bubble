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
expected<MetaAny, string> TryIntoValue( const entt::meta_ctx& context,
                                        V& value,
                                        const FieldPath::Step& step,
                                        const string& where )
{
    if ( const size_t* position = get_if<size_t>( &step ) )
    {
        auto listMaybe = value.template TryAs<Value::List>();
        if ( not listMaybe )
            return unexpected( format( "{}: not a list, it is {}", where, value.Describe() ) );
        if ( *position > listMaybe->size() )
            return unexpected( format( "{}: no element {}, there are {}", where, *position, listMaybe->size() ) );
        return entt::forward_as_meta( context, ( *listMaybe )[*position - 1] );
    }
    const Name name = get<Name>( step );
    auto recordMaybe = value.template TryAs<Value::Record>();
    if ( not recordMaybe )
        return unexpected( format( "{}: not a record, it is {}", where, value.Describe() ) );
    for ( auto& field : *recordMaybe )
        if ( field.mName == name )
            return entt::forward_as_meta( context, field.mValue );
    return unexpected( format( "{}: no field {} (fields: {})", where, name.ToString(), ValueFields( *recordMaybe ) ) );
}
}

expected<MetaAny, string> Reflection::TryStepInto( MetaAny& current,
                                                   const FieldPath::Step& step,
                                                   const string& where ) const
{
    if ( Value* value = current.try_cast<Value>() )
        return TryIntoValue( mContext, *value, step, where );
    if ( const Value* value = static_cast<const MetaAny&>( current ).try_cast<const Value>() )
        return TryIntoValue( mContext, *value, step, where );

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

expected<void, string> Reflection::TryAssign( MetaAny& current,
                                              const FieldPath::Step& step,
                                              MetaAny value,
                                              const string& where )
{
    // Inside a Value: the slot keeps its kind.
    if ( Value* parent = current.try_cast<Value>() )
    {
        auto slotMaybe = TryIntoValue( mContext, *parent, step, where );
        if ( not slotMaybe )
            return unexpected( std::move( slotMaybe.error() ) );
        auto givenMaybe = TryToValueAt( value, where );
        if ( not givenMaybe )
            return unexpected( std::move( givenMaybe.error() ) );
        Value& target = slotMaybe->cast<Value&>();
        if ( givenMaybe->Kind() != target.Kind() )
            return unexpected( format( "{}: {} expected, got {}", where, target.Kind(), givenMaybe->Describe() ) );
        target = std::move( *givenMaybe );
        return {};
    }
    if ( current.base().policy() == entt::any_policy::cref )
        return unexpected( format( "{} cannot be set here: the object is const", where ) );

    // The type the slot takes, and a Value made into it.
    const auto fit = [&]( const MetaType& type ) -> expected<void, string> {
        if ( const Value* given = value.try_cast<const Value>(); given and not Is<Value>( type ) )
        {
            auto madeMaybe = TryFromValueAt( *given, type, where );
            if ( not madeMaybe )
                return unexpected( std::move( madeMaybe.error() ) );
            value = std::move( *madeMaybe );
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
        if ( auto fitsMaybe = fit( sequence.value_type() ); not fitsMaybe )
            return fitsMaybe;
        if ( not sequence[*position - 1].assign( value ) )
            return unexpected( format( "{} cannot be set here", where ) );
        return {};
    }

    const Name name = get<Name>( step );
    const MetaField field = current.type().data( name.Hash() );
    if ( not field )
        return unexpected( format( "{}: {} has no field {} (fields: {})", where, NameOf( current.type() ),
                                   name.ToString(), FieldNames( current.type() ) ) );
    if ( auto fitsMaybe = fit( field.type() ); not fitsMaybe )
        return fitsMaybe;
    if ( not field.set( current, value ) )
        return unexpected( format( "{} cannot be set here", where ) );
    NotifyChanged( current, name );
    return {};
}

expected<void, string> Reflection::TrySetAt( MetaAny& current,
                                             const FieldPath& path,
                                             size_t at,
                                             MetaAny& value,
                                             const string& root )
{
    const FieldPath::Step& step = path.Steps()[at];
    const string where = format( "{}.{}", root, path.Prefix( at + 1 ) );
    if ( at + 1 == path.Steps().size() )
        return TryAssign( current, step, std::move( value ), where );

    auto childMaybe = TryStepInto( current, step, where );
    if ( not childMaybe )
        return unexpected( std::move( childMaybe.error() ) );
    if ( auto doneMaybe = TrySetAt( *childMaybe, path, at + 1, value, root ); not doneMaybe )
        return doneMaybe;

    // A copy - a Property's value - goes back through its setter, which
    // reports the field; a reference changed in place, so report it here.
    if ( childMaybe->base().policy() != entt::any_policy::ref )
        return TryAssign( current, step, std::move( *childMaybe ), where );
    if ( const Name* name = get_if<Name>( &step ); name and not current.try_cast<Value>() )
        NotifyChanged( current, *name );
    return {};
}

expected<MetaAny, string> Reflection::TryWalk( MetaAny current, const FieldPath& path ) const
{
    const string root = NameOf( current.type() );
    for ( size_t at = 0; at < path.Steps().size(); ++at )
    {
        auto nextMaybe = TryStepInto( current, path.Steps()[at], format( "{}.{}", root, path.Prefix( at + 1 ) ) );
        if ( not nextMaybe )
            return nextMaybe;
        current = std::move( *nextMaybe );
    }
    return current;
}

expected<MetaAny, string> Reflection::TryGetField( const MetaAny& object, const FieldPath& path ) const
{
    return TryWalk( object.as_ref(), path );
}

expected<MetaAny, string> Reflection::TryGetField( MetaAny& object, const FieldPath& path ) const
{
    return TryWalk( object.as_ref(), path );
}

expected<void, string> Reflection::TrySetField( MetaAny& object, const FieldPath& path, MetaAny value )
{
    if ( path.Steps().empty() )
        return unexpected( "an empty field path"s );
    MetaAny current = object.as_ref();
    return TrySetAt( current, path, 0, value, NameOf( object.type() ) );
}

// ---- Value -----------------------------------------------------------------

expected<Value, string> Reflection::TryToValue( const MetaAny& value ) const
{
    return TryToValueAt( value, NameOf( value.type() ) );
}

expected<MetaAny, string> Reflection::TryFromValue( const Value& value, const MetaType& type ) const
{
    return TryFromValueAt( value, type, NameOf( type ) );
}

expected<Value, string> Reflection::TryToValueAt( const MetaAny& value, const string& where ) const
{
    const MetaType type = value.type();
    if ( const Value* given = value.try_cast<const Value>() )
        return *given;
    if ( Is<bool>( type ) )
        return Value( value.cast<bool>() );
    if ( Is<f32>( type ) )
        return Value( WidenFloat( value.cast<f32>() ) );
    if ( type.is_arithmetic() )
        return Value( value.allow_cast<f64>().cast<f64>() );
    if ( Is<string>( type ) )
    {
        auto textMaybe = Text::TryFrom( value.cast<const string&>() );
        if ( not textMaybe )
            return unexpected( format( "{}: {}", where, textMaybe.error() ) );
        return Value( std::move( *textMaybe ) );
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
            auto elementMaybe = TryToValueAt( sequence[i], format( "{}[{}]", where, i + 1 ) );
            if ( not elementMaybe )
                return elementMaybe;
            list.push_back( std::move( *elementMaybe ) );
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
            auto fieldValueMaybe = TryToValueAt( field.get( value ), format( "{}.{}", where, name.ToString() ) );
            if ( not fieldValueMaybe )
                return fieldValueMaybe;
            record.push_back( ValueField{ name, std::move( *fieldValueMaybe ) } );
        }
        return Value( std::move( record ) );
    }
    return unexpected( format( "{}: a {} cannot be kept as a value", where, NameOf( type ) ) );
}

expected<MetaAny, string> Reflection::TryFromValueAt( const Value& value,
                                                      const MetaType& type,
                                                      const string& where ) const
{
    const auto mismatch = [&] {
        return unexpected( format( "{}: {} expected, got {}", where, NameOf( type ), value.Describe() ) );
    };
    // The same C++ type on both sides.
    const auto exact = [&]<typename T>( OptRef<const T> givenMaybe ) -> expected<MetaAny, string> {
        if ( not givenMaybe )
            return mismatch();
        return MetaAny( mContext, in_place_type<T>, *givenMaybe );
    };

    if ( Is<Value>( type ) )
        return MetaAny( mContext, in_place_type<Value>, value );
    if ( Is<bool>( type ) )
        return exact( value.TryAs<bool>() );
    if ( type.is_arithmetic() )
    {
        const auto numberMaybe = value.TryAs<f64>();
        if ( not numberMaybe or ( type.is_integral() and not Whole( *numberMaybe ) ) )
            return mismatch();
        MetaAny converted( mContext, in_place_type<f64>, *numberMaybe );
        if ( not converted.allow_cast( type ) )
            return mismatch();
        return converted;
    }
    if ( Is<string>( type ) )
    {
        const auto textMaybe = value.TryAs<Text>();
        if ( not textMaybe )
            return mismatch();
        return MetaAny( mContext, in_place_type<string>, textMaybe->String() );
    }
    if ( Is<Text>( type ) )
        return exact( value.TryAs<Text>() );
    if ( Is<vec3>( type ) )
        return exact( value.TryAs<vec3>() );
    if ( Is<Color>( type ) )
        return exact( value.TryAs<Color>() );
    if ( Is<Name>( type ) )
        return exact( value.TryAs<Name>() );
    if ( Is<AssetId>( type ) )
        return exact( value.TryAs<AssetId>() );
    if ( Is<EntityPath>( type ) )
        return exact( value.TryAs<EntityPath>() );

    MetaAny made = type.construct();
    if ( not made )
        return unexpected( format( "{}: a {} cannot be made from a value", where, NameOf( type ) ) );
    if ( auto sequence = made.as_sequence_container() )
    {
        const auto listMaybe = value.TryAs<Value::List>();
        if ( not listMaybe )
            return mismatch();
        for ( size_t i = 0; i < listMaybe->size(); ++i )
        {
            auto elementMaybe =
                TryFromValueAt( ( *listMaybe )[i], sequence.value_type(), format( "{}[{}]", where, i + 1 ) );
            if ( not elementMaybe )
                return elementMaybe;
            if ( not sequence.insert( sequence.end(), std::move( *elementMaybe ) ) )
                return unexpected( format( "{}[{}] cannot be added", where, i + 1 ) );
        }
        return made;
    }
    if ( mDescribed.contains( Name::FromHash( type.id() ) ) )
    {
        const auto recordMaybe = value.TryAs<Value::Record>();
        if ( not recordMaybe )
            return mismatch();
        for ( const ValueField& field : *recordMaybe )
        {
            const string fieldWhere = format( "{}.{}", where, field.mName.ToString() );
            const MetaField data = type.data( field.mName.Hash() );
            if ( not data )
                return unexpected( format( "{}: {} has no field {} (fields: {})", fieldWhere, NameOf( type ),
                                           field.mName.ToString(), FieldNames( type ) ) );
            auto fieldValueMaybe = TryFromValueAt( field.mValue, data.type(), fieldWhere );
            if ( not fieldValueMaybe )
                return fieldValueMaybe;
            if ( not data.set( made, *fieldValueMaybe ) )
                return unexpected( format( "{} cannot be set", fieldWhere ) );
        }
        return made;
    }
    return unexpected( format( "{}: a {} cannot be made from a value", where, NameOf( type ) ) );
}

expected<ValueType, string> Reflection::TryValueTypeOf( const MetaType& type ) const
{
    const auto kind = ValueType::Of;
    if ( Is<Value>( type ) )
        return kind( ValueKind::Any );
    if ( Is<bool>( type ) )
        return kind( ValueKind::Bool );
    if ( type.is_integral() )
        return kind( ValueKind::Integer );
    if ( type.is_arithmetic() )
        return kind( ValueKind::Number );
    if ( Is<string>( type ) or Is<Text>( type ) )
        return kind( ValueKind::Text );
    if ( Is<vec3>( type ) )
        return kind( ValueKind::Vec3 );
    if ( Is<Color>( type ) )
        return kind( ValueKind::Color );
    if ( Is<Name>( type ) )
        return kind( ValueKind::Name );
    if ( Is<AssetId>( type ) )
        return kind( ValueKind::Asset );
    if ( Is<EntityPath>( type ) )
        return kind( ValueKind::Entity );

    // A sequence says its element type only through an instance.
    if ( MetaAny made = type.construct(); made )
        if ( auto sequence = made.as_sequence_container() )
        {
            auto elementMaybe = TryValueTypeOf( sequence.value_type() );
            if ( not elementMaybe )
                return elementMaybe;
            return ValueType::ListOf( std::move( *elementMaybe ) );
        }
    if ( mDescribed.contains( Name::FromHash( type.id() ) ) )
    {
        vector<ValueTypeField> fields;
        for ( const auto [id, field] : type.data() )
        {
            if ( InfoOf( field ).Has( FieldInfo::Transient ) )
                continue;
            auto fieldTypeMaybe = TryValueTypeOf( field.type() );
            if ( not fieldTypeMaybe )
                return fieldTypeMaybe;
            fields.push_back( ValueTypeField{ Name::FromHash( id ), std::move( *fieldTypeMaybe ) } );
        }
        return ValueType::RecordOf( std::move( fields ) );
    }
    return unexpected( format( "a {} cannot be kept as a value", NameOf( type ) ) );
}
}
