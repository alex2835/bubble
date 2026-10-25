// The scene: entities in an EnTT registry and their components by type.
#include "bubble/scene/hierarchy.hpp"
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

TEST_CASE( "An entity that is gone comes back under its number, and a number in use does not" )
{
    Scene scene;
    const Entity first = scene.Create();
    scene.Add<Health>( first );
    scene.Destroy( first );

    scene.Revive( first );
    CHECK( scene.Alive( first ) );
    // With nothing on it.
    CHECK_FALSE( scene.Has<Health>( first ) );
    CHECK_THROWS_WITH_AS(
        scene.Revive( first ),
        doctest::Contains( format( "Revive of entity {}: its number is in use", static_cast<u32>( first ) ).c_str() ),
        logic_error );
    CHECK( scene.Count() == 2 );

    // Gone again, it can come back again.
    scene.Destroy( first );
    scene.Revive( first );
    CHECK( scene.Alive( first ) );
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

TEST_CASE( "A component put on during a walk does not join that walk" )
{
    Scene scene;
    const Entity first = scene.Create();
    const Entity second = scene.Create();
    scene.Add<Health>( first, 1 );

    vector<Entity> seen;
    scene.ForEach<Health>( [&]( Entity entity, Health& health ) {
        seen.push_back( entity );
        if ( health.mValue == 1 )
            scene.Add<Health>( second, 2 );
    } );
    // The walk was over what had the component when it started.
    CHECK( seen == vector<Entity>{ first } );
    CHECK( scene.Get<Health>( second ).mValue == 2 );
}

TEST_CASE( "Taking a component off one entity may move another's, but not change its value" )
{
    Scene scene;
    const Entity first = scene.Create();
    const Entity second = scene.Create();
    const Entity third = scene.Create();
    // Components of one type sit together; a removal fills the hole with the
    // last of them, so the address a Get gave is only good until the next
    // removal of that type - the Entity is what stays.
    scene.Add<Health>( first, 10 );
    scene.Add<Health>( second, 20 );
    scene.Add<Health>( third, 30 );

    scene.Remove<Health>( first );
    CHECK( scene.Get<Health>( second ).mValue == 20 );
    CHECK( scene.Get<Health>( third ).mValue == 30 );

    // Destroying an entity takes its components with it: revived, it has none,
    // and what is left of the type is where it was left.
    scene.Destroy( second );
    CHECK_FALSE( scene.Has<Health>( second ) );
    scene.Revive( second );
    CHECK( scene.Has<Health>( third ) );
    CHECK_FALSE( scene.Has<Health>( second ) );
    CHECK_FALSE( scene.Has<Health>( first ) );
}

TEST_CASE( "An entity is a number, so another scene can take the same one" )
{
    Scene first;
    const Entity hero = first.Create();
    Scene other;
    // Nothing links an Entity to the scene it came from: the number is free
    // over there, and Revive takes it as it is.
    CHECK_FALSE( other.Alive( hero ) );
    other.Revive( hero );
    CHECK( other.Alive( hero ) );
    CHECK( first.Alive( hero ) );
    // The root of one scene is the number of the root of the other.
    CHECK( other.Root() == first.Root() );
    CHECK_THROWS_WITH_AS( first.Revive( first.Root() ), "Revive of entity 0: its number is in use", logic_error );
}

TEST_CASE( "The root is not an entity to destroy, and a scene always has it" )
{
    Scene scene;
    CHECK_THROWS_WITH_AS( scene.Destroy( scene.Root() ), "Destroy on the root: it goes with the scene", logic_error );
    CHECK( scene.Alive( scene.Root() ) );
    CHECK( scene.Count() == 1 );
    // A component of the root is a component like any other.
    scene.Add<Health>( scene.Root(), 5 );
    CHECK( scene.Get<Health>( scene.Root() ).mValue == 5 );
    CHECK( scene.Count() == 1 );
    // The Hierarchy every entity in the tree carries is on the root from the
    // start.
    CHECK( scene.Has<Hierarchy>( scene.Root() ) );
}
