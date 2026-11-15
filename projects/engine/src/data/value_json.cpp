#include "bubble/data/value_json.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
const ValueType cAny = ValueType::Of( ValueKind::Any );

bool Whole( f64 number )
{
    constexpr f64 cExact = 9007199254740992.0; // 2^53
    return number >= -cExact and number <= cExact and number == static_cast<f64>( static_cast<i64>( number ) );
}

// NaN and infinity are refused, not written null: null would not read back
// as a number, and the file would not load.
unexpected<string> Unwritable( f64 number, const string& where )
{
    // A NaN's sign says nothing, so it is not printed: "nan", not "-nan".
    return unexpected(
        format( "{}: {} is a number a file cannot hold", where, isnan( number ) ? "nan"s : format( "{}", number ) ) );
}

expected<Json, string> TryWriteFloats( initializer_list<f32> numbers, const string& where )
{
    Json::Array array;
    for ( const f32 number : numbers )
    {
        if ( not isfinite( number ) )
            return Unwritable( number, where );
        array.push_back( Json( WidenFloat( number ) ) );
    }
    return Json( std::move( array ) );
}

// Plain where the type says the kind, tagged where it is Any.
Json Kind( string_view tag, Json json, const ValueType& type )
{
    if ( type.mKind != ValueKind::Any )
        return json;
    Json::Object object;
    object.push_back( Json::Member{ string( tag ), std::move( json ) } );
    return Json( std::move( object ) );
}

const ValueType& FieldType( const ValueType& type, Name field )
{
    for ( const ValueTypeField& declared : type.mFields )
        if ( declared.mName == field )
            return declared.mType;
    return cAny;
}

string FieldNames( const ValueType& type )
{
    string names;
    for ( const ValueTypeField& field : type.mFields )
        names += ( names.empty() ? "" : ", " ) + field.mName.ToString();
    return names;
}

unexpected<string> Fail( const Json& json, const string& where, string_view what )
{
    const string at = json.Where();
    return unexpected( at.empty() ? format( "{}: {}", where, what ) : format( "{}: {}: {}", at, where, what ) );
}

class Reader
{
public:
    explicit Reader( const AssetCatalog& assets ) : mAssets( assets ) {}

