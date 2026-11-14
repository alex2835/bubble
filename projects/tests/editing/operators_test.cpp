// Operators: run by name with JSON arguments checked against what each
// takes, one step of the active document's history apiece, undone whole when
// they fail halfway; and the built-in ones over the tree, components, the
// clipboard, the history and the selection.
#include "bubble/editing/edits.hpp"
#include "bubble/editing/operators.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Light
{
    f32 mBrightness = 1;
};

struct Health
{
    i32 mValue = 100;
};

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

Json Args( string_view text )
{
    auto jsonMaybe = Json::TryParse( text );
    REQUIRE_MESSAGE( jsonMaybe, jsonMaybe.error() );
    return *jsonMaybe;
}

// /lamp with a light; /props/chair, /props/table.
struct Fixture
{
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    Operators mOperators;
    Workspace mWorkspace{ mComponentTypes };
    Document& mDocument = mWorkspace.Open();
    Entity mLamp;
    Entity mProps;
    Entity mChair;
    Entity mTable;

    Fixture()
    {
        mReflection.Type<Light>( "light" ).Field<&Light::mBrightness>( "brightness" );
        mReflection.Type<Health>( "health" ).Field<&Health::mValue>( "value" );
        mComponentTypes.Add<Light>();
        mComponentTypes.Add<Health>();
        AddBuiltinOperators( mOperators );

        Run( "entity.create", R"({ "name": "lamp" })" );
        mLamp = Find( "/lamp" );
        Run( "component.add", R"({ "entities": [ "/lamp" ], "component": "light" })" );
        Run( "entity.create", R"({ "name": "props", "parent": "/" })" );
        mProps = Find( "/props" );
        Run( "entity.create", R"({ "name": "chair", "parent": "/props" })" );
        mChair = Find( "/props/chair" );
        Run( "entity.create", R"({ "name": "table", "parent": "/props" })" );
        mTable = Find( "/props/table" );
        mDocument.GetSelection().Clear();
        mDocument.MarkSaved();
    }

    const Scene& GetScene() const { return mDocument.GetScene(); }
    History& GetHistory() { return mDocument.GetHistory(); }
    Selection& GetSelection() { return mDocument.GetSelection(); }

    void Run( string_view id, string_view args = "null" )
    {
        const auto ranMaybe = mOperators.TryRun( mWorkspace, id, Args( args ) );
        REQUIRE_MESSAGE( ranMaybe, ranMaybe.error() );
    }

    string Why( string_view id, string_view args = "null" )
    {
        const auto ranMaybe = mOperators.TryRun( mWorkspace, id, Args( args ) );
        REQUIRE_FALSE( ranMaybe );
        return ranMaybe.error();
    }

    Entity Find( string_view path ) const
    {
        const auto foundMaybe = TryResolve( GetScene(), EntityPath{ Make( path ) } );
        REQUIRE_MESSAGE( foundMaybe, foundMaybe.error() );
        return *foundMaybe;
    }

    vector<string> Children( Entity entity ) const
    {
        vector<string> names;
        for ( const Entity child : ChildrenOf( GetScene(), entity ) )
            names.push_back( NameOf( GetScene(), child ).String() );
        return names;
    }

    string PathText( Entity entity ) const { return PathOf( GetScene(), entity ).mPath.String(); }
};
}

TEST_CASE( "An operator that edits is one step, labelled as the operator is" )
{
    Fixture f;
    f.Run( "entity.rename", R"({ "entity": "/lamp", "name": "light" })" );
    CHECK( f.PathText( f.mLamp ) == "/light" );
    CHECK( f.GetHistory().UndoLabel() == "Rename" );
    CHECK( f.mDocument.IsDirty() );

    f.Run( "edit.undo" );
    CHECK( f.PathText( f.mLamp ) == "/lamp" );
    CHECK_FALSE( f.mDocument.IsDirty() );
    f.Run( "edit.redo" );
    CHECK( f.PathText( f.mLamp ) == "/light" );
}

