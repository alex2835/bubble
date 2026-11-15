#include "bubble/scene/engine_file.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
expected<Json, string> TryParseObject( string_view text )
{
    auto jsonMaybe = Json::TryParse( text );
    if ( not jsonMaybe )
        return jsonMaybe;
    if ( not jsonMaybe->Is<Json::Object>() )
        return unexpected( format( "1:1: an object expected, got {}", jsonMaybe->Describe() ) );
    return jsonMaybe;
}
}

expected<opt<AssetId>, string> TryReadFileId( string_view text )
{
    const auto jsonMaybe = TryParseObject( text );
    if ( not jsonMaybe )
        return unexpected( jsonMaybe.error() );
    const auto idJsonMaybe = jsonMaybe->TryFind( "asset" );
    if ( not idJsonMaybe )
        return opt<AssetId>();
    const auto textMaybe = idJsonMaybe->TryAs<string>();
    const auto assetIdMaybe = textMaybe ? AssetId::TryParse( *textMaybe ) : nullopt;
    if ( not assetIdMaybe or not *assetIdMaybe )
        return unexpected( format( "{}: asset: an id - 32 hex digits, not all zeros - expected, got {}",
                                   idJsonMaybe->Where(), idJsonMaybe->Describe() ) );
    return assetIdMaybe;
}

expected<string, string> TryWithFileId( string_view text, const AssetId& assetId )
{
    const auto jsonMaybe = TryParseObject( text );
    if ( not jsonMaybe )
        return unexpected( jsonMaybe.error() );
    Json::Object file;
    bool written = false;
    for ( const Json::Member& member : *jsonMaybe->TryAs<Json::Object>() )
    {
        if ( member.mKey == "asset" )
            continue;
        file.push_back( member );
        // Right after the version, where a save puts it.
        if ( member.mKey == "version" )
        {
            file.push_back( Json::Member{ "asset", Json( assetId.ToString() ) } );
            written = true;
        }
    }
    if ( not written )
        file.insert( file.begin(), Json::Member{ "asset", Json( assetId.ToString() ) } );
    return Json( std::move( file ) ).Write();
}

void WriteFileHead( Json::Object& file, int version, const AssetId& assetId )
{
    file.push_back( Json::Member{ "version", Json( static_cast<f64>( version ) ) } );
    if ( assetId )
        file.push_back( Json::Member{ "asset", Json( assetId.ToString() ) } );
}
}
