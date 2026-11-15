// A level's and a prefab's own id, kept inside the file.
#include "bubble/scene/engine_file.hpp"
#include "bubble/scene/scene_file.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
opt<AssetId> IdIn( string_view text )
{
    const auto idMaybe = TryReadFileId( text );
    REQUIRE_MESSAGE( idMaybe, idMaybe.error() );
    return *idMaybe;
}
}

TEST_CASE( "An engine file keeps its own id at the top, and one without is nothing yet" )
{
    const AssetId id = AssetId::Generate();
    const string withId = R"({ "version": 1, "asset": ")" + id.ToString() + R"(", "root": {} })";
    CHECK( IdIn( withId ) == opt<AssetId>( id ) );
    CHECK( IdIn( R"({ "version": 1, "root": {} })" ) == opt<AssetId>() );

    CHECK( TryReadFileId( "[]" ).error() == "1:1: an object expected, got an array of 0" );
    CHECK( TryReadFileId( R"({ "version": 1, "asset": "hero" })" ).error() ==
           R"(1:26: asset: an id - 32 hex digits, not all zeros - expected, got "hero")" );
    CHECK( TryReadFileId( R"({ "asset": ")" + string( 32, '0' ) + R"(" })" ).error().starts_with( "1:12: asset:" ) );
}

TEST_CASE( "An id given to an engine file goes after its version, the rest as it was" )
{
    const AssetId id = AssetId::Generate();
    const auto givenMaybe = TryWithFileId( R"({ "version": 1, "root": { "children": [] } })", id );
    REQUIRE( givenMaybe );
    CHECK( givenMaybe->find( R"("version": 1,)" ) < givenMaybe->find( R"("asset": ")" + id.ToString() ) );
    CHECK( givenMaybe->find( R"("asset": ")" + id.ToString() ) < givenMaybe->find( R"("root")" ) );

    // One that has an id gets the new one in its place.
    const AssetId other = AssetId::Generate();
    const auto againMaybe = TryWithFileId( *givenMaybe, other );
    REQUIRE( againMaybe );
    CHECK( IdIn( *againMaybe ) == opt<AssetId>( other ) );
    CHECK( againMaybe->find( id.ToString() ) == string::npos );
}

TEST_CASE( "A level is saved with its id, and loads with or without one" )
{
    Reflection reflection;
    const ComponentTypes componentTypes( reflection );
    const AssetCatalog assets;
    const FileContext files( componentTypes, assets );
    const AssetId id = AssetId::Generate();
    const auto savedMaybe = TrySaveScene( Scene(), files, id );
    REQUIRE( savedMaybe );
    CHECK( IdIn( *savedMaybe ) == opt<AssetId>( id ) );
    CHECK( TryLoadScene( *savedMaybe, files ) );
    // A zero id is not written.
    CHECK( IdIn( *TrySaveScene( Scene(), files ) ) == opt<AssetId>() );
    CHECK(
        TryLoadScene( R"({ "version": 1, "asset": "x", "root": {} })", files ).error().starts_with( "1:26: asset:" ) );
}
