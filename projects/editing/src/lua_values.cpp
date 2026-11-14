#include "lua_values.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
const ValueType cAny = ValueType::Of( ValueKind::Any );

// The entries of an array: keys 1..n and nothing else. An empty table is one.
opt<vector<LuaValue>> TryArray( const LuaTable& table )
{
    const auto pairs = table.Pairs();
    const int length = table.Length();
    if ( static_cast<int>( pairs.size() ) != length )
        return nullopt;
    vector<LuaValue> elements;
    for ( int i = 1; i <= length; ++i )
        elements.push_back( table.Get( i ) );
    return elements;
}

class LuaReader
{
public:
    explicit LuaReader( LuaState& lua ) : mLua( lua ) {}

    expected<Value, string> TryRead( const LuaValue& value, const ValueType& type, const string& where )
    {
        const auto mismatch = [&] {
            return unexpected( format( "{}: {} expected, got {}", where, type.Describe(), mLua.Describe( value ) ) );
        };
        switch ( type.mKind )
        {
            case ValueKind::Bool:
            {
                const auto boolMaybe = value.TryAs<bool>();
                if ( not boolMaybe )
                    return mismatch();
                return Value( *boolMaybe );
            }
            case ValueKind::Number:
            case ValueKind::Integer:
            {
                // Whole or not is the declaration's to check, as for JSON.
                const auto numberMaybe = value.TryAs<f64>();
                if ( not numberMaybe )
                    return mismatch();
                return Value( *numberMaybe );
            }
            case ValueKind::Text:
            {
                const auto stringMaybe = value.TryAs<string>();
                if ( not stringMaybe )
                    return mismatch();
                auto textMaybe = Text::TryFrom( *stringMaybe );
                if ( not textMaybe )
                    return unexpected( format( "{}: {}", where, textMaybe.error() ) );
                return Value( std::move( *textMaybe ) );
            }
            case ValueKind::Vec3:
            {
                const auto vectorMaybe = value.TryAs<vec3>();
                if ( not vectorMaybe )
                    return mismatch();
                return Value( *vectorMaybe );
            }
            case ValueKind::Color:
            {
                const auto arrayMaybe = value.Is( LuaKind::Table ) ? TryArray( LuaTable( value ) ) : nullopt;
                if ( not arrayMaybe or arrayMaybe->size() < 3 or arrayMaybe->size() > 4 )
                    return mismatch();
                f32 channels[4] = { 1, 1, 1, 1 };
                for ( size_t i = 0; i < arrayMaybe->size(); ++i )
                {
                    const auto numberMaybe = ( *arrayMaybe )[i].TryAs<f64>();
                    if ( not numberMaybe )
                        return mismatch();
                    channels[i] = static_cast<f32>( *numberMaybe );
                }
                return Value( Color{ channels[0], channels[1], channels[2], channels[3] } );
            }
            case ValueKind::Name:
            {
                const auto stringMaybe = value.TryAs<string>();
                if ( not stringMaybe )
                    return mismatch();
                auto nameMaybe = Name::TryFrom( *stringMaybe );
                if ( not nameMaybe )
                    return unexpected( format( "{}: {}", where, nameMaybe.error() ) );
                return Value( *nameMaybe );
            }
            case ValueKind::Asset:
            {
                const auto stringMaybe = value.TryAs<string>();
                const auto idMaybe = stringMaybe ? AssetId::TryParse( *stringMaybe ) : nullopt;
                if ( not idMaybe )
                    return mismatch();
                return Value( *idMaybe );
            }
            case ValueKind::Entity:
            {
                const auto stringMaybe = value.TryAs<string>();
                if ( not stringMaybe )
                    return mismatch();
                auto textMaybe = Text::TryFrom( *stringMaybe );
                if ( not textMaybe )
                    return unexpected( format( "{}: {}", where, textMaybe.error() ) );
                return Value( EntityPath{ std::move( *textMaybe ) } );
            }
            case ValueKind::List: return TryList( value, *type.mElement, where, mismatch );
            case ValueKind::Record: return TryRecord( value, &type, where, mismatch );
            case ValueKind::Any: return TryAny( value, where );
        }
        return mismatch();
    }

private:
    template <typename Mismatch>
    expected<Value, string> TryList( const LuaValue& value,
                                     const ValueType& element,
                                     const string& where,
                                     const Mismatch& mismatch )
    {
        const auto arrayMaybe = value.Is( LuaKind::Table ) ? TryArray( LuaTable( value ) ) : nullopt;
        if ( not arrayMaybe )
            return mismatch();
        Value::List list;
        for ( size_t i = 0; i < arrayMaybe->size(); ++i )
        {
            auto elementMaybe = TryRead( ( *arrayMaybe )[i], element, format( "{}[{}]", where, i + 1 ) );
            if ( not elementMaybe )
                return elementMaybe;
            list.push_back( std::move( *elementMaybe ) );
        }
        return Value( std::move( list ) );
    }