TEST_CASE( "Arguments are checked against what the operator takes, and say where they are wrong" )
{
    Fixture f;
    CHECK( f.Why( "entity.rename", R"({ "entity": "/lamp" })" ) ==
           "entity.rename: name is missing (arguments: entity?, name)" );
    CHECK(
        f.Why( "entity.rename", R"({ "entity": "/lamp", "name": 3 })" ).starts_with( "1:30: entity.rename.name: " ) );
    CHECK( f.Why( "entity.rename", R"({ "name": "x", "colour": 3 })" )
               .starts_with( "1:26: entity.rename.colour: not a field (fields: entity, name)" ) );
    CHECK( f.Why( "entity.rename", R"({ "entity": "/lmap", "name": "x" })" ) ==
           "entity.rename: entity: /lmap: / has no child 'lmap' (children: 'lamp', 'props')" );
    CHECK( f.Why( "entity.create", R"({ "index": 0 })" ) == "entity.create: index: a place counts from 1, not 0" );
    CHECK( f.Why( "entity.vanish" ).starts_with( "entity.vanish is not an operator (operators: component.add, " ) );
    // Nothing of what failed is a step.
    CHECK_FALSE( f.mDocument.IsDirty() );
}

TEST_CASE( "An operator that fails halfway leaves the document as it found it" )
{
    Fixture f;
    f.Run( "component.add", R"({ "entities": [ "/props/table" ], "component": "health" })" );
    const u64 before = f.GetHistory().Position();
    // The chair gets health, then the table, which has it, refuses.
    CHECK( f.Why( "component.add", R"({ "entities": [ "/props/chair", "/props/table" ], "component": "health" })" ) ==
           "component.add: /props/table has a health already" );
    CHECK_FALSE( f.GetScene().Has<Health>( f.mChair ) );
    CHECK( f.GetHistory().Position() == before );
    CHECK_FALSE( f.GetHistory().InGroup() );
}

TEST_CASE( "What takes entities takes the selection when none are given, and polls for one" )
{
    Fixture f;
    CHECK( f.mOperators.TryPoll( f.mWorkspace, "entity.destroy" ).error() == "entity.destroy: nothing selected" );
    CHECK( f.Why( "entity.destroy" ) == "entity.destroy: nothing selected" );

    f.GetSelection().Select( f.mChair );
    f.GetSelection().Add( f.mProps );
    CHECK( f.mOperators.TryPoll( f.mWorkspace, "entity.destroy" ) );
    // /props/chair goes with /props: the top of the selection.
    f.Run( "entity.destroy" );
    CHECK( f.Children( f.GetScene().Root() ) == vector<string>{ "lamp" } );
    CHECK( f.GetSelection().Empty() );
    f.Run( "edit.undo" );
    CHECK( f.Children( f.mProps ) == vector<string>{ "chair", "table" } );
    CHECK( f.GetSelection().All() == vector<Entity>{ f.mChair, f.mProps } );
}

TEST_CASE( "A new entity goes under the active one, or the root, and is selected" )
{
    Fixture f;
    f.Run( "entity.create" );
    CHECK( f.Children( f.GetScene().Root() ) == vector<string>{ "lamp", "props", "entity" } );
    f.GetSelection().Select( f.mProps );
    f.Run( "entity.create", R"({ "name": "shelf", "index": 1 })" );
    CHECK( f.Children( f.mProps ) == vector<string>{ "shelf", "chair", "table" } );
    CHECK( f.GetSelection().Active() == f.Find( "/props/shelf" ) );
    CHECK( f.GetHistory().UndoLabel() == "Create" );
}

