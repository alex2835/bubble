#include "bubble/scripts/script_values.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/scene.hpp"
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
        elements.push_back( table.RawGet( i ) );
    return elements;
}

class ToLua
{
public:
    ToLua( LuaState& lua, const Scene& scene, Entity from ) : mLua( lua ), mScene( scene ), mFrom( from ) {}

    expected<LuaValue, string> TryConvert( const Value& value, const string& where )
    {
        if ( const auto boolMaybe = value.TryAs<bool>() )
            return mLua.Value( *boolMaybe );
        if ( const auto numberMaybe = value.TryAs<f64>() )
            return mLua.Value( *numberMaybe );
        if ( const auto textMaybe = value.TryAs<Text>() )
            return mLua.Value( textMaybe->String() );
        if ( const auto vectorMaybe = value.TryAs<vec3>() )
            return mLua.Value( *vectorMaybe );
        if ( const auto colorMaybe = value.TryAs<Color>() )
            return mLua.Value( *colorMaybe );
        if ( const auto nameMaybe = value.TryAs<Name>() )
            return mLua.Value( nameMaybe->ToString() );
        if ( const auto idMaybe = value.TryAs<AssetId>() )
            return mLua.Value( *idMaybe );
        if ( const auto pathMaybe = value.TryAs<EntityPath>() )
        {
            if ( pathMaybe->mPath.View().empty() )
                return mLua.Value( Entity::Null );
            const auto entityMaybe = TryResolve( mScene, *pathMaybe, mFrom );
            if ( not entityMaybe )
                return unexpected( format( "{}: {}", where, entityMaybe.error() ) );
            return mLua.Value( *entityMaybe );
        }
        if ( const auto listMaybe = value.TryAs<Value::List>() )
        {
            LuaTable list = mLua.NewTable();
            for ( size_t i = 0; i < listMaybe->size(); ++i )
            {
                auto elementMaybe = TryConvert( ( *listMaybe )[i], format( "{}[{}]", where, i + 1 ) );
                if ( not elementMaybe )
                    return elementMaybe;
                list.Append( *elementMaybe );
            }
            return list;
        }
        LuaTable record = mLua.NewTable();
        for ( const ValueField& field : *value.TryAs<Value::Record>() )
        {
            auto fieldMaybe = TryConvert( field.mValue, format( "{}.{}", where, field.mName.ToString() ) );
            if ( not fieldMaybe )
                return fieldMaybe;
            record.RawSet( field.mName.ToString(), *fieldMaybe );
        }
        return record;
    }

private:
    LuaState& mLua;
    const Scene& mScene;
    Entity mFrom;
};

class FromLua
{
public:
    FromLua( LuaState& lua, const Scene& scene ) : mLua( lua ), mScene( scene ) {}

    expected<Value, string> TryRead( const LuaValue& value, const ValueType& type, const string& where )
    {
        const auto mismatch = [&] {
            return unexpected( format( "{}: {} expected, got {}", where, type.Describe(), mLua.Describe( value ) ) );
        };
        switch ( type.mKind )
        {
            case ValueKind::Bool:
                if ( const auto boolMaybe = value.TryAs<bool>() )
                    return Value( *boolMaybe );
                return mismatch();
            case ValueKind::Number:
            case ValueKind::Integer:
            {
                const auto numberMaybe = value.TryAs<f64>();
                if ( not numberMaybe )
                    return mismatch();
                // Whole or not is the declaration's to say, as for a file.
                Value number( *numberMaybe );
                if ( auto checkedMaybe = TryCheck( number, type, where ); not checkedMaybe )
                    return unexpected( std::move( checkedMaybe.error() ) );
                return number;
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
                if ( const auto vectorMaybe = value.TryAs<vec3>() )
                    return Value( *vectorMaybe );
                return mismatch();
            case ValueKind::Color:
                if ( const auto colorMaybe = value.TryAs<Color>() )
                    return Value( *colorMaybe );
                return mismatch();
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
                if ( const auto idMaybe = value.TryAs<AssetId>() )
                    return Value( *idMaybe );
                return mismatch();
            case ValueKind::Entity:
                if ( const auto entityMaybe = value.TryAs<Entity>() )
                    return Value( mScene.Alive( *entityMaybe ) ? PathOf( mScene, *entityMaybe ) : EntityPath{} );
                return mismatch();
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

    // Fields by `type` when it is given - each one there, in its order -
    // any names, by name, when not.
    template <typename Mismatch>
    expected<Value, string> TryRecord( const LuaValue& value,
                                       const ValueType* type,
                                       const string& where,
                                       const Mismatch& mismatch )
    {
        if ( not value.Is( LuaKind::Table ) )
            return mismatch();
        const LuaTable table( value );
        Value::Record record;
        if ( type )
        {
            for ( const ValueTypeField& field : type->mFields )
            {
                const string name = field.mName.ToString();
                auto fieldMaybe = TryRead( table.RawGet( name ), field.mType, format( "{}.{}", where, name ) );
                if ( not fieldMaybe )
                    return fieldMaybe;
                record.push_back( ValueField{ field.mName, std::move( *fieldMaybe ) } );
            }
            return Value( std::move( record ) );
        }
        for ( const auto& [key, fieldValue] : table.Pairs() )
        {
            const auto keyMaybe = key.TryAs<string>();
            if ( not keyMaybe )
                return unexpected( format( "{}: a field is named by a string, not {}", where, mLua.Describe( key ) ) );
            const string fieldWhere = format( "{}.{}", where, *keyMaybe );
            const auto nameMaybe = Name::TryFrom( *keyMaybe );
            if ( not nameMaybe )
                return unexpected( format( "{}: {}", fieldWhere, nameMaybe.error() ) );
            auto readMaybe = TryRead( fieldValue, cAny, fieldWhere );
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
            return unexpected( format( "{}: a boolean, number, string, vector, color, asset, entity or table "
                                       "expected, got {}",
                                       where, mLua.Describe( value ) ) );
        };
        switch ( value.Kind() )
        {
            case LuaKind::Boolean: return Value( *value.TryAs<bool>() );
            case LuaKind::Number: return Value( *value.TryAs<f64>() );
            case LuaKind::String: return TryRead( value, ValueType::Of( ValueKind::Text ), where );
            case LuaKind::Vector: return Value( *value.TryAs<vec3>() );
            case LuaKind::Userdata:
                if ( value.TryAs<Color>() )
                    return TryRead( value, ValueType::Of( ValueKind::Color ), where );
                if ( value.TryAs<AssetId>() )
                    return TryRead( value, ValueType::Of( ValueKind::Asset ), where );
                if ( value.TryAs<Entity>() )
                    return TryRead( value, ValueType::Of( ValueKind::Entity ), where );
                return unreadable();
            case LuaKind::Table:
                if ( TryArray( LuaTable( value ) ) )
                    return TryList( value, cAny, where, unreadable );
                return TryRecord( value, nullptr, where, unreadable );
            default: return unreadable();
        }
    }

    LuaState& mLua;
    const Scene& mScene;
};
}

expected<LuaValue, string> TryLuaFromValue( LuaState& lua,
                                            const Value& value,
                                            const Scene& scene,
                                            Entity from,
                                            string_view where )
{
    return ToLua( lua, scene, from ).TryConvert( value, string( where ) );
}

expected<Value, string> TryValueFromLua( LuaState& lua,
                                         const LuaValue& value,
                                         const ValueType& type,
                                         const Scene& scene,
                                         string_view where )
{
    return FromLua( lua, scene ).TryRead( value, type, string( where ) );
}
}