    // Fields by `type` when it is given; any names, any kinds when not.
    template <typename Mismatch>
    expected<Value, string> TryRecord( const LuaValue& value,
                                       const ValueType* type,
                                       const string& where,
                                       const Mismatch& mismatch )
    {
        if ( not value.Is( LuaKind::Table ) )
            return mismatch();
        Value::Record record;
        for ( const auto& [key, fieldValue] : LuaTable( value ).Pairs() )
        {
            const auto keyMaybe = key.TryAs<string>();
            if ( not keyMaybe )
                return unexpected( format( "{}: a field is named by a string, not {}", where, mLua.Describe( key ) ) );
            const string fieldWhere = format( "{}.{}", where, *keyMaybe );
            const auto nameMaybe = Name::TryFrom( *keyMaybe );
            if ( not nameMaybe )
                return unexpected( format( "{}: {}", fieldWhere, nameMaybe.error() ) );
            const ValueType* fieldType = &cAny;
            if ( type )
            {
                const auto declared = ranges::find( type->mFields, *nameMaybe, &ValueTypeField::mName );
                if ( declared == type->mFields.end() )
                    return unexpected( format( "{}: not a field", fieldWhere ) );
                fieldType = &declared->mType;
            }
            auto readMaybe = TryRead( fieldValue, *fieldType, fieldWhere );
            if ( not readMaybe )
                return readMaybe;
            record.push_back( ValueField{ *nameMaybe, std::move( *readMaybe ) } );
        }
        // Pairs come in no order a person chose: by name, so a record read
        // twice is the same record.
        ranges::sort( record, []( const ValueField& a, const ValueField& b ) {
            return a.mName.ToString() < b.mName.ToString();
        } );
        return Value( std::move( record ) );
    }

    expected<Value, string> TryAny( const LuaValue& value, const string& where )
    {
        const auto unreadable = [&] {
            return unexpected( format( "{}: a boolean, number, string, vector or table expected, got {}", where,
                                       mLua.Describe( value ) ) );
        };
        switch ( value.Kind() )
        {
            case LuaKind::Boolean: return Value( *value.TryAs<bool>() );
            case LuaKind::Number: return Value( *value.TryAs<f64>() );
            case LuaKind::String: return TryRead( value, ValueType::Of( ValueKind::Text ), where );
            case LuaKind::Vector: return Value( *value.TryAs<vec3>() );
            case LuaKind::Table:
                if ( TryArray( LuaTable( value ) ) )
                    return TryList( value, cAny, where, unreadable );
                return TryRecord( value, nullptr, where, unreadable );
            default: return unreadable();
        }
    }

    LuaState& mLua;
};
}

expected<Value, string> TryValueFromLua( LuaState& lua,
                                         const LuaValue& value,
                                         const ValueType& type,
                                         string_view where )
{
    return LuaReader( lua ).TryRead( value, type, string( where ) );
}

LuaValue LuaFromValue( LuaState& lua, const Value& value )
{
    if ( const auto boolMaybe = value.TryAs<bool>() )
        return lua.Value( *boolMaybe );
    if ( const auto numberMaybe = value.TryAs<f64>() )
        return lua.Value( *numberMaybe );
    if ( const auto textMaybe = value.TryAs<Text>() )
        return lua.Value( textMaybe->String() );
    if ( const auto vectorMaybe = value.TryAs<vec3>() )
        return lua.Value( *vectorMaybe );
    if ( const auto colorMaybe = value.TryAs<Color>() )
    {
        LuaTable color = lua.NewTable();
        for ( const f32 channel : { colorMaybe->mR, colorMaybe->mG, colorMaybe->mB, colorMaybe->mA } )
            color.Append( static_cast<f64>( channel ) );
        return color;
    }
    if ( const auto nameMaybe = value.TryAs<Name>() )
        return lua.Value( nameMaybe->ToString() );
    if ( const auto idMaybe = value.TryAs<AssetId>() )
        return lua.Value( idMaybe->ToString() );
    if ( const auto pathMaybe = value.TryAs<EntityPath>() )
        return lua.Value( pathMaybe->mPath.String() );
    if ( const auto listMaybe = value.TryAs<Value::List>() )
    {
        LuaTable list = lua.NewTable();
        for ( const Value& element : *listMaybe )
            list.Append( LuaFromValue( lua, element ) );
        return list;
    }
    LuaTable record = lua.NewTable();
    for ( const ValueField& field : *value.TryAs<Value::Record>() )
        record[field.mName.ToString()] = LuaFromValue( lua, field.mValue );
    return record;
}
}