TEST_CASE( "A move puts what moves in front of the child at the place, in the tree's order" )
{
    Fixture f;
    f.Run( "entity.create", R"({ "name": "bench", "parent": "/props" })" );
    f.Run( "entity.create", R"({ "name": "stool", "parent": "/props" })" );
    // chair, table, bench, stool: the first two go in front of what is
    // second without them - stool.
    f.Run( "entity.move", R"({ "entities": [ "/props/table", "/props/chair" ], "parent": "/props", "index": 2 })" );
    CHECK( f.Children( f.mProps ) == vector<string>{ "bench", "chair", "table", "stool" } );
    f.Run( "entity.move", R"({ "entities": [ "/props/stool" ], "parent": "/props", "index": 1 })" );
    CHECK( f.Children( f.mProps ) == vector<string>{ "stool", "bench", "chair", "table" } );
    f.Run( "entity.move", R"({ "entities": [ "/lamp" ], "parent": "/props" })" );
    CHECK( f.PathText( f.mLamp ) == "/props/lamp" );

    CHECK( f.Why( "entity.move", R"({ "entities": [ "/props" ], "parent": "/props/chair" })" ) ==
           "entity.move: /props cannot go under /props/chair: that is inside it" );
}

TEST_CASE( "A group gathers what is selected where the first of it was, and ungroup lets it go" )
{
    Fixture f;
    f.GetSelection().Select( f.mTable );
    f.GetSelection().Add( f.mChair );
    f.Run( "entity.group", R"({ "name": "set" })" );
    const Entity set = f.Find( "/props/set" );
    CHECK( f.Children( f.mProps ) == vector<string>{ "set" } );
    CHECK( f.Children( set ) == vector<string>{ "chair", "table" } );
    CHECK( f.GetSelection().All() == vector<Entity>{ set } );
    CHECK( f.GetHistory().UndoLabel() == "Group" );

    f.Run( "entity.ungroup" );
    CHECK( f.Children( f.mProps ) == vector<string>{ "chair", "table" } );
    CHECK_FALSE( f.GetScene().Alive( set ) );
    CHECK( f.GetSelection().All() == vector<Entity>{ f.mChair, f.mTable } );

    // Each was one step.
    f.Run( "edit.undo" );
    CHECK( f.Children( set ) == vector<string>{ "chair", "table" } );
    f.Run( "edit.undo" );
    CHECK( f.Children( f.mProps ) == vector<string>{ "chair", "table" } );
    CHECK_FALSE( f.GetScene().Alive( set ) );
}

TEST_CASE( "A property set on everything selected is one step, its value converted to each field" )
{
    Fixture f;
    f.Run( "component.add", R"({ "entities": [ "/props/chair" ], "component": "light" })" );
    f.GetSelection().Select( f.mLamp );
    f.GetSelection().Add( f.mChair );
    f.Run( "property.set", R"({ "component": "light", "path": "brightness", "value": 4 })" );
    CHECK( f.GetScene().Get<Light>( f.mLamp ).mBrightness == 4.0f );
    CHECK( f.GetScene().Get<Light>( f.mChair ).mBrightness == 4.0f );
    CHECK( f.GetHistory().UndoLabel() == "Set Property" );
    f.Run( "edit.undo" );
    CHECK( f.GetScene().Get<Light>( f.mLamp ).mBrightness == 1.0f );
    CHECK( f.GetScene().Get<Light>( f.mChair ).mBrightness == 1.0f );

    CHECK( f.Why( "property.set", R"({ "component": "light", "path": "brightness..x", "value": 4 })" ) ==
           "property.set: path: brightness..x: an empty step" );
}

TEST_CASE( "Cut, paste and duplicate select what they made, and paste lands after the active entity" )
{
    Fixture f;
    CHECK( f.mOperators.TryPoll( f.mWorkspace, "edit.paste" ).error() == "edit.paste: the clipboard is empty" );
    f.GetSelection().Select( f.mChair );
    f.Run( "edit.cut" );
    CHECK( f.Children( f.mProps ) == vector<string>{ "table" } );
    CHECK( f.GetHistory().UndoLabel() == "Cut" );

    f.GetSelection().Select( f.mLamp );
    f.Run( "edit.paste" );
    CHECK( f.Children( f.GetScene().Root() ) == vector<string>{ "lamp", "chair", "props" } );
    CHECK( f.GetSelection().All() == vector<Entity>{ f.Find( "/chair" ) } );

    f.Run( "entity.duplicate" );
    CHECK( f.Children( f.GetScene().Root() ) == vector<string>{ "lamp", "chair", "chair_2", "props" } );
    CHECK( f.GetSelection().All() == vector<Entity>{ f.Find( "/chair_2" ) } );

    f.Run( "edit.paste", R"({ "parent": "/props", "index": 1 })" );
    CHECK( f.Children( f.mProps ) == vector<string>{ "chair", "table" } );
}

