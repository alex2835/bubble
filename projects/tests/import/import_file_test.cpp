// The .import beside a source file: its asset's id and its settings.
#include "bubble/import/import_file.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "An import file is written and read back, settings and all" )
{
    ImportFile file;
    file.mId = AssetId::Generate();
    file.mSettings = *Json::TryParse( R"({ "lods": [0.5, 0.25] })" );
    const string text = WriteImportFile( file );
    const auto readMaybe = TryParseImportFile( text );
    REQUIRE( readMaybe );
    CHECK( readMaybe->mId == file.mId );
    CHECK( readMaybe->mSettings.Write() == file.mSettings.Write() );

    // Settings may be left out: an importer takes its defaults.
    const auto bareMaybe = TryParseImportFile( R"({ "version": 1, "asset": ")" + file.mId.ToString() + R"(" })" );
    REQUIRE( bareMaybe );
    CHECK( bareMaybe->mSettings.Write() == Json( Json::Object() ).Write() );
}

TEST_CASE( "An import file that cannot be read says where and what is wrong" )
{
    const string id = AssetId::Generate().ToString();
    const auto error = [&]( const string& text ) { return TryParseImportFile( text ).error(); };
    CHECK( error( "[]" ) == "1:1: an object expected, got an array of 0" );
    CHECK( error( R"({ "asset": ")" + id + R"(" })" ) == R"(no version: an import file says "version": 1)" );
    CHECK( error( R"({ "version": 2, "asset": ")" + id + R"(" })" ) ==
           "1:14: version: 2 is not a version this engine reads (1)" );
    CHECK( error( R"({ "version": 1 })" ) == R"(no asset: an import file says "asset": "<32 hex digits>")" );
    CHECK( error( R"({ "version": 1, "asset": "hero" })" ) ==
           R"(1:26: asset: an id - 32 hex digits, not all zeros - expected, got "hero")" );
    CHECK(
        error( R"({ "version": 1, "asset": ")" + string( 32, '0' ) + R"(" })" ).starts_with( "1:26: asset: an id" ) );
    CHECK( error( R"({ "version": 1, "asset": ")" + id + R"(", "settings": 3 })" ) ==
           "1:74: settings: an object expected, got 3" );
    CHECK( error( R"({ "version": 1, "asset": ")" + id + R"(", "guid": 3 })" ) ==
           "1:70: guid is not a key of an import file (keys: version, asset, settings)" );
}

TEST_CASE( "An import file lies beside its source, named after it" )
{
    const AssetPath source = *AssetPath::TryFrom( "models/hero.glb" );
    CHECK( ImportFileOf( source ).View() == "models/hero.glb.import" );
    CHECK( TrySourceOf( ImportFileOf( source ) ) == source );
    CHECK_FALSE( TrySourceOf( source ) );
}
