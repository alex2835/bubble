#include "bubble/data/value.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
// What a person counts with: whole, and exact in a double.
bool Whole( f64 number )
{
    constexpr f64 cExact = 9007199254740992.0; // 2^53
    return number >= -cExact and number <= cExact and number == static_cast<f64>( static_cast<i64>( number ) );
}

string Noun( const ValueType& type )
{
    switch ( type.mKind )
    {
        case ValueKind::Bool: return "bool";
        case ValueKind::Number: return "number";
        case ValueKind::Integer: return "integer";
        case ValueKind::Text: return "text";
        case ValueKind::Vec3: return "vec3";
        case ValueKind::Color: return "color";
        case ValueKind::Name: return "name";
        case ValueKind::Asset: return "asset";
        case ValueKind::Entity: return "entity";
        case ValueKind::List: return type.mElement ? "list of " + Noun( *type.mElement ) : "list";
        case ValueKind::Record:
        {
            string fields;
            for ( const ValueTypeField& field : type.mFields )
                fields += ( fields.empty() ? "" : ", " ) + field.mName.ToString();
            return "record (" + fields + ")";
        }
    }
    return "value";
}

string FieldNames( const ValueType& type )
{
    string names;
    for ( const ValueTypeField& field : type.mFields )
        names += ( names.empty() ? "" : ", " ) + field.mName.ToString();
    return names;
}

expected<void, string> CheckAt( const Value& value, const ValueType& type, const string& path )
{
    const auto mismatch = [&] {
        return unexpected( format( "{}: {} expected, got {}", path, type.Describe(), value.Describe() ) );
    };
    switch ( type.mKind )
    {
        case ValueKind::Bool: return value.Is<bool>() ? expected<void, string>() : mismatch();
        case ValueKind::Number: return value.Is<f64>() ? expected<void, string>() : mismatch();
        case ValueKind::Integer:
        {
            const auto number = value.As<f64>();
            return number and Whole( *number ) ? expected<void, string>() : mismatch();
        }
        case ValueKind::Text: return value.Is<Text>() ? expected<void, string>() : mismatch();
        case ValueKind::Vec3: return value.Is<vec3>() ? expected<void, string>() : mismatch();
        case ValueKind::Color: return value.Is<Color>() ? expected<void, string>() : mismatch();
        case ValueKind::Name: return value.Is<Name>() ? expected<void, string>() : mismatch();
        case ValueKind::Asset: return value.Is<AssetId>() ? expected<void, string>() : mismatch();
        case ValueKind::Entity: return value.Is<EntityPath>() ? expected<void, string>() : mismatch();
        case ValueKind::List:
        {
            const auto list = value.As<Value::List>();
            if ( not list )
                return mismatch();
            if ( not type.mElement )
                throw logic_error( format( "{}: a list type without an element type", path ) );
            for ( size_t i = 0; i < list->size(); ++i )
                if ( auto checked = CheckAt( ( *list )[i], *type.mElement, format( "{}[{}]", path, i + 1 ) );
                     not checked )
                    return checked;
            return {};
        }
        case ValueKind::Record:
        {
            const auto record = value.As<Value::Record>();
            if ( not record )
                return mismatch();
            hset<Name> seen;
            for ( const ValueField& field : *record )
            {
                const string fieldPath = path + "." + field.mName.ToString();
                if ( not seen.insert( field.mName ).second )
                    return unexpected( format( "{}: given twice", fieldPath ) );
                const auto declared = ranges::find( type.mFields, field.mName, &ValueTypeField::mName );
                if ( declared == type.mFields.end() )
                    return unexpected( format( "{}: not a field (fields: {})", fieldPath, FieldNames( type ) ) );
                if ( auto checked = CheckAt( field.mValue, declared->mType, fieldPath ); not checked )
                    return checked;
            }
            for ( const ValueTypeField& field : type.mFields )
                if ( not seen.contains( field.mName ) )
                    return unexpected( format( "{}: no field {}", path, field.mName.ToString() ) );
            return {};
        }
    }
    return mismatch();
}
}

Value::Value() : mData( false )
{
}

Value::Value( bool value ) : mData( value )
{
}

Value::Value( f64 value ) : mData( value )
{
}

Value::Value( int value ) : mData( static_cast<f64>( value ) )
{
}

Value::Value( Text value ) : mData( std::move( value ) )
{
}

Value::Value( vec3 value ) : mData( value )
{
}

Value::Value( Color value ) : mData( value )
{
}

Value::Value( Name value ) : mData( value )
{
}

Value::Value( AssetId value ) : mData( value )
{
}

Value::Value( EntityPath value ) : mData( std::move( value ) )
{
}

Value::Value( List value ) : mData( std::move( value ) )
{
}

Value::Value( Record value ) : mData( std::move( value ) )
{
}

Value::Value( const Value& ) = default;
Value::Value( Value&& ) noexcept = default;
Value& Value::operator=( const Value& ) = default;
Value& Value::operator=( Value&& ) noexcept = default;
Value::~Value() = default;

bool operator==( const Value& a, const Value& b )
{
    return a.mData == b.mData;
}

string Value::Describe() const
{
    if ( const auto value = As<bool>() )
        return *value ? "true" : "false";
    if ( const auto value = As<f64>() )
        return format( "{}", *value );
    if ( const auto value = As<Text>() )
        return format( "\"{}\"", value->View() );
    if ( const auto value = As<vec3>() )
        return format( "vec3( {}, {}, {} )", value->x, value->y, value->z );
    if ( const auto value = As<Color>() )
        return format( "color( {}, {}, {}, {} )", value->mR, value->mG, value->mB, value->mA );
    if ( const auto value = As<Name>() )
        return format( "name {}", value->ToString() );
    if ( const auto value = As<AssetId>() )
        return format( "asset {}", value->ToString() );
    if ( const auto value = As<EntityPath>() )
        return format( "entity {}", value->mPath.View() );
    if ( const auto value = As<List>() )
        return format( "a list of {}", value->size() );
    return "a record";
}

ValueType ValueType::ListOf( ValueType element )
{
    return ValueType{ ValueKind::List, CreateRef<const ValueType>( std::move( element ) ), {} };
}

ValueType ValueType::RecordOf( vector<ValueTypeField> fields )
{
    return ValueType{ ValueKind::Record, nullptr, std::move( fields ) };
}

string ValueType::Describe() const
{
    const string noun = Noun( *this );
    if ( mKind == ValueKind::Text )
        return noun;
    const bool vowel = noun.find_first_of( "aeiou" ) == 0;
    return ( vowel ? "an " : "a " ) + noun;
}

expected<void, string> Check( const Value& value, const ValueType& type, string_view name )
{
    return CheckAt( value, type, string( name ) );
}
}
