// The scene's tree: names unique among siblings, paths that find an entity
// a step at a time, and moves, renames and removals that keep both sides of
// every link in step.
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/scene.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
Text Make( string_view utf8 )
{
    return *Text::From( utf8 );
}

EntityPath Path( string_view text )
{
    return EntityPath{ Make( text ) };
}

string PathText( const Scene& scene, Entity entity )
{
    return PathOf( scene, entity ).mPath.String();
}

// /props/chair, /props/table, /player.
struct Fixture
{
    Scene mScene;
    Entity mProps = CreateChild( mScene, mScene.Root(), Make( "props" ) );
    Entity mChair = CreateChild( mScene, mProps, Make( "chair" ) );
    Entity mTable = CreateChild( mScene, mProps, Make( "table" ) );
    Entity mPlayer = CreateChild( mScene, mScene.Root(), Make( "player" ) );

    Entity Find( string_view path, Entity from = Entity::Null ) const
    {
        const auto found = Resolve( mScene, Path( path ), from );
        REQUIRE_MESSAGE( found, found.error() );
        return *found;
    }

    string Why( string_view path, Entity from = Entity::Null ) const
    {
        const auto found = Resolve( mScene, Path( path ), from );
        REQUIRE_FALSE( found );
        return found.error();
    }
};
}

TEST_CASE( "A scene starts with a root, which has no name and no parent" )
{
    Scene scene;
    CHECK( InTree( scene, scene.Root() ) );
    CHECK( ParentOf( scene, scene.Root() ) == Entity::Null );
    CHECK( NameOf( scene, scene.Root() ).Empty() );
    CHECK( PathText( scene, scene.Root() ) == "/" );
    CHECK_THROWS_AS( scene.Destroy( scene.Root() ), logic_error );

    // Made by Create, an entity is outside the tree: no path.
    const Entity loose = scene.Create();
    CHECK_FALSE( InTree( scene, loose ) );
    CHECK( PathOf( scene, loose ).mPath.Empty() );
    CHECK( Describe( scene, loose ) == format( "entity {} (not in the tree)", static_cast<u32>( loose ) ) );
}

TEST_CASE( "A child's name is made fit and unique among its siblings" )
{
    Scene scene;
    const Entity root = scene.Root();
    const auto named = [&]( string_view name ) { return NameOf( scene, CreateChild( scene, root, Make( name ) ) ); };

    CHECK( named( "chair" ) == Make( "chair" ) );
    CHECK( named( "chair" ) == Make( "chair_2" ) );
    CHECK( named( "chair" ) == Make( "chair_3" ) );
    // A number already there counts on.
    CHECK( named( "chair_2" ) == Make( "chair_4" ) );
    // Not a number to count on: a leading zero, or a 1.
    CHECK( named( "door_01" ) == Make( "door_01" ) );
    CHECK( named( "door_01" ) == Make( "door_01_2" ) );
    CHECK( named( "lamp_1" ) == Make( "lamp_1" ) );
    CHECK( named( "lamp_1" ) == Make( "lamp_1_2" ) );

    CHECK( named( "a/b" ) == Make( "a_b" ) );
    CHECK( named( "" ) == Make( "entity" ) );
    CHECK( named( ".." ) == Make( "entity_2" ) );
    CHECK( named( "Ð¡Ñ‚ÑƒÐ»" ) == Make( "Ð¡Ñ‚ÑƒÐ»" ) );
    CHECK( named( "Ð¡Ñ‚ÑƒÐ»" ) == Make( "Ð¡Ñ‚ÑƒÐ»_2" ) );

    CHECK( ChildrenOf( scene, root ).size() == 13 );
}

TEST_CASE( "Children keep the order they are put in" )
{
    Scene scene;
    const Entity b = CreateChild( scene, scene.Root(), Make( "b" ) );
    const Entity c = CreateChild( scene, scene.Root(), Make( "c" ) );
    const Entity a = CreateChild( scene, scene.Root(), Make( "a" ), 0 );
    const auto children = ChildrenOf( scene, scene.Root() );
    CHECK( vector<Entity>( children.begin(), children.end() ) == vector<Entity>{ a, b, c } );
}

TEST_CASE( "A path finds an entity a step at a time" )
{
    Fixture f;
    CHECK( f.Find( "/" ) == f.mScene.Root() );
    CHECK( f.Find( "/props/chair" ) == f.mChair );
    CHECK( f.Find( "chair", f.mProps ) == f.mChair );
    CHECK( f.Find( "../table", f.mChair ) == f.mTable );
    CHECK( f.Find( "../../player", f.mChair ) == f.mPlayer );
    CHECK( f.Find( ".", f.mChair ) == f.mChair );
    CHECK( f.Find( "./chair/..", f.mProps ) == f.mProps );

    // Names are Text in NFC: "Ã©" typed as e and an accent finds "Ã©".
    const Entity cafe = CreateChild( f.mScene, f.mScene.Root(), Make( "caf\xC3\xA9" ) );
    CHECK( f.Find( "/cafe\xCC\x81" ) == cafe );
}

