#include "bubble/scripts/script_asset.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/core/asset_path.hpp"
#include "bubble/data/binary.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include <cstring>

namespace bubble
{
namespace
{
constexpr string_view cMagic = "BSCR";
// Bumped when the layout changes; old bytes are refused, not converted.
constexpr u32 cFormat = 1;
// Deeper than any prop a person writes; what damaged bytes stop at.
constexpr u32 cMaxDepth = 64;

void WriteF64( BinaryWriter& out, f64 value )
{
    char bytes[sizeof( f64 )];
    std::memcpy( bytes, &value, sizeof( f64 ) );
    out.Bytes( string_view( bytes, sizeof( f64 ) ) );
}

void WriteType( BinaryWriter& out, const ValueType& type )
{
    out.U32( static_cast<u32>( type.mKind ) );
    if ( type.mKind == ValueKind::List )
        WriteType( out, *type.mElement );
    if ( type.mKind == ValueKind::Record )
    {
        out.U32( static_cast<u32>( type.mFields.size() ) );
        for ( const ValueTypeField& field : type.mFields )
        {
            out.Text( field.mName.ToString() );
            WriteType( out, field.mType );
        }
    }
}

// The variant's alternative first, then what it holds.
void WriteValue( BinaryWriter& out, const Value& value )
{
    if ( const auto boolMaybe = value.TryAs<bool>() )
    {
        out.U32( 0 );
        out.U32( *boolMaybe ? 1 : 0 );
    }
    else if ( const auto numberMaybe = value.TryAs<f64>() )
    {
        out.U32( 1 );
        WriteF64( out, *numberMaybe );
    }
    else if ( const auto textMaybe = value.TryAs<Text>() )
    {
        out.U32( 2 );
        out.Text( textMaybe->View() );
    }
    else if ( const auto vectorMaybe = value.TryAs<vec3>() )
    {
        out.U32( 3 );
        for ( size_t i = 0; i < 3; ++i )
            out.F32( ( *vectorMaybe )[static_cast<int>( i )] );
    }
    else if ( const auto colorMaybe = value.TryAs<Color>() )
    {
        out.U32( 4 );
        for ( const f32 channel : { colorMaybe->mR, colorMaybe->mG, colorMaybe->mB, colorMaybe->mA } )
            out.F32( channel );
    }
    else if ( const auto nameMaybe = value.TryAs<Name>() )
    {
        out.U32( 5 );
        out.Text( nameMaybe->ToString() );
    }
    else if ( const auto idMaybe = value.TryAs<AssetId>() )
    {
        out.U32( 6 );
        out.Text( *idMaybe ? idMaybe->ToString() : string() );
    }
    else if ( const auto pathMaybe = value.TryAs<EntityPath>() )
    {
        out.U32( 7 );
        out.Text( pathMaybe->mPath.View() );
    }
    else if ( const auto listMaybe = value.TryAs<Value::List>() )
    {
        out.U32( 8 );
        out.U32( static_cast<u32>( listMaybe->size() ) );
        for ( const Value& element : *listMaybe )
            WriteValue( out, element );
    }
    else
    {
        const Value::Record& record = *value.TryAs<Value::Record>();
        out.U32( 9 );
        out.U32( static_cast<u32>( record.size() ) );
        for ( const ValueField& field : record )
        {
            out.Text( field.mName.ToString() );
            WriteValue( out, field.mValue );
        }
    }
}

class Reader
{
public:
    explicit Reader( string_view bytes ) : mIn( bytes ) {}

    BinaryReader& In() { return mIn; }

    opt<Name> TryName()
    {
        const auto textMaybe = mIn.TryText();
        if ( not textMaybe )
            return nullopt;
        const auto nameMaybe = Name::TryFrom( *textMaybe );
        return nameMaybe ? opt<Name>( *nameMaybe ) : nullopt;
    }

    opt<ValueType> TryType( u32 depth )
    {
        const auto kindMaybe = mIn.TryU32();
        if ( not kindMaybe or *kindMaybe > static_cast<u32>( ValueKind::Any ) or depth > cMaxDepth )
            return nullopt;
        const auto kind = static_cast<ValueKind>( *kindMaybe );
        if ( kind == ValueKind::List )
        {
            auto elementMaybe = TryType( depth + 1 );
            return elementMaybe ? opt<ValueType>( ValueType::ListOf( std::move( *elementMaybe ) ) ) : nullopt;
        }
        if ( kind == ValueKind::Record )
        {
            const auto countMaybe = mIn.TryU32();
            if ( not countMaybe or *countMaybe > mIn.Left() )
                return nullopt;
            vector<ValueTypeField> fields;
            for ( u32 i = 0; i < *countMaybe; ++i )
            {
                const auto nameMaybe = TryName();
                auto typeMaybe = nameMaybe ? TryType( depth + 1 ) : nullopt;
                if ( not typeMaybe )
                    return nullopt;
                fields.push_back( ValueTypeField{ *nameMaybe, std::move( *typeMaybe ) } );
            }
            return ValueType::RecordOf( std::move( fields ) );
        }
        return ValueType::Of( kind );
    }