    expected<Value, string> TryRead( const Json& json, const ValueType& type, const string& where )
    {
        const auto mismatch = [&] {
            return Fail( json, where, format( "{} expected, got {}", type.Describe(), json.Describe() ) );
        };
        switch ( type.mKind )
        {
            case ValueKind::Bool:
                if ( const auto flagMaybe = json.TryAs<bool>() )
                    return Value( *flagMaybe );
                return mismatch();
            case ValueKind::Number:
                if ( const auto numberMaybe = json.TryAs<f64>() )
                    return Value( *numberMaybe );
                return mismatch();
            case ValueKind::Integer:
                if ( const auto numberMaybe = json.TryAs<f64>(); numberMaybe and Whole( *numberMaybe ) )
                    return Value( *numberMaybe );
                return mismatch();
            case ValueKind::Text:
            {
                const auto textMaybe = json.TryAs<string>();
                if ( not textMaybe )
                    return mismatch();
                auto madeMaybe = Text::TryFrom( *textMaybe );
                if ( not madeMaybe )
                    return Fail( json, where, madeMaybe.error() );
                return Value( std::move( *madeMaybe ) );
            }
            case ValueKind::Vec3:
            {
                const auto numbersMaybe = TryNumbers( json, 3, 3 );
                if ( not numbersMaybe )
                    return mismatch();
                const auto floatsMaybe = TryFloats( json, *numbersMaybe, where );
                if ( not floatsMaybe )
                    return unexpected( floatsMaybe.error() );
                return Value( vec3( ( *floatsMaybe )[0], ( *floatsMaybe )[1], ( *floatsMaybe )[2] ) );
            }
            case ValueKind::Color:
            {
                // Alpha may be left out: an opaque color.
                const auto numbersMaybe = TryNumbers( json, 3, 4 );
                if ( not numbersMaybe )
                    return Fail( json, where, format( "a color [r, g, b, a] expected, got {}", json.Describe() ) );
                const auto floatsMaybe = TryFloats( json, *numbersMaybe, where );
                if ( not floatsMaybe )
                    return unexpected( floatsMaybe.error() );
                const vector<f32>& at = *floatsMaybe;
                return Value( Color{ at[0], at[1], at[2], at.size() == 4 ? at[3] : 1.0f } );
            }
            case ValueKind::Name:
            {
                const auto textMaybe = json.TryAs<string>();
                if ( not textMaybe )
                    return mismatch();
                const auto nameMaybe = Name::TryFrom( *textMaybe );
                if ( not nameMaybe )
                    return Fail( json, where, nameMaybe.error() );
                return Value( *nameMaybe );
            }
            case ValueKind::Asset:
            {
                const auto assetIdMaybe = TryAssetFromJson( json, where, mAssets );
                if ( not assetIdMaybe )
                    return unexpected( assetIdMaybe.error() );
                return Value( *assetIdMaybe );
            }
            case ValueKind::Entity:
            {
                const auto textMaybe = json.TryAs<string>();
                if ( not textMaybe )
                    return mismatch();
                auto pathMaybe = Text::TryFrom( *textMaybe );
                if ( not pathMaybe )
                    return Fail( json, where, pathMaybe.error() );
                return Value( EntityPath{ std::move( *pathMaybe ) } );
            }
            case ValueKind::List:
            {
                const auto arrayMaybe = json.TryAs<Json::Array>();
                if ( not arrayMaybe )
                    return mismatch();
                const ValueType& element = type.mElement ? *type.mElement : cAny;
                Value::List list;
                for ( size_t i = 0; i < arrayMaybe->size(); ++i )
                {
                    auto valueMaybe = TryRead( ( *arrayMaybe )[i], element, format( "{}[{}]", where, i + 1 ) );
                    if ( not valueMaybe )
                        return valueMaybe;
                    list.push_back( std::move( *valueMaybe ) );
                }
                return Value( std::move( list ) );
            }
            case ValueKind::Record:
            {
                if ( not json.Is<Json::Object>() )
                    return mismatch();
                return TryRecordOf( json, &type, where );
            }
            case ValueKind::Any: return TryReadAny( json, where );
        }
        return mismatch();
    }

private:
    // A value that says its own kind: the plain JSON kinds as themselves,
    // the rest tagged.
    expected<Value, string> TryReadAny( const Json& json, const string& where )
    {
        if ( json.Is<bool>() )
            return TryRead( json, ValueType::Of( ValueKind::Bool ), where );
        if ( json.Is<f64>() )
            return TryRead( json, ValueType::Of( ValueKind::Number ), where );
        if ( json.Is<string>() )
            return TryRead( json, ValueType::Of( ValueKind::Text ), where );
        if ( json.Is<Json::Array>() )
            return TryRead( json, ValueType::ListOf( cAny ), where );
        const auto objectMaybe = json.TryAs<Json::Object>();
        if ( not objectMaybe )
            return Fail( json, where, format( "a value expected, got {}", json.Describe() ) );

        if ( objectMaybe->size() == 1 and objectMaybe->front().mKey.starts_with( '$' ) )
        {
            static constexpr pair<string_view, ValueKind> cTags[] = {
                { "$vec3", ValueKind::Vec3 },   { "$color", ValueKind::Color },   { "$name", ValueKind::Name },
                { "$asset", ValueKind::Asset }, { "$entity", ValueKind::Entity },
            };
            const string& tag = objectMaybe->front().mKey;
            for ( const auto& [known, kind] : cTags )
                if ( tag == known )
                    return TryRead( objectMaybe->front().mValue, ValueType::Of( kind ), where );
            return Fail( json, where, format( "{} is not a tag (tags: $vec3, $color, $name, $asset, $entity)", tag ) );
        }
        return TryRecordOf( json, nullptr, where );
    }