TEST_CASE( "Selecting, undoing and copying change no document and are no step" )
{
    Fixture f;
    f.Run( "selection.set", R"({ "entities": [ "/lamp", "/props" ] })" );
    CHECK( f.GetSelection().All() == vector<Entity>{ f.mLamp, f.mProps } );
    f.Run( "selection.add", R"({ "entities": [ "/props/chair" ] })" );
    CHECK( f.GetSelection().Active() == f.mChair );
    f.Run( "edit.copy" );
    CHECK( f.mWorkspace.GetClipboard().All().size() == 2 );
    f.Run( "selection.clear" );
    CHECK( f.GetSelection().Empty() );
    CHECK_FALSE( f.mDocument.IsDirty() );

    CHECK( f.Why( "edit.redo" ) == "edit.redo: nothing to redo" );
    Workspace empty( f.mComponentTypes );
    CHECK( f.mOperators.TryRun( empty, "entity.create" ).error() == "entity.create: no document is open" );
    CHECK( f.mOperators.TryRun( empty, "edit.undo" ).error() == "edit.undo: no document is open" );
}

TEST_CASE( "An operator run inside another's step is part of that step" )
{
    Fixture f;
    f.GetHistory().BeginGroup( "macro" );
    f.Run( "entity.create", R"({ "name": "a" })" );
    f.Run( "entity.create", R"({ "name": "b", "parent": "/" })" );
    f.GetHistory().EndGroup();
    CHECK( f.GetHistory().UndoLabel() == "macro" );
    f.Run( "edit.undo" );
    CHECK( f.Children( f.GetScene().Root() ) == vector<string>{ "lamp", "props" } );
}

TEST_CASE( "Operators are listed by id, and a bad one is refused when it is added" )
{
    Operators operators;
    AddBuiltinOperators( operators );
    REQUIRE( operators.All().size() > 1 );
    for ( size_t i = 1; i < operators.All().size(); ++i )
        CHECK( operators.All()[i - 1].mId < operators.All()[i].mId );
    CHECK( operators.TryFind( "entity.rename" )->mLabel == "Rename" );

    const auto made = []( string id, bool runs ) {
        Operator op;
        op.mId = std::move( id );
        if ( runs )
            op.mTryRun = []( Workspace&, const OperatorArgs& ) { return expected<void, string>(); };
        return op;
    };
    CHECK_THROWS_AS( operators.Add( made( "Entity.Rename", true ) ), logic_error );
    CHECK_THROWS_AS( operators.Add( made( "rename", true ) ), logic_error );
    CHECK_THROWS_AS( operators.Add( made( "entity.rename", true ) ), logic_error );
    CHECK_THROWS_AS( operators.Add( made( "entity.spin", false ) ), logic_error );
}

TEST_CASE( "Arguments can come as a record too" )
{
    Fixture f;
    const Value::Record args{ { "entity"_n, EntityPath{ Make( "/lamp" ) } }, { "name"_n, Make( "light" ) } };
    REQUIRE( f.mOperators.TryRun( f.mWorkspace, "entity.rename", args ) );
    CHECK( f.PathText( f.mLamp ) == "/light" );
    const Value::Record wrong{ { "name"_n, 3 } };
    CHECK( f.mOperators.TryRun( f.mWorkspace, "entity.rename", wrong ).error().starts_with( "entity.rename.name: " ) );
}