TEST_CASE( "A path that leads nowhere says where it stopped and what is there" )
{
    Fixture f;
    CHECK( f.Why( "/props/chiar" ) == "/props/chiar: /props has no child 'chiar' (children: 'chair', 'table')" );
    CHECK( f.Why( "/props/chair/leg" ) == "/props/chair/leg: /props/chair has no child 'leg' (it has no children)" );
    CHECK( f.Why( "/.." ) == "/..: / has no parent" );
    CHECK( f.Why( "/props//chair" ) == "/props//chair: an empty name between slashes" );
    CHECK( f.Why( "/props/" ) == "/props/: an empty name between slashes" );
    CHECK( f.Why( "chair" ) == "chair: a relative path, from no entity" );
    CHECK( Resolve( f.mScene, EntityPath{} ).error() == "an empty path" );

    for ( int i = 0; i < 20; ++i )
        CreateChild( f.mScene, f.mPlayer, Make( "item" ) );
    CHECK( f.Why( "/player/sword" ) ==
           "/player/sword: /player has no child 'sword' (children: 'item', 'item_2', 'item_3', 'item_4', 'item_5', "
           "'item_6', 'item_7', 'item_8', 'item_9', 'item_10', 'item_11', 'item_12' and 8 more)" );
}

TEST_CASE( "A path made from an entity leads back to it" )
{
    Fixture f;
    CHECK( PathText( f.mScene, f.mChair ) == "/props/chair" );
    CHECK( RelativePath( f.mScene, f.mChair, f.mTable ).mPath == Make( "../table" ) );
    CHECK( RelativePath( f.mScene, f.mChair, f.mPlayer ).mPath == Make( "../../player" ) );
    CHECK( RelativePath( f.mScene, f.mProps, f.mChair ).mPath == Make( "chair" ) );
    CHECK( RelativePath( f.mScene, f.mChair, f.mChair ).mPath == Make( "." ) );

    for ( const Entity from : { f.mScene.Root(), f.mProps, f.mChair, f.mTable, f.mPlayer } )
        for ( const Entity to : { f.mScene.Root(), f.mProps, f.mChair, f.mTable, f.mPlayer } )
            CHECK( f.Find( RelativePath( f.mScene, from, to ).mPath.View(), from ) == to );
}

TEST_CASE( "A rename changes the name the parent knows it by, and keeps it unique" )
{
    Fixture f;
    CHECK( Rename( f.mScene, f.mTable, Make( "desk" ) ) == Make( "desk" ) );
    CHECK( f.Find( "/props/desk" ) == f.mTable );
    CHECK_FALSE( Resolve( f.mScene, Path( "/props/table" ) ) );

    CHECK( Rename( f.mScene, f.mTable, Make( "chair" ) ) == Make( "chair_2" ) );
    // Its own name is not taken by itself.
    CHECK( Rename( f.mScene, f.mTable, Make( "chair_2" ) ) == Make( "chair_2" ) );
    // The order stays.
    CHECK( ChildrenOf( f.mScene, f.mProps )[1] == f.mTable );

    CHECK_THROWS_AS( Rename( f.mScene, f.mScene.Root(), Make( "world" ) ), logic_error );
}

TEST_CASE( "SetParent moves an entity with what is under it, and refuses a loop" )
{
    Fixture f;
    const Entity leg = CreateChild( f.mScene, f.mChair, Make( "leg" ) );
    REQUIRE( SetParent( f.mScene, f.mChair, f.mPlayer ) );
    CHECK( ParentOf( f.mScene, f.mChair ) == f.mPlayer );
    CHECK( PathText( f.mScene, leg ) == "/player/chair/leg" );
    CHECK( ChildrenOf( f.mScene, f.mProps ).size() == 1 );
    CHECK_FALSE( Resolve( f.mScene, Path( "/props/chair" ) ) );

    // Into a parent with a child of that name: renamed.
    CreateChild( f.mScene, f.mProps, Make( "chair" ) );
    REQUIRE( SetParent( f.mScene, f.mChair, f.mProps, 0 ) );
    CHECK( NameOf( f.mScene, f.mChair ) == Make( "chair_2" ) );
    CHECK( ChildrenOf( f.mScene, f.mProps )[0] == f.mChair );

    // Within one parent: a change of place.
    REQUIRE( SetParent( f.mScene, f.mChair, f.mProps ) );
    CHECK( ChildrenOf( f.mScene, f.mProps ).back() == f.mChair );
    CHECK( NameOf( f.mScene, f.mChair ) == Make( "chair_2" ) );

    CHECK( SetParent( f.mScene, f.mProps, leg ).error() ==
           "/props cannot go under /props/chair_2/leg: that is inside it" );
    CHECK( SetParent( f.mScene, f.mProps, f.mProps ).error() == "/props cannot go under /props: that is inside it" );
    CHECK( SetParent( f.mScene, f.mScene.Root(), f.mProps ).error() ==
           "/ cannot go under /props: the root stays the root" );
    const Entity loose = f.mScene.Create();
    CHECK( SetParent( f.mScene, loose, f.mProps ).error() ==
           format( "entity {} (not in the tree) cannot go under /props: it is not in the tree",
                   static_cast<u32>( loose ) ) );
}

TEST_CASE( "Destroying an entity takes what is under it and its place in the parent" )
{
    Fixture f;
    const Entity leg = CreateChild( f.mScene, f.mChair, Make( "leg" ) );
    const size_t before = f.mScene.Count();
    f.mScene.Destroy( f.mChair );
    CHECK_FALSE( f.mScene.Alive( f.mChair ) );
    CHECK_FALSE( f.mScene.Alive( leg ) );
    CHECK( f.mScene.Count() == before - 2 );
    CHECK( ChildrenOf( f.mScene, f.mProps ).size() == 1 );
    CHECK( f.Why( "/props/chair" ) == "/props/chair: /props has no child 'chair' (children: 'table')" );
    CHECK( Describe( f.mScene, leg ) == format( "entity {} (gone)", static_cast<u32>( leg ) ) );

    // The name is free again.
    CHECK( NameOf( f.mScene, CreateChild( f.mScene, f.mProps, Make( "chair" ) ) ) == Make( "chair" ) );
}
