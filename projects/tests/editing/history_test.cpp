// A document's undo and redo: steps, groups that are one step, a group
// cancelled, and the position that tells a saved state from a changed one.
#include "bubble/editing/history.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
// Adds to a number outside the scene; refuses to add 0.
class AddCommand final : public Command
{
public:
    AddCommand( i32& total, i32 amount ) : mTotal( total ), mAmount( amount ) {}

    expected<void, string> TryDo( Scene& ) override
    {
        if ( mAmount == 0 )
            return unexpected( "nothing to add"s );
        mTotal += mAmount;
        return {};
    }

    void Undo( Scene& ) override { mTotal -= mAmount; }

private:
    i32& mTotal;
    i32 mAmount;
};

// Writes what it does into a `log`, so the order commands go back is seen, and
// folds into the command before it the way a field set while a gizmo is dragged.
class MarkCommand final : public Command
{
public:
    MarkCommand( vector<string>& log, string target, string note )
        : mLog( log ),
          mTarget( std::move( target ) ),
          mNote( std::move( note ) )
    {
    }

    expected<void, string> TryDo( Scene& ) override
    {
        if ( mNote.empty() )
            return unexpected( "nothing to mark"s );
        mLog.push_back( "do " + mTarget + " " + mNote );
        return {};
    }

    void Undo( Scene& ) override { mLog.push_back( "undo " + mTarget + " " + mNote ); }
    void Redo( Scene& ) override { mLog.push_back( "redo " + mTarget + " " + mNote ); }

    bool Absorb( const Command& next ) override
    {
        const auto* mark = dynamic_cast<const MarkCommand*>( &next );
        if ( mark == nullptr or mark->mTarget != mTarget )
            return false;
        mNote = mark->mNote;
        return true;
    }

private:
    vector<string>& mLog;
    string mTarget;
    string mNote;
};

void Require( const expected<void, string>& doneMaybe )
{
    REQUIRE_MESSAGE( doneMaybe, doneMaybe.error() );
}

struct Fixture
{
    Scene mScene;
    History mHistory{ mScene };
    i32 mTotal = 0;

    void Add( i32 amount )
    {
        const auto doneMaybe = mHistory.TryDo( CreateScope<AddCommand>( mTotal, amount ), format( "add {}", amount ) );
        REQUIRE_MESSAGE( doneMaybe, doneMaybe.error() );
    }
};
}

TEST_CASE( "A step undone comes back on redo, and a new step forgets what was undone" )
{
    Fixture f;
    f.Add( 1 );
    f.Add( 2 );
    CHECK( f.mTotal == 3 );
    CHECK( f.mHistory.UndoLabel() == "add 2" );

    f.mHistory.Undo();
    CHECK( f.mTotal == 1 );
    CHECK( f.mHistory.RedoLabel() == "add 2" );
    f.mHistory.Redo();
    CHECK( f.mTotal == 3 );

    f.mHistory.Undo();
    f.Add( 10 );
    CHECK( f.mTotal == 11 );
    CHECK_FALSE( f.mHistory.CanRedo() );

    f.mHistory.Undo();
    f.mHistory.Undo();
    CHECK( f.mTotal == 0 );
    CHECK_FALSE( f.mHistory.CanUndo() );
    // Nothing more to undo is nothing done.
    f.mHistory.Undo();
    CHECK( f.mTotal == 0 );
}

TEST_CASE( "A command that cannot be done changes nothing and is not kept" )
{
    Fixture f;
    f.Add( 1 );
    f.mHistory.Undo();
    const auto doneMaybe = f.mHistory.TryDo( CreateScope<AddCommand>( f.mTotal, 0 ), "add 0" );
    REQUIRE_FALSE( doneMaybe );
    CHECK( doneMaybe.error() == "nothing to add" );
    CHECK( f.mHistory.UndoLabel().empty() );
    // Nor did it forget what was undone.
    CHECK( f.mHistory.RedoLabel() == "add 1" );
}