    opt<Value> TryValue( u32 depth )
    {
        const auto tagMaybe = mIn.TryU32();
        if ( not tagMaybe or depth > cMaxDepth )
            return nullopt;
        switch ( *tagMaybe )
        {
            case 0:
            {
                const auto boolMaybe = mIn.TryU32();
                return boolMaybe ? opt<Value>( Value( *boolMaybe != 0 ) ) : nullopt;
            }
            case 1:
            {
                const auto bytesMaybe = mIn.TryBytes( sizeof( f64 ) );
                if ( not bytesMaybe )
                    return nullopt;
                f64 number = 0;
                std::memcpy( &number, bytesMaybe->data(), sizeof( f64 ) );
                return Value( number );
            }
            case 2:
            case 7:
            {
                const auto textMaybe = mIn.TryText();
                auto validMaybe = textMaybe ? Text::TryFrom( *textMaybe ) : unexpected( ""s );
                if ( not validMaybe )
                    return nullopt;
                if ( *tagMaybe == 2 )
                    return Value( std::move( *validMaybe ) );
                return Value( EntityPath{ std::move( *validMaybe ) } );
            }
            case 3:
            case 4:
            {
                const size_t count = *tagMaybe == 3 ? 3 : 4;
                f32 channels[4] = {};
                for ( size_t i = 0; i < count; ++i )
                {
                    const auto channelMaybe = mIn.TryF32();
                    if ( not channelMaybe )
                        return nullopt;
                    channels[i] = *channelMaybe;
                }
                if ( count == 3 )
                    return Value( vec3( channels[0], channels[1], channels[2] ) );
                return Value( Color{ .mR = channels[0], .mG = channels[1], .mB = channels[2], .mA = channels[3] } );
            }
            case 5:
            {
                const auto nameMaybe = TryName();
                return nameMaybe ? opt<Value>( Value( *nameMaybe ) ) : nullopt;
            }
            case 6:
            {
                const auto textMaybe = mIn.TryText();
                if ( not textMaybe )
                    return nullopt;
                if ( textMaybe->empty() )
                    return Value( AssetId{} );
                const auto idMaybe = AssetId::TryParse( *textMaybe );
                return idMaybe ? opt<Value>( Value( *idMaybe ) ) : nullopt;
            }
            case 8:
            {
                const auto countMaybe = mIn.TryU32();
                if ( not countMaybe or *countMaybe > mIn.Left() )
                    return nullopt;
                Value::List list;
                for ( u32 i = 0; i < *countMaybe; ++i )
                {
                    auto elementMaybe = TryValue( depth + 1 );
                    if ( not elementMaybe )
                        return nullopt;
                    list.push_back( std::move( *elementMaybe ) );
                }
                return Value( std::move( list ) );
            }
            case 9:
            {
                const auto countMaybe = mIn.TryU32();
                if ( not countMaybe or *countMaybe > mIn.Left() )
                    return nullopt;
                Value::Record record;
                for ( u32 i = 0; i < *countMaybe; ++i )
                {
                    const auto nameMaybe = TryName();
                    auto fieldMaybe = nameMaybe ? TryValue( depth + 1 ) : nullopt;
                    if ( not fieldMaybe )
                        return nullopt;
                    record.push_back( ValueField{ *nameMaybe, std::move( *fieldMaybe ) } );
                }
                return Value( std::move( record ) );
            }
            default: return nullopt;
        }
    }

private:
    BinaryReader mIn;
};
}

string WriteScriptAsset( const ScriptAsset& asset )
{
    BinaryWriter out;
    out.Bytes( cMagic );
    out.U32( cFormat );
    out.U32( static_cast<u32>( asset.mProps.size() ) );
    for ( const ScriptProp& prop : asset.mProps )
    {
        out.Text( prop.mName.ToString() );
        WriteType( out, prop.mType );
        WriteValue( out, prop.mDefault );
        out.F32( prop.mMin );
        out.F32( prop.mMax );
        out.Text( prop.mTooltip );
    }
    out.U32( static_cast<u32>( asset.mRequires.size() ) );
    for ( const string& path : asset.mRequires )
        out.Text( path );
    out.Text( asset.mBytecode );
    return out.Take();
}

expected<ScriptAsset, string> TryReadScriptAsset( string_view bytes )
{
    Reader reader( bytes );
    BinaryReader& in = reader.In();
    if ( in.TryBytes( cMagic.size() ) != cMagic )
        return unexpected( "not a script the importer wrote"s );
    if ( const auto formatMaybe = in.TryU32(); formatMaybe != cFormat )
        return unexpected( format( "a script of format {}, this build reads {}", formatMaybe.value_or( 0 ), cFormat ) );
    const auto damaged = [] { return unexpected( "the imported script is damaged"s ); };

    ScriptAsset asset;
    const auto propsMaybe = in.TryU32();
    if ( not propsMaybe or *propsMaybe > in.Left() )
        return damaged();
    for ( u32 i = 0; i < *propsMaybe; ++i )
    {
        ScriptProp prop;
        const auto nameMaybe = reader.TryName();
        auto typeMaybe = nameMaybe ? reader.TryType( 0 ) : nullopt;
        auto defaultMaybe = typeMaybe ? reader.TryValue( 0 ) : nullopt;
        const auto minMaybe = defaultMaybe ? in.TryF32() : nullopt;
        const auto maxMaybe = minMaybe ? in.TryF32() : nullopt;
        const auto tooltipMaybe = maxMaybe ? in.TryText() : nullopt;
        if ( not tooltipMaybe )
            return damaged();
        asset.mProps.push_back( ScriptProp{ .mName = *nameMaybe,
                                            .mType = std::move( *typeMaybe ),
                                            .mDefault = std::move( *defaultMaybe ),
                                            .mMin = *minMaybe,
                                            .mMax = *maxMaybe,
                                            .mTooltip = string( *tooltipMaybe ) } );
    }
    const auto requiresMaybe = in.TryU32();
    if ( not requiresMaybe or *requiresMaybe > in.Left() )
        return damaged();
    for ( u32 i = 0; i < *requiresMaybe; ++i )
    {
        const auto pathMaybe = in.TryText();
        if ( not pathMaybe )
            return damaged();
        asset.mRequires.emplace_back( *pathMaybe );
    }
    const auto bytecodeMaybe = in.TryText();
    if ( not bytecodeMaybe or not in.AtEnd() )
        return damaged();
    asset.mBytecode = string( *bytecodeMaybe );
    return asset;
}

void RegisterScriptLoader( AssetRegistry& registry )
{
    registry.RegisterLoader<ScriptAsset>(
        cScriptKind, []( string_view bytes, const AssetPath& ) { return TryReadScriptAsset( bytes ); } );
}

expected<string, string> TryResolveRequire( string_view from, string_view request, const hmap<string, string>& aliases )
{
    string joined;
    if ( request.starts_with( "./" ) or request.starts_with( "../" ) )
    {
        const size_t slash = from.rfind( '/' );
        const string_view directory = slash == string_view::npos ? string_view() : from.substr( 0, slash );
        joined = string( directory ) + "/" + string( request );
    }
    else if ( request.starts_with( '@' ) )
    {
        const size_t slash = request.find( '/' );
        const string alias( request.substr( 1, slash == string_view::npos ? string_view::npos : slash - 1 ) );
        const auto found = aliases.find( alias );
        if ( found == aliases.end() )
        {
            vector<string> known;
            for ( const auto& entry : aliases )
                known.push_back( "@" + entry.first );
            ranges::sort( known );
            string list;
            for ( const string& name : known )
                list += ( list.empty() ? "" : ", " ) + name;
            return unexpected( format( "no alias @{} (aliases: {})", alias, list.empty() ? "none" : list ) );
        }
        joined = found->second + ( slash == string_view::npos ? "" : string( request.substr( slash ) ) );
    }
    else
        return unexpected( "a path starts with ./, ../ or @alias"s );

    // "." steps go, ".." takes one back.
    vector<string_view> steps;
    const string_view all = joined;
    for ( size_t at = 0; at <= all.size(); )
    {
        const size_t end = min( all.find( '/', at ), all.size() );
        const string_view step = all.substr( at, end - at );
        at = end + 1;
        if ( step.empty() or step == "." )
            continue;
        if ( step == ".." )
        {
            if ( steps.empty() )
                return unexpected( "the path leads out of the project"s );
            steps.pop_back();
        }
        else
            steps.push_back( step );
    }
    string path;
    for ( const string_view step : steps )
        path += ( path.empty() ? "" : "/" ) + string( step );
    path += ".luau";
    if ( auto validMaybe = AssetPath::TryFrom( path ); not validMaybe )
        return unexpected( validMaybe.error() );
    return path;
}
}
