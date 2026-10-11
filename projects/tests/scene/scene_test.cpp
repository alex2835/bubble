// The scene: entities in an EnTT registry, each with a uid that lasts, and
// components by type.
#include "bubble/scene/scene.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Health
{
    int mValue = 100;
};

struct Speed
{
    float mValue = 1;
};
}

TEST_CASE( "An entity is alive until it is destroyed, and a stale one stays dead" )
{
    Scene scene( 1 );
    const Entity first = scene.Create();
    CHECK( scene.Alive( first ) );
    CHECK( scene.Count() == 1 );

    scene.Destroy( first );
    CHECK_FALSE( scene.Alive( first ) );
    CHECK( scene.Count() == 0 );
    // Destroying it again is nothing.
    scene.Destroy( first );

    // The next entity takes the freed index under a new version: the old
    // Entity does not find it.
    const Entity second = scene.Create();
    CHECK( scene.Alive( second ) );
    CHECK_FALSE( scene.Alive( first ) );
    CHECK( second != first );
    CHECK_FALSE( scene.Alive( Entity::Null ) );
}

TEST_CASE( "Every entity has its own uid, and the scene finds the entity by it" )
{
    Scene scene( 1 );
    const Entity a = scene.Create();
    const Entity b = scene.Create();
    const Uid aUid = *scene.UidOf( a );
    const Uid bUid = *scene.UidOf( b );
    CHECK( aUid );
    CHECK( aUid != bUid );
    CHECK( scene.Find( aUid ) == a );
    CHECK( scene.Find( bUid ) == b );

    scene.Destroy( a );
    CHECK_FALSE( scene.Find( aUid ) );
    CHECK_FALSE( scene.UidOf( a ) );
}

TEST_CASE( "The same seed gives the same uids" )
{
    Scene one( 7 );
    Scene two( 7 );
    for ( int i = 0; i < 3; ++i )
        CHECK( one.UidOf( one.Create() ) == two.UidOf( two.Create() ) );
    Scene seven( 7 );
    Scene eight( 8 );
    CHECK( seven.UidOf( seven.Create() ) != eight.UidOf( eight.Create() ) );
}

TEST_CASE( "An entity comes back under the uid it had, and a taken uid is refused" )
{
    Scene scene( 1 );
    const auto loaded = scene.Create( Uid{ 0x1234 } );
    REQUIRE( loaded );
    CHECK( scene.UidOf( *loaded ) == Uid{ 0x1234 } );
    CHECK( scene.Find( Uid{ 0x1234 } ) == *loaded );

    const auto again = scene.Create( Uid{ 0x1234 } );
    REQUIRE_FALSE( again );
    CHECK( again.error().starts_with( "uid 0000000000001234 is taken by entity " ) );
    CHECK( scene.Create( Uid{} ).error() == "an entity's uid is never 0" );
    CHECK( scene.Count() == 1 );
}

TEST_CASE( "A uid is written as 16 hex digits and read back" )
{
    const Uid uid{ 0x00ab'cdef'0123'4567 };
    CHECK( uid.ToString() == "00abcdef01234567" );
    CHECK( Uid::Parse( uid.ToString() ) == uid );
    CHECK_FALSE( Uid::Parse( "abcdef01234567" ) );
    CHECK_FALSE( Uid::Parse( "00abcdef0123456g" ) );
    CHECK_FALSE( Uid::Parse( "0000000000000000" ) );
}

TEST_CASE( "Components are added, found, replaced and removed by type" )
{
    Scene scene( 1 );
    const Entity entity = scene.Create();
    CHECK_FALSE( scene.Has<Health>( entity ) );
    CHECK_FALSE( scene.Get<Health>( entity ) );

    scene.Add<Health>( entity ).mValue = 50;
    REQUIRE( scene.Has<Health>( entity ) );
    CHECK( scene.Get<Health>( entity )->mValue == 50 );

    // Add replaces what is there.
    scene.Add<Health>( entity, 75 );
    CHECK( scene.Get<Health>( entity )->mValue == 75 );

    const Scene& constScene = scene;
    CHECK( constScene.Get<Health>( entity )->mValue == 75 );

    scene.Remove<Health>( entity );
    CHECK_FALSE( scene.Has<Health>( entity ) );
}

TEST_CASE( "An entity that is gone has no components, and adding one is a bug" )
{
    Scene scene( 1 );
    const Entity entity = scene.Create();
    scene.Add<Health>( entity );
    scene.Destroy( entity );
    CHECK_FALSE( scene.Has<Health>( entity ) );
    CHECK_FALSE( scene.Get<Health>( entity ) );
    scene.Remove<Health>( entity );
    CHECK_THROWS_WITH_AS( scene.Add<Health>( entity ), doctest::Contains( ", which is gone" ), logic_error );
}

TEST_CASE( "ForEach visits the entities that have every component asked for" )
{
    Scene scene( 1 );
    const Entity both = scene.Create();
    const Entity healthOnly = scene.Create();
    scene.Add<Health>( both );
    scene.Add<Speed>( both, 2.0f );
    scene.Add<Health>( healthOnly );

    vector<Entity> seen;
    scene.ForEach<Health, Speed>( [&]( Entity entity, Health& health, Speed& speed ) {
        seen.push_back( entity );
        health.mValue += static_cast<int>( speed.mValue );
    } );
    CHECK( seen == vector<Entity>{ both } );
    CHECK( scene.Get<Health>( both )->mValue == 102 );

    int count = 0;
    const Scene& constScene = scene;
    constScene.ForEach<Health>( [&]( Entity, const Health& ) { ++count; } );
    CHECK( count == 2 );
    // A type no entity ever had.
    constScene.ForEach<double>( [&]( Entity, const double& ) { ++count; } );
    CHECK( count == 2 );
}
