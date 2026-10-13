// The scene: entities in an EnTT registry and their components by type.
#include "bubble/scene/scene.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/format.hpp"
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
    Scene scene;
    const Entity first = scene.Create();
    CHECK( scene.Alive( first ) );
    // It and the root.
    CHECK( scene.Count() == 2 );

    scene.Destroy( first );
    CHECK_FALSE( scene.Alive( first ) );
    CHECK( scene.Count() == 1 );
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

TEST_CASE( "Components are added, found, replaced and removed by type" )
{
    Scene scene;
    const Entity entity = scene.Create();
    CHECK_FALSE( scene.Has<Health>( entity ) );
    CHECK_FALSE( scene.TryGet<Health>( entity ) );
    // Get is for a component that has to be there.
    const string missing = format( "on entity {} (not in the tree), which has none", static_cast<u32>( entity ) );
    CHECK_THROWS_WITH_AS( scene.Get<Health>( entity ), doctest::Contains( missing.c_str() ), logic_error );

    scene.Add<Health>( entity ).mValue = 50;
    REQUIRE( scene.Has<Health>( entity ) );
    CHECK( scene.Get<Health>( entity ).mValue == 50 );

    // Add replaces what is there.
    scene.Add<Health>( entity, 75 );
    CHECK( scene.Get<Health>( entity ).mValue == 75 );

    const Scene& constScene = scene;
    CHECK( constScene.Get<Health>( entity ).mValue == 75 );

    scene.Remove<Health>( entity );
    CHECK_FALSE( scene.Has<Health>( entity ) );
}

TEST_CASE( "An entity that is gone has no components, and adding one is a bug" )
{
    Scene scene;
    const Entity entity = scene.Create();
    scene.Add<Health>( entity );
    scene.Destroy( entity );
    CHECK_FALSE( scene.Has<Health>( entity ) );
    CHECK_FALSE( scene.TryGet<Health>( entity ) );
    scene.Remove<Health>( entity );
    CHECK_THROWS_WITH_AS( scene.Add<Health>( entity ), doctest::Contains( ", which is gone" ), logic_error );
}

TEST_CASE( "ForEach visits the entities that have every component asked for" )
{
    Scene scene;
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
    CHECK( scene.Get<Health>( both ).mValue == 102 );

    int count = 0;
    const Scene& constScene = scene;
    constScene.ForEach<Health>( [&]( Entity, const Health& ) { ++count; } );
    CHECK( count == 2 );
    // A type no entity ever had.
    constScene.ForEach<double>( [&]( Entity, const double& ) { ++count; } );
    CHECK( count == 2 );
}