TEST_CASE( "A group is one step, undone and redone whole" )
{
    Fixture f;
    f.mHistory.BeginGroup( "add three" );
    f.Add( 1 );
    f.Add( 2 );
    f.Add( 3 );
    CHECK_FALSE( f.mHistory.CanUndo() );
    f.mHistory.EndGroup();
    CHECK( f.mTotal == 6 );
    CHECK( f.mHistory.UndoLabel() == "add three" );

    f.mHistory.Undo();
    CHECK( f.mTotal == 0 );
    CHECK_FALSE( f.mHistory.CanUndo() );
    f.mHistory.Redo();
    CHECK( f.mTotal == 6 );
}

TEST_CASE( "A group nothing was done in leaves no step" )
{
    Fixture f;
    f.Add( 1 );
    f.mHistory.Undo();
    f.mHistory.BeginGroup( "nothing" );
    f.mHistory.EndGroup();
    CHECK_FALSE( f.mHistory.CanUndo() );
    CHECK( f.mHistory.CanRedo() );
}

TEST_CASE( "A cancelled group puts everything back and keeps what can be redone" )
{
    Fixture f;
    f.Add( 1 );
    f.Add( 2 );
    f.mHistory.Undo();
    const u64 before = f.mHistory.Position();

    f.mHistory.BeginGroup( "half done" );
    f.Add( 10 );
    f.Add( 20 );
    CHECK( f.mHistory.Position() != before );
    f.mHistory.CancelGroup();

    CHECK( f.mTotal == 1 );
    CHECK_FALSE( f.mHistory.InGroup() );
    CHECK( f.mHistory.Position() == before );
    f.mHistory.Redo();
    CHECK( f.mTotal == 3 );
}

TEST_CASE( "Undo and redo inside a group, and closing a group not open, are logic_errors" )
{
    Fixture f;
    f.mHistory.BeginGroup( "drag" );
    f.Add( 1 );
    CHECK_THROWS_WITH_AS( f.mHistory.Undo(), "Undo inside the group drag, which is not a step yet", logic_error );
    CHECK_THROWS_AS( f.mHistory.Redo(), logic_error );
    f.mHistory.EndGroup();
    CHECK_THROWS_WITH_AS( f.mHistory.EndGroup(), "EndGroup with no group open", logic_error );
    CHECK_THROWS_AS( f.mHistory.CancelGroup(), logic_error );
}

TEST_CASE( "A group inside a group is part of its step, and its cancel takes back only what it did" )
{
    Fixture f;
    f.mHistory.BeginGroup( "outer" );
    f.Add( 1 );
    f.mHistory.BeginGroup( "inner" );
    f.Add( 10 );
    f.mHistory.EndGroup();
    CHECK( f.mHistory.InGroup() );
    CHECK_FALSE( f.mHistory.CanUndo() );

    f.mHistory.BeginGroup( "failed" );
    f.Add( 100 );
    f.mHistory.CancelGroup();
    CHECK( f.mTotal == 11 );
    CHECK( f.mHistory.InGroup() );
    f.mHistory.EndGroup();

    CHECK( f.mHistory.UndoLabel() == "outer" );
    f.mHistory.Undo();
    CHECK( f.mTotal == 0 );
    f.mHistory.Redo();
    CHECK( f.mTotal == 11 );
}

TEST_CASE( "The position comes back with the state, and a new step is a new position" )
{
    Fixture f;
    CHECK( f.mHistory.Position() == 0 );
    f.Add( 1 );
    const u64 one = f.mHistory.Position();
    f.Add( 2 );
    const u64 two = f.mHistory.Position();
    CHECK( one != two );

    f.mHistory.Undo();
    CHECK( f.mHistory.Position() == one );
    f.mHistory.Redo();
    CHECK( f.mHistory.Position() == two );

    // The same total by another way is another state of the history.
    f.mHistory.Undo();
    f.Add( 2 );
    CHECK( f.mHistory.Position() != two );
    f.mHistory.Undo();
    f.mHistory.Undo();
    CHECK( f.mHistory.Position() == 0 );
}

