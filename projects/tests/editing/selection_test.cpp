// What is selected in a document: in the order it was picked, the last one
// active; an entity that goes is not shown and comes back with an undo; the
// top of the selection in the tree's order.
#include "bubble/editing/edits.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Fixture
{
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    Document mDocument{ mComponentTypes };
    Selection& mSelection = mDocument.GetSelection();
    // /lamp, /props/chair, /props/table.
    Entity mLamp = Create( Root(), "lamp" );
    Entity mProps = Create( Root(), "props" );
    Entity mChair = Create( mProps, "chair" );
    Entity mTable = Create( mProps, "table" );

    Entity Root() const { return mDocument.GetScene().Root(); }

    Entity Create( Entity parent, string_view name )
    {
        const auto createdMaybe = TryCreateEntity( mDocument, parent, *Text::TryFrom( name ) );
        REQUIRE_MESSAGE( createdMaybe, createdMaybe.error() );
        return *createdMaybe;
    }
};
}

TEST_CASE( "A selection keeps the order things were picked in, the last one active" )
{
    Fixture f;
    CHECK( f.mSelection.Empty() );
    CHECK( f.mSelection.Active() == Entity::Null );

    f.mSelection.Select( f.mChair );
    f.mSelection.Add( f.mLamp );
    CHECK( f.mSelection.All() == vector<Entity>{ f.mChair, f.mLamp } );
    CHECK( f.mSelection.Active() == f.mLamp );

    // Picked again, it is active again.
    f.mSelection.Add( f.mChair );
    CHECK( f.mSelection.All() == vector<Entity>{ f.mLamp, f.mChair } );
    CHECK( f.mSelection.Active() == f.mChair );

    // Select starts over.
    f.mSelection.Select( f.mTable );
    CHECK( f.mSelection.All() == vector<Entity>{ f.mTable } );
}

TEST_CASE( "Taking the active one out leaves the one picked before it active" )
{
    Fixture f;
    f.mSelection.Select( f.mChair );
    f.mSelection.Add( f.mLamp );
    f.mSelection.Remove( f.mLamp );
    CHECK( f.mSelection.Active() == f.mChair );

    f.mSelection.Toggle( f.mTable );
    CHECK( f.mSelection.Has( f.mTable ) );
    CHECK( f.mSelection.Active() == f.mTable );
    f.mSelection.Toggle( f.mTable );
    CHECK_FALSE( f.mSelection.Has( f.mTable ) );

    f.mSelection.Clear();
    CHECK( f.mSelection.Empty() );
}

TEST_CASE( "An entity that goes is not selected, and an undo brings it back selected" )
{
    Fixture f;
    f.mSelection.Select( f.mLamp );
    f.mSelection.Add( f.mChair );
    REQUIRE( TryDestroyEntity( f.mDocument, f.mProps ) );
    CHECK( f.mSelection.All() == vector<Entity>{ f.mLamp } );
    CHECK( f.mSelection.Active() == f.mLamp );
    CHECK_FALSE( f.mSelection.Has( f.mChair ) );

    f.mDocument.GetHistory().Undo();
    CHECK( f.mSelection.All() == vector<Entity>{ f.mLamp, f.mChair } );
    CHECK( f.mSelection.Active() == f.mChair );
}

TEST_CASE( "The top of a selection leaves out what is under another selected, in the tree's order" )
{
    Fixture f;
    f.mSelection.Select( f.mTable );
    f.mSelection.Add( f.mLamp );
    f.mSelection.Add( f.mChair );
    CHECK( f.mSelection.TopLevel() == vector<Entity>{ f.mLamp, f.mChair, f.mTable } );

    f.mSelection.Add( f.mProps );
    CHECK( f.mSelection.TopLevel() == vector<Entity>{ f.mLamp, f.mProps } );
}

TEST_CASE( "Only an entity of the tree is selected" )
{
    Fixture f;
    REQUIRE( TryDestroyEntity( f.mDocument, f.mLamp ) );
    CHECK_THROWS_WITH_AS( f.mSelection.Select( f.mLamp ), doctest::Contains( "Selection::Select of entity" ),
                          logic_error );
    CHECK_THROWS_AS( f.mSelection.Add( Entity::Null ), logic_error );
}
