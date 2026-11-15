// AssetCatalog: which asset is at which path, both ways.
#include "bubble/assets/asset_catalog.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "The catalog turns an id into a path and a path into an id" )
{
    AssetCatalog catalog;
    const AssetId hero = AssetId::Generate();
    const AssetPath heroPath = *AssetPath::TryFrom( "models/hero.glb" );
    catalog.Add( hero, heroPath );
    REQUIRE( catalog.TryPathOf( hero ) );
    CHECK( *catalog.TryPathOf( hero ) == heroPath );
    CHECK( catalog.TryIdOf( heroPath ) == hero );
    CHECK( catalog.Count() == 1 );
    CHECK_FALSE( catalog.TryPathOf( AssetId::Generate() ) );
    CHECK_FALSE( catalog.TryIdOf( *AssetPath::TryFrom( "models/villain.glb" ) ) );

    // One id, one path: a second of either is a bug.
    CHECK_THROWS_AS( catalog.Add( hero, *AssetPath::TryFrom( "models/copy.glb" ) ), logic_error );
    CHECK_THROWS_AS( catalog.Add( AssetId::Generate(), heroPath ), logic_error );

    catalog.Remove( hero );
    catalog.Remove( hero );
    CHECK_FALSE( catalog.TryPathOf( hero ) );
    CHECK_FALSE( catalog.TryIdOf( heroPath ) );
    CHECK( catalog.Count() == 0 );
}