TEST_CASE( "The oldest steps go past the limit" )
{
    Fixture f;
    f.mHistory.SetMaxSteps( 2 );
    f.Add( 1 );
    f.Add( 2 );
    f.Add( 3 );
    f.mHistory.Undo();
    f.mHistory.Undo();
    CHECK_FALSE( f.mHistory.CanUndo() );
    CHECK( f.mTotal == 1 );

    f.mHistory.Redo();
    f.mHistory.Redo();
    f.mHistory.SetMaxSteps( 1 );
    CHECK( f.mHistory.UndoLabel() == "add 3" );
    f.mHistory.Undo();
    CHECK_FALSE( f.mHistory.CanUndo() );
    CHECK_THROWS_AS( f.mHistory.SetMaxSteps( 0 ), logic_error );
}

TEST_CASE( "A step of several commands goes back in the order they came last" )
{
    vector<string> log;
    Fixture f;
    f.mHistory.BeginGroup( "paint" );
    Require( f.mHistory.TryDo( CreateScope<MarkCommand>( log, "a", "1" ), "mark a" ) );
    Require( f.mHistory.TryDo( CreateScope<MarkCommand>( log, "b", "2" ), "mark b" ) );
    f.mHistory.EndGroup();
    CHECK( log == vector<string>{ "do a 1", "do b 2" } );

    f.mHistory.Undo();
    CHECK( log == vector<string>{ "do a 1", "do b 2", "undo b 2", "undo a 1" } );
    f.mHistory.Redo();
    CHECK( log == vector<string>{ "do a 1", "do b 2", "undo b 2", "undo a 1", "redo a 1", "redo b 2" } );
}

TEST_CASE( "What a dragged field set is folds into one command, from the first value to the last" )
{
    vector<string> log;
    Fixture f;
    f.mHistory.BeginGroup( "drag" );
    Require( f.mHistory.TryDo( CreateScope<MarkCommand>( log, "brightness", "2" ), "set" ) );
    Require( f.mHistory.TryDo( CreateScope<MarkCommand>( log, "brightness", "3" ), "set" ) );
    Require( f.mHistory.TryDo( CreateScope<MarkCommand>( log, "brightness", "4" ), "set" ) );
    Require( f.mHistory.TryDo( CreateScope<MarkCommand>( log, "steps", "9" ), "set" ) );
    f.mHistory.EndGroup();
    CHECK( log == vector<string>{ "do brightness 2", "do brightness 3", "do brightness 4", "do steps 9" } );

    // One undo takes the whole drag back: three sets of one field are one
    // command that ends where the last one did, and another field stands apart.
    f.mHistory.Undo();
    CHECK( log == vector<string>{ "do brightness 2", "do brightness 3", "do brightness 4", "do steps 9", "undo steps 9",
                                  "undo brightness 4" } );
    f.mHistory.Redo();
    CHECK( log == vector<string>{ "do brightness 2", "do brightness 3", "do brightness 4", "do steps 9", "undo steps 9",
                                  "undo brightness 4", "redo brightness 4", "redo steps 9" } );
}

TEST_CASE( "A command that cannot be done inside a group leaves the group open to cancel" )
{
    vector<string> log;
    Fixture f;
    f.Add( 1 );
    f.mHistory.Undo();
    const u64 before = f.mHistory.Position();

    f.mHistory.BeginGroup( "half done" );
    CHECK( f.mHistory.Position() == before );
    Require( f.mHistory.TryDo( CreateScope<MarkCommand>( log, "a", "1" ), "mark a" ) );
    CHECK( f.mHistory.Position() != before );
    const auto doneMaybe = f.mHistory.TryDo( CreateScope<MarkCommand>( log, "b", "" ), "mark b" );
    REQUIRE( not doneMaybe );
    CHECK( doneMaybe.error() == "nothing to mark" );
    CHECK( f.mHistory.InGroup() );
    CHECK_FALSE( f.mHistory.CanUndo() );
    CHECK( log == vector<string>{ "do a 1" } );

    f.mHistory.CancelGroup();
    CHECK( log == vector<string>{ "do a 1", "undo a 1" } );
    CHECK( f.mTotal == 0 );
    CHECK( f.mHistory.Position() == before );
    CHECK( f.mHistory.CanRedo() );
}