    // Fields by `type` when it is given; any names, any kinds when not.
    expected<Value, string> TryRecordOf( const Json& json, const ValueType* type, const string& where )
    {
        Value::Record record;
        for ( const Json::Member& member : *json.TryAs<Json::Object>() )
        {
            const string fieldWhere = format( "{}.{}", where, member.mKey );
            const auto nameMaybe = Name::TryFrom( member.mKey );
            if ( not nameMaybe )
                return Fail( member.mValue, fieldWhere, nameMaybe.error() );
            for ( const ValueField& field : record )
                if ( field.mName == *nameMaybe )
                    return Fail( member.mValue, fieldWhere, "given twice" );
            const ValueType* fieldType = &cAny;
            if ( type )
            {
                const auto declared = ranges::find( type->mFields, *nameMaybe, &ValueTypeField::mName );
                if ( declared == type->mFields.end() )
                    return Fail( member.mValue, fieldWhere, format( "not a field (fields: {})", FieldNames( *type ) ) );
                fieldType = &declared->mType;
            }
            auto valueMaybe = TryRead( member.mValue, *fieldType, fieldWhere );
            if ( not valueMaybe )
                return valueMaybe;
            record.push_back( ValueField{ *nameMaybe, std::move( *valueMaybe ) } );
        }
        return Value( std::move( record ) );
    }

    static opt<vector<f64>> TryNumbers( const Json& json, size_t least, size_t most )
    {
        const auto arrayMaybe = json.TryAs<Json::Array>();
        if ( not arrayMaybe or arrayMaybe->size() < least or arrayMaybe->size() > most )
            return nullopt;
        vector<f64> numbers;
        for ( const Json& element : *arrayMaybe )
        {
            const auto numberMaybe = element.TryAs<f64>();
            if ( not numberMaybe )
                return nullopt;
            numbers.push_back( *numberMaybe );
        }
        return numbers;
    }

    // A float reaches less far than the double a file's number is read as;
    // what lies past it would be infinity, which no file holds. A number too
    // small for a float is only rounded, to zero, as 0.1 is to its float.
    static expected<vector<f32>, string> TryFloats( const Json& json, const vector<f64>& numbers, const string& where )
    {
        // Halfway between the biggest float and the next power of two: from
        // there on a double rounds to infinity.
        constexpr f64 cFloatReach = 0x1.ffffffp127;
        vector<f32> floats;
        for ( size_t i = 0; i < numbers.size(); ++i )
        {
            if ( not( numbers[i] > -cFloatReach and numbers[i] < cFloatReach ) )
                return Fail( ( *json.TryAs<Json::Array>() )[i], format( "{}[{}]", where, i + 1 ),
                             format( "{} is past what a float holds", numbers[i] ) );
            floats.push_back( static_cast<f32>( numbers[i] ) );
        }
        return floats;
    }

    const AssetCatalog& mAssets;
};
}

namespace
{
expected<Json, string> TryWrite( const Value& value,
                                 const ValueType& type,
                                 const string& where,
                                 const AssetCatalog& assets )
{
    if ( const auto flagMaybe = value.TryAs<bool>() )
        return Json( *flagMaybe );
    if ( const auto numberMaybe = value.TryAs<f64>() )
    {
        if ( not isfinite( *numberMaybe ) )
            return Unwritable( *numberMaybe, where );
        return Json( *numberMaybe );
    }
    if ( const auto textMaybe = value.TryAs<Text>() )
        return Json( textMaybe->String() );
    if ( const auto vMaybe = value.TryAs<vec3>() )
    {
        auto numbersMaybe = TryWriteFloats( { vMaybe->x, vMaybe->y, vMaybe->z }, where );
        if ( not numbersMaybe )
            return numbersMaybe;
        return Kind( "$vec3", std::move( *numbersMaybe ), type );
    }
    if ( const auto colorMaybe = value.TryAs<Color>() )
    {
        auto numbersMaybe = TryWriteFloats( { colorMaybe->mR, colorMaybe->mG, colorMaybe->mB, colorMaybe->mA }, where );
        if ( not numbersMaybe )
            return numbersMaybe;
        return Kind( "$color", std::move( *numbersMaybe ), type );
    }
    if ( const auto nameMaybe = value.TryAs<Name>() )
        return Kind( "$name", Json( nameMaybe->ToString() ), type );
    if ( const auto assetIdMaybe = value.TryAs<AssetId>() )
        return Kind( "$asset", AssetToJson( *assetIdMaybe, assets ), type );
    if ( const auto pathMaybe = value.TryAs<EntityPath>() )
        return Kind( "$entity", Json( pathMaybe->mPath.String() ), type );
    if ( const auto listMaybe = value.TryAs<Value::List>() )
    {
        const ValueType& element = type.mKind == ValueKind::List and type.mElement ? *type.mElement : cAny;
        Json::Array array;
        for ( size_t i = 0; i < listMaybe->size(); ++i )
        {
            auto itemMaybe = TryWrite( ( *listMaybe )[i], element, format( "{}[{}]", where, i + 1 ), assets );
            if ( not itemMaybe )
                return itemMaybe;
            array.push_back( std::move( *itemMaybe ) );
        }
        return Json( std::move( array ) );
    }
    Json::Object object;
    for ( const ValueField& field : *value.TryAs<Value::Record>() )
    {
        const string name = field.mName.ToString();
        auto fieldMaybe =
            TryWrite( field.mValue, FieldType( type, field.mName ), format( "{}.{}", where, name ), assets );
        if ( not fieldMaybe )
            return fieldMaybe;
        object.push_back( Json::Member{ name, std::move( *fieldMaybe ) } );
    }
    return Json( std::move( object ) );
}
}

