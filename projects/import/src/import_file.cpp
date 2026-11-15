#include "bubble/import/import_file.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
unexpected<string> Fail( const Json& json, string_view what )
{
    return unexpected( format( "{}: {}", json.Where(), what ) );
}
}

expected<ImportFile, string> TryParseImportFile( string_view text )
{
    auto jsonMaybe = Json::TryParse( text );
    if ( not jsonMaybe )
        return unexpected( std::move( jsonMaybe.error() ) );
    const Json& json = *jsonMaybe;
    const auto membersMaybe = json.TryAs<Json::Object>();
    if ( not membersMaybe )
        return Fail( json, format( "an object expected, got {}", json.Describe() ) );
    for ( const Json::Member& member : *membersMaybe )
        if ( member.mKey != "version" and member.mKey != "asset" and member.mKey != "settings" )
            return Fail( member.mValue,
                         format( "{} is not a key of an import file (keys: version, asset, settings)", member.mKey ) );

    const auto versionMaybe = json.TryFind( "version" );
    if ( not versionMaybe )
        return unexpected( format( "no version: an import file says \"version\": {}", ImportFile::cVersion ) );
    const auto numberMaybe = versionMaybe->TryAs<f64>();
    if ( not numberMaybe or *numberMaybe != static_cast<f64>( ImportFile::cVersion ) )
        return Fail( *versionMaybe, format( "version: {} is not a version this engine reads ({})",
                                            versionMaybe->Describe(), ImportFile::cVersion ) );

    ImportFile file;
    const auto assetMaybe = json.TryFind( "asset" );
    if ( not assetMaybe )
        return unexpected( "no asset: an import file says \"asset\": \"<32 hex digits>\""s );
    const auto textMaybe = assetMaybe->TryAs<string>();
    const auto assetIdMaybe = textMaybe ? AssetId::TryParse( *textMaybe ) : nullopt;
    if ( not assetIdMaybe or not *assetIdMaybe )
        return Fail( *assetMaybe, format( "asset: an id - 32 hex digits, not all zeros - expected, got {}",
                                          assetMaybe->Describe() ) );
    file.mId = *assetIdMaybe;

    if ( const auto settingsMaybe = json.TryFind( "settings" ) )
    {
        if ( not settingsMaybe->Is<Json::Object>() )
            return Fail( *settingsMaybe, format( "settings: an object expected, got {}", settingsMaybe->Describe() ) );
        file.mSettings = *settingsMaybe;
    }
    return file;
}

string WriteImportFile( const ImportFile& file )
{
    Json::Object members;
    members.push_back( Json::Member{ "version", Json( static_cast<f64>( ImportFile::cVersion ) ) } );
    members.push_back( Json::Member{ "asset", Json( file.mId.ToString() ) } );
    members.push_back( Json::Member{ "settings", file.mSettings } );
    return Json( std::move( members ) ).Write();
}

AssetPath ImportFileOf( const AssetPath& source )
{
    return *AssetPath::TryFrom( source.String() + string( cImportExtension ) );
}

opt<AssetPath> TrySourceOf( const AssetPath& importFile )
{
    const string_view path = importFile.View();
    if ( importFile.Extension() != cImportExtension )
        return nullopt;
    auto sourceMaybe = AssetPath::TryFrom( path.substr( 0, path.size() - cImportExtension.size() ) );
    if ( not sourceMaybe )
        return nullopt;
    return std::move( *sourceMaybe );
}
}
