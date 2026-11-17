// Images imported as textures: RGBA8, mip levels averaged as light, the
// settings, and the texture's own bytes - then loaded by a registry, with
// no window anywhere.
#include "bubble/assets/asset_registry.hpp"
#include "bubble/assets/texture_asset.hpp"
#include "bubble/import/import_file.hpp"
#include "bubble/import/project_assets.hpp"
#include "bubble/import/texture_importer.hpp"
#include "bubble/scene/engine_file.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
// 2×2: red, green; blue, white at half alpha.
constexpr char cPng[] =
    "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52\x00\x00\x00\x02\x00\x00\x00\x02"
    "\x08\x06\x00\x00\x00\x72\xb6\x0d\x24\x00\x00\x00\x13\x49\x44\x41\x54\x78\x9c\x63\xf8\xcf\xc0\xf0"
    "\x1f\x0c\x81\x34\x08\x34\x00\x00\x49\x49\x09\x78\x28\xa0\xdb\x77\x00\x00\x00\x00\x49\x45\x4e\x44"
    "\xae\x42\x60\x82";
const string_view cPngBytes( cPng, sizeof cPng - 1 );

// 3×1, uncompressed 32-bit TGA, rows from the top: three grey pixels.
string Tga()
{
    string tga( 18, '\0' );
    tga[2] = 2;     // true color
    tga[12] = 3;    // width
    tga[14] = 1;    // height
    tga[16] = 32;   // bits a pixel
    tga[17] = 0x28; // 8 bits of alpha, rows from the top
    for ( const char grey : { '\x10', '\x20', '\x30' } )
        tga += string{ grey, grey, grey, '\xff' };
    return tga;
}

AssetPath Path( string_view text )
{
    return *AssetPath::TryFrom( text );
}

const Importer& PngImporter()
{
    static const vector<Importer> importers = TextureImporters();
    return importers.front();
}

expected<TextureAsset, string> TryImport( string_view source,
                                          string_view settings = "{}",
                                          string_view extension = ".png" )
{
    for ( const Importer& importer : TextureImporters() )
        if ( importer.mExtension == extension )
        {
            auto bytesMaybe = importer.mImport( source, *Json::TryParse( settings ), Path( "textures/a.png" ) );
            if ( not bytesMaybe )
                return unexpected( std::move( bytesMaybe.error() ) );
            return TryReadTextureAsset( bytesMaybe->mBytes );
        }
    return unexpected( "no importer"s );
}
}

TEST_CASE( "An image is imported as RGBA8 with its mip levels down to 1x1, averaged as light" )
{
    const auto textureMaybe = TryImport( cPngBytes );
    REQUIRE_MESSAGE( textureMaybe, textureMaybe.error() );
    CHECK( textureMaybe->mSrgb );
    REQUIRE( textureMaybe->mMips.size() == 2 );
    const TextureMip& full = textureMaybe->mMips[0];
    CHECK( full.mWidth == 2 );
    CHECK( full.mHeight == 2 );
    CHECK( full.mPixels == vector<u8>{ 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 128 } );
    // Half the light of full red is not half its stored value: 188, not 128.
    // Alpha is no light: averaged as stored.
    CHECK( textureMaybe->mMips[1].mPixels == vector<u8>{ 188, 188, 188, 223 } );

    // Data, averaged as stored; and no levels below the full size.
    const auto dataMaybe = TryImport( cPngBytes, R"({ "srgb": false })" );
    REQUIRE( dataMaybe );
    CHECK_FALSE( dataMaybe->mSrgb );
    CHECK( dataMaybe->mMips[1].mPixels == vector<u8>{ 128, 128, 128, 223 } );
    CHECK( TryImport( cPngBytes, R"({ "mips": false })" )->mMips.size() == 1 );

    // A TGA, three wide: the levels halve, an odd edge dropped.
    const auto tgaMaybe = TryImport( Tga(), "{}", ".tga" );
    REQUIRE_MESSAGE( tgaMaybe, tgaMaybe.error() );
    REQUIRE( tgaMaybe->mMips.size() == 2 );
    CHECK( tgaMaybe->mMips[0].mWidth == 3 );
    CHECK( tgaMaybe->mMips[0].mPixels[4] == 0x20 );
    CHECK( tgaMaybe->mMips[1].mWidth == 1 );
}

TEST_CASE( "An image that will not decode, and settings that are wrong, say why" )
{
    CHECK( TryImport( "not an image" ).error().starts_with( "textures/a.png: " ) );
    CHECK( TryImport( cPngBytes, R"({ "sharpen": true })" )
               .error()
               .ends_with( "settings.sharpen: not a field (fields: srgb, mips)" ) );
    CHECK( TryImport( cPngBytes, R"({ "srgb": 1 })" ).error().ends_with( "settings.srgb: a bool expected, got 1" ) );
    CHECK( PngImporter().mKind == cTextureKind );
}

TEST_CASE( "A texture's bytes read back as written, and broken ones say what is wrong" )
{
    const TextureAsset texture{ .mSrgb = false,
                                .mMips = { TextureMip{ .mWidth = 1, .mHeight = 1, .mPixels = { 1, 2, 3, 4 } } } };
    const string bytes = WriteTextureAsset( texture );
    const auto readMaybe = TryReadTextureAsset( bytes );
    REQUIRE( readMaybe );
    CHECK_FALSE( readMaybe->mSrgb );
    CHECK( readMaybe->mMips[0].mPixels == texture.mMips[0].mPixels );

    CHECK( TryReadTextureAsset( bytes.substr( 0, bytes.size() - 1 ) ).error() == "a texture cut short" );
    CHECK( TryReadTextureAsset( bytes + "x" ).error() == "a texture with more after its last level" );
    CHECK( TryReadTextureAsset( "PNG!" ).error() == "not a texture the importer made" );
}

TEST_CASE( "A project's image is imported and loaded with no window" )
{
    hmap<string, string> files{ { "textures/brick.png", string( cPngBytes ) } };
    Importers importers;
    for ( Importer& importer : TextureImporters() )
        importers.Add( std::move( importer ) );
    ProjectAssets project( ProjectFiles::InMemory( files ), std::move( importers ), cEngineFileExtensions );
    const auto refreshMaybe = project.TryRefresh();
    REQUIRE_MESSAGE( refreshMaybe, refreshMaybe.error() );
    CHECK( refreshMaybe->mImported == vector<AssetPath>{ Path( "textures/brick.png" ) } );

    Jobs jobs( 1 );
    AssetRegistry assets( project.GetCatalog(), AssetRegistry::AtOnce( [&]( const AssetId& assetId, string_view kind ) {
                              return project.GetFiles().TryRead( ImportedPath( assetId, kind ) );
                          } ),
                          jobs );
    RegisterTextureLoader( assets );
    const auto brick = assets.LoadNow<TextureAsset>( Path( "textures/brick.png" ) );
    REQUIRE_MESSAGE( brick.Ready(), brick.Entry().Error() );
    CHECK( brick.TryGet()->mMips.size() == 2 );
}