expected<Json, string> TryValueToJson( const Value& value,
                                       const ValueType& type,
                                       string_view name,
                                       const AssetCatalog& assets )
{
    return TryWrite( value, type, string( name ), assets );
}

expected<Value, string> TryValueFromJson( const Json& json,
                                          const ValueType& type,
                                          string_view name,
                                          const AssetCatalog& assets )
{
    return Reader( assets ).TryRead( json, type, string( name ) );
}

Json AssetToJson( const AssetId& assetId, const AssetCatalog& assets )
{
    Json::Object link;
    link.push_back( Json::Member{ "asset", Json( assetId.ToString() ) } );
    if ( const auto pathMaybe = assets.TryPathOf( assetId ) )
        link.push_back( Json::Member{ "path", Json( pathMaybe->String() ) } );
    return Json( std::move( link ) );
}

expected<AssetId, string> TryAssetFromJson( const Json& json, string_view where, const AssetCatalog& assets )
{
    const string at( where );
    const auto membersMaybe = json.TryAs<Json::Object>();
    if ( not membersMaybe )
        return Fail(
            json, at,
            format( R"(an asset - {{ "asset": "<id>", "path": "<path>" }} - expected, got {})", json.Describe() ) );
    for ( const Json::Member& member : *membersMaybe )
        if ( member.mKey != "asset" and member.mKey != "path" )
            return Fail( member.mValue, at, format( "{} is not a key of an asset (keys: asset, path)", member.mKey ) );
    const auto idJsonMaybe = json.TryFind( "asset" );
    if ( not idJsonMaybe )
        return Fail( json, at, R"(an asset without its id: "asset": "<32 hex digits>")" );
    const auto idTextMaybe = idJsonMaybe->TryAs<string>();
    const auto assetIdMaybe = idTextMaybe ? AssetId::TryParse( *idTextMaybe ) : nullopt;
    if ( not assetIdMaybe )
        return Fail( *idJsonMaybe, at,
                     format( "an asset id - 32 hex digits - expected, got {}", idJsonMaybe->Describe() ) );
    if ( assets.TryPathOf( *assetIdMaybe ) )
        return *assetIdMaybe;

    if ( const auto pathJsonMaybe = json.TryFind( "path" ) )
    {
        const auto pathTextMaybe = pathJsonMaybe->TryAs<string>();
        if ( not pathTextMaybe )
            return Fail( *pathJsonMaybe, at,
                         format( "an asset's path - text - expected, got {}", pathJsonMaybe->Describe() ) );
        const auto pathMaybe = AssetPath::TryFrom( *pathTextMaybe );
        if ( not pathMaybe )
            return Fail( *pathJsonMaybe, at, pathMaybe.error() );
        if ( const auto foundMaybe = assets.TryIdOf( *pathMaybe ) )
            return *foundMaybe;
    }
    return *assetIdMaybe;
}
}
