// A document changed by its edits, each one undone and redone: fields,
// entities made, removed, moved and renamed, components added and removed.
// What a command names before a removal it still names after the undo, and
// a link to an entity leads to it after it is renamed or moved.
#include "bubble/editing/edits.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Light
{
    f32 mBrightness = 1;
    vector<f32> mSteps;
    Value mProps = Value::Record{};
};

struct Health
{
    i32 mValue = 100;
};

struct Follow
{
    EntityPath mTarget;
    vector<EntityPath> mWaypoints;
};

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

// A name with its text in the table, for messages; "lamp"_n has none.
Name Declared( string_view name )
{
    const auto declaredMaybe = Name::TryFrom( name );
    REQUIRE( declaredMaybe );
    return *declaredMaybe;
}

FieldPath Path( string_view text )
{
    auto pathMaybe = FieldPath::TryParse( text );
    REQUIRE_MESSAGE( pathMaybe, pathMaybe.error() );
    return *pathMaybe;
}

struct Fixture
{
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    Document mDocument{ mComponentTypes };
    Entity mLamp;
    Entity mBulb;
    Entity mProps;

    // /lamp with a light and /lamp/bulb with health; /props.
    Fixture()
    {
        mReflection.Type<Light>( "light" )
            .Field<&Light::mBrightness>( "brightness" )
            .Field<&Light::mSteps>( "steps" )
            .Field<&Light::mProps>( "props" );
        mReflection.Type<Health>( "health" ).Field<&Health::mValue>( "value" );
        mReflection.Type<Follow>( "follow" )
            .Field<&Follow::mTarget>( "target" )
            .Field<&Follow::mWaypoints>( "waypoints" );
        mComponentTypes.Add<Light>();
        mComponentTypes.Add<Health>();
        mComponentTypes.Add<Follow>();

        mLamp = Create( Root(), "lamp" );
        mBulb = Create( mLamp, "bulb" );
        mProps = Create( Root(), "props" );
        Require( TryAddComponent( mDocument, mLamp, "light"_n ) );
        Require( TryAddComponent( mDocument, mBulb, "health"_n ) );
        Require( TrySetField( mDocument, mBulb, "health"_n, Path( "value" ), MetaAny( 50 ) ) );
        mDocument.MarkSaved();
    }

    const Scene& GetScene() const { return mDocument.GetScene(); }
    Entity Root() const { return GetScene().Root(); }
    History& GetHistory() { return mDocument.GetHistory(); }

    template <typename T>
    static void Require( const expected<T, string>& doneMaybe )
    {
        REQUIRE_MESSAGE( doneMaybe, doneMaybe.error() );
    }

    Entity Create( Entity parent, string_view name, size_t index = cAtEnd )
    {
        const auto createdMaybe = TryCreateEntity( mDocument, parent, Make( name ), index );
        Require( createdMaybe );
        return *createdMaybe;
    }

    void Set( Entity entity, string_view path, MetaAny value )
    {
        Require( TrySetField( mDocument, entity, "light"_n, Path( path ), std::move( value ) ) );
    }

    f32 Brightness( Entity entity ) const { return GetScene().Get<Light>( entity ).mBrightness; }

    string PathText( Entity entity ) const { return PathOf( GetScene(), entity ).mPath.String(); }

    // `holder` follows `target`, by way of `waypoints`.
    void Follows( Entity holder, string_view target, vector<string_view> waypoints = {} )
    {
        Require( TryAddComponent( mDocument, holder, "follow"_n ) );
        Value::List list;
        for ( const string_view waypoint : waypoints )
            list.push_back( EntityPath{ Make( waypoint ) } );
        Require(
            TrySetField( mDocument, holder, "follow"_n, Path( "target" ), MetaAny( EntityPath{ Make( target ) } ) ) );
        Require( TrySetField( mDocument, holder, "follow"_n, Path( "waypoints" ), MetaAny( Value( list ) ) ) );
    }

    const Follow& FollowOf( Entity holder ) const { return GetScene().Get<Follow>( holder ); }

    static vector<string> Texts( const vector<EntityPath>& paths )
    {
        vector<string> texts;
        for ( const EntityPath& path : paths )
            texts.push_back( path.mPath.String() );
        return texts;
    }

    vector<string> Children( Entity entity ) const
    {
        vector<string> names;
        for ( const Entity child : ChildrenOf( GetScene(), entity ) )
            names.push_back( NameOf( GetScene(), child ).String() );
        return names;
    }
};
}

TEST_CASE( "A field set is undone and redone, as the field's own type" )
{
    Fixture f;
    // An integer, converted to the f32 the field is.
    f.Set( f.mLamp, "brightness", MetaAny( 3 ) );
    CHECK( f.Brightness( f.mLamp ) == 3.0f );
    CHECK( f.GetHistory().UndoLabel() == "set light.brightness" );

    f.GetHistory().Undo();
    CHECK( f.Brightness( f.mLamp ) == 1.0f );
    f.GetHistory().Redo();
    CHECK( f.Brightness( f.mLamp ) == 3.0f );
}

TEST_CASE( "A field set through a list and into a Value is undone too" )
{
    Fixture f;
    f.Set( f.mLamp, "steps", MetaAny( Value( Value::List{ 1, 2, 3 } ) ) );
    f.Set( f.mLamp, "steps[2]", MetaAny( 20.0f ) );
    f.Set( f.mLamp, "props", MetaAny( Value( Value::Record{ { "speed"_n, 5 } } ) ) );
    f.Set( f.mLamp, "props.speed", MetaAny( 7.0 ) );
    const Light& light = f.GetScene().Get<Light>( f.mLamp );
    CHECK( light.mSteps == vector<f32>{ 1, 20, 3 } );
    CHECK( light.mProps == Value( Value::Record{ { "speed"_n, 7 } } ) );

    f.GetHistory().Undo();
    CHECK( f.GetScene().Get<Light>( f.mLamp ).mProps == Value( Value::Record{ { "speed"_n, 5 } } ) );
    f.GetHistory().Undo();
    CHECK( f.GetScene().Get<Light>( f.mLamp ).mProps == Value( Value::Record{} ) );
    f.GetHistory().Undo();
    CHECK( f.GetScene().Get<Light>( f.mLamp ).mSteps == vector<f32>{ 1, 2, 3 } );
    f.GetHistory().Redo();
    f.GetHistory().Redo();
    f.GetHistory().Redo();
    CHECK( f.GetScene().Get<Light>( f.mLamp ).mProps == Value( Value::Record{ { "speed"_n, 7 } } ) );
}

TEST_CASE( "Sets of one field in a group fold into one, from the first value to the last" )
{
    Fixture f;
    f.GetHistory().BeginGroup( "drag" );
    for ( i32 frame = 2; frame <= 100; ++frame )
        f.Set( f.mLamp, "brightness", MetaAny( static_cast<f32>( frame ) ) );
    f.GetHistory().EndGroup();
    CHECK( f.Brightness( f.mLamp ) == 100.0f );

    f.GetHistory().Undo();
    CHECK( f.Brightness( f.mLamp ) == 1.0f );
    f.GetHistory().Redo();
    CHECK( f.Brightness( f.mLamp ) == 100.0f );
}

TEST_CASE( "An edit that fails says why and leaves the document as it was" )
{
    Fixture f;
    const auto setMaybe = TrySetField( f.mDocument, f.mLamp, "light"_n, Path( "brightness" ), MetaAny( "bright"s ) );
    REQUIRE_FALSE( setMaybe );
    CHECK( setMaybe.error().starts_with( "/lamp: light.brightness: " ) );
    CHECK( TrySetField( f.mDocument, f.mLamp, "light"_n, Path( "steps[5]" ), MetaAny( 1.0f ) ).error() ==
           "/lamp: light.steps[5]: no element 5, there are 0" );
    CHECK( f.Brightness( f.mLamp ) == 1.0f );
    CHECK_FALSE( f.mDocument.IsDirty() );
    CHECK( f.GetHistory().UndoLabel() == "set health.value" );
}

TEST_CASE( "Edits refuse what is not there and what cannot be" )
{
    Fixture f;
    Document& document = f.mDocument;
    CHECK( TrySetField( document, f.mLamp, "health"_n, Path( "value" ), MetaAny( 1 ) ).error() ==
           "/lamp has no health" );
    CHECK( TryAddComponent( document, f.mLamp, "light"_n ).error() == "/lamp has a light already" );
    CHECK( TryRemoveComponent( document, f.mProps, "light"_n ).error() == "/props has no light" );
    CHECK( TryAddComponent( document, f.mLamp, Declared( "lamp" ) ).error() ==
           "/lamp: lamp is not a component (components: light, health, follow)" );
    // The tree is changed by the tree's edits, not as a component.
    CHECK( TryAddComponent( document, f.mLamp, Declared( "hierarchy" ) ).error() ==
           "/lamp: hierarchy is not a component (components: light, health, follow)" );

    CHECK( TryDestroyEntity( document, f.Root() ).error() == "the root stays: it goes with the scene" );
    CHECK( TryRenameEntity( document, f.Root(), Make( "top" ) ).error() == "the root has no name to change" );
    CHECK( TryMoveEntity( document, f.mLamp, f.mBulb ).error() ==
           "/lamp cannot go under /lamp/bulb: that is inside it" );

    const Entity gone = f.mBulb;
    REQUIRE( TryDestroyEntity( document, gone ) );
    CHECK( TryDestroyEntity( document, gone ).error() ==
           format( "not an entity of the document: entity {} (gone)", static_cast<u32>( gone ) ) );
    CHECK( TryCreateEntity( document, gone, Make( "child" ) ).error().starts_with( "not an entity of the document" ) );
    CHECK( TryRenameEntity( document, Entity::Null, Make( "x" ) ).error() ==
           "not an entity of the document: no entity" );
    CHECK( f.GetHistory().UndoLabel() == "destroy bulb" );
}

TEST_CASE( "A destroyed entity comes back with what was under it, its components and its number" )
{
    Fixture f;
    const Entity table = f.Create( f.Root(), "table" );
    f.Set( f.mLamp, "brightness", MetaAny( 4.0f ) );
    REQUIRE( TryDestroyEntity( f.mDocument, f.mLamp ) );
    CHECK_FALSE( f.GetScene().Alive( f.mLamp ) );
    CHECK_FALSE( f.GetScene().Alive( f.mBulb ) );
    CHECK( f.Children( f.Root() ) == vector<string>{ "props", "table" } );

    for ( int round = 0; round < 2; ++round )
    {
        f.GetHistory().Undo();
        CHECK( f.GetScene().Alive( f.mLamp ) );
        CHECK( f.GetScene().Alive( f.mBulb ) );
        CHECK( f.Children( f.Root() ) == vector<string>{ "lamp", "props", "table" } );
        CHECK( f.PathText( f.mBulb ) == "/lamp/bulb" );
        CHECK( f.Brightness( f.mLamp ) == 4.0f );
        CHECK( f.GetScene().Get<Health>( f.mBulb ).mValue == 50 );
        // Redone, it goes again, and the next undo brings it back again.
        f.GetHistory().Redo();
        CHECK_FALSE( f.GetScene().Alive( f.mLamp ) );
    }
    CHECK( f.GetScene().Alive( table ) );
}

TEST_CASE( "A step made before a removal still reaches its entity after the removal is undone" )
{
    Fixture f;
    f.Set( f.mLamp, "brightness", MetaAny( 4.0f ) );
    REQUIRE( TryDestroyEntity( f.mDocument, f.mLamp ) );
    f.GetHistory().Undo();
    f.GetHistory().Undo();
    CHECK( f.Brightness( f.mLamp ) == 1.0f );
    f.GetHistory().Redo();
    CHECK( f.Brightness( f.mLamp ) == 4.0f );
}

TEST_CASE( "An entity made, undone and redone is the same entity, and what was done to it is done again" )
{
    Fixture f;
    const Entity chair = f.Create( f.mProps, "chair" );
    REQUIRE( TryAddComponent( f.mDocument, chair, "light"_n ) );
    f.Set( chair, "brightness", MetaAny( 2.0f ) );
    CHECK( f.GetHistory().UndoLabel() == "set light.brightness" );

    f.GetHistory().Undo();
    f.GetHistory().Undo();
    CHECK( f.GetHistory().UndoLabel() == "create chair" );
    f.GetHistory().Undo();
    CHECK_FALSE( f.GetScene().Alive( chair ) );
    CHECK( f.Children( f.mProps ).empty() );

    f.GetHistory().Redo();
    f.GetHistory().Redo();
    f.GetHistory().Redo();
    CHECK( f.PathText( chair ) == "/props/chair" );
    CHECK( f.Brightness( chair ) == 2.0f );
}

TEST_CASE( "A moved entity goes back to its place and its name" )
{
    Fixture f;
    const Entity first = f.Create( f.mProps, "lamp" );
    const Entity second = f.Create( f.mProps, "shelf" );
    REQUIRE( TryMoveEntity( f.mDocument, first, f.Root(), 0 ) );
    // A lamp is there already.
    CHECK( f.Children( f.Root() ) == vector<string>{ "lamp_2", "lamp", "props" } );
    CHECK( f.Children( f.mProps ) == vector<string>{ "shelf" } );
    CHECK( f.GetHistory().UndoLabel() == "move lamp" );

    f.GetHistory().Undo();
    CHECK( f.Children( f.mProps ) == vector<string>{ "lamp", "shelf" } );
    CHECK( f.Children( f.Root() ) == vector<string>{ "lamp", "props" } );
    CHECK( f.PathText( first ) == "/props/lamp" );
    f.GetHistory().Redo();
    CHECK( f.PathText( first ) == "/lamp_2" );
    CHECK( f.PathText( second ) == "/props/shelf" );
}

TEST_CASE( "A rename gives back the name it got, and undo the old one" )
{
    Fixture f;
    const Entity shelf = f.Create( f.mProps, "shelf" );
    f.Create( f.mProps, "table" );
    const auto renamedMaybe = TryRenameEntity( f.mDocument, shelf, Make( "table" ) );
    REQUIRE( renamedMaybe );
    CHECK( renamedMaybe->View() == "table_2" );
    CHECK( f.GetHistory().UndoLabel() == "rename shelf" );

    f.GetHistory().Undo();
    CHECK( f.PathText( shelf ) == "/props/shelf" );
    f.GetHistory().Redo();
    CHECK( f.PathText( shelf ) == "/props/table_2" );
}

TEST_CASE( "A component added starts as its defaults, and a removed one comes back as it was" )
{
    Fixture f;
    REQUIRE( TryAddComponent( f.mDocument, f.mProps, "health"_n ) );
    CHECK( f.GetScene().Get<Health>( f.mProps ).mValue == 100 );
    f.GetHistory().Undo();
    CHECK_FALSE( f.GetScene().Has<Health>( f.mProps ) );

    REQUIRE( TryRemoveComponent( f.mDocument, f.mBulb, "health"_n ) );
    CHECK_FALSE( f.GetScene().Has<Health>( f.mBulb ) );
    CHECK( f.GetHistory().UndoLabel() == "remove health" );
    f.GetHistory().Undo();
    CHECK( f.GetScene().Get<Health>( f.mBulb ).mValue == 50 );
    f.GetHistory().Redo();
    CHECK_FALSE( f.GetScene().Has<Health>( f.mBulb ) );
}

TEST_CASE( "A document is dirty until undo takes it back to where it was saved" )
{
    Fixture f;
    CHECK_FALSE( f.mDocument.IsDirty() );
    f.Set( f.mLamp, "brightness", MetaAny( 2.0f ) );
    CHECK( f.mDocument.IsDirty() );
    f.GetHistory().Undo();
    CHECK_FALSE( f.mDocument.IsDirty() );
    // Undone past the save, it is dirty again.
    f.GetHistory().Undo();
    CHECK( f.mDocument.IsDirty() );
    f.GetHistory().Redo();
    CHECK_FALSE( f.mDocument.IsDirty() );

    f.Set( f.mLamp, "brightness", MetaAny( 2.0f ) );
    f.mDocument.MarkSaved();
    f.GetHistory().Undo();
    CHECK( f.mDocument.IsDirty() );

    // An open group has changed the scene already.
    f.GetHistory().Redo();
    f.GetHistory().BeginGroup( "drag" );
    CHECK_FALSE( f.mDocument.IsDirty() );
    f.Set( f.mLamp, "brightness", MetaAny( 5.0f ) );
    CHECK( f.mDocument.IsDirty() );
    f.GetHistory().CancelGroup();
    CHECK_FALSE( f.mDocument.IsDirty() );
}

TEST_CASE( "A rename leads the links to the entity and to what is under it to their new paths" )
{
    Fixture f;
    const Entity door = f.Create( f.mProps, "door" );
    f.Create( door, "handle" );
    f.Follows( f.mLamp, "/props/door", { "/props/door/handle", "/lamp", "/props/doorway" } );
    f.Set( f.mLamp, "props", MetaAny( Value( Value::Record{ { "door"_n, EntityPath{ Make( "/props/door" ) } } } ) ) );
    const auto renamedValue = [&] {
        return Value( Value::Record{ { "door"_n, EntityPath{ Make( "/props/gate" ) } } } );
    };

    REQUIRE( TryRenameEntity( f.mDocument, door, Make( "gate" ) ) );
    CHECK( f.FollowOf( f.mLamp ).mTarget.mPath.View() == "/props/gate" );
    // What led elsewhere, or nowhere, is as it was.
    CHECK( Fixture::Texts( f.FollowOf( f.mLamp ).mWaypoints ) ==
           vector<string>{ "/props/gate/handle", "/lamp", "/props/doorway" } );
    // A link inside a Value too.
    CHECK( f.GetScene().Get<Light>( f.mLamp ).mProps == renamedValue() );

    f.GetHistory().Undo();
    CHECK( f.FollowOf( f.mLamp ).mTarget.mPath.View() == "/props/door" );
    CHECK( Fixture::Texts( f.FollowOf( f.mLamp ).mWaypoints ) ==
           vector<string>{ "/props/door/handle", "/lamp", "/props/doorway" } );
    CHECK( f.GetScene().Get<Light>( f.mLamp ).mProps ==
           Value( Value::Record{ { "door"_n, EntityPath{ Make( "/props/door" ) } } } ) );
    f.GetHistory().Redo();
    CHECK( f.FollowOf( f.mLamp ).mTarget.mPath.View() == "/props/gate" );
    CHECK( f.GetScene().Get<Light>( f.mLamp ).mProps == renamedValue() );
}

TEST_CASE( "A move keeps relative links relative, from the moved entity and to it" )
{
    Fixture f;
    // From under the moved entity out, and from outside in.
    f.Follows( f.mBulb, "../../props", { ".." } );
    f.Follows( f.mProps, "../lamp/bulb", { "/lamp/bulb" } );

    REQUIRE( TryMoveEntity( f.mDocument, f.mBulb, f.Root() ) );
    CHECK( f.PathText( f.mBulb ) == "/bulb" );
    CHECK( f.FollowOf( f.mBulb ).mTarget.mPath.View() == "../props" );
    CHECK( Fixture::Texts( f.FollowOf( f.mBulb ).mWaypoints ) == vector<string>{ "../lamp" } );
    CHECK( f.FollowOf( f.mProps ).mTarget.mPath.View() == "../bulb" );
    CHECK( Fixture::Texts( f.FollowOf( f.mProps ).mWaypoints ) == vector<string>{ "/bulb" } );

    f.GetHistory().Undo();
    CHECK( f.PathText( f.mBulb ) == "/lamp/bulb" );
    CHECK( f.FollowOf( f.mBulb ).mTarget.mPath.View() == "../../props" );
    CHECK( Fixture::Texts( f.FollowOf( f.mBulb ).mWaypoints ) == vector<string>{ ".." } );
    CHECK( f.FollowOf( f.mProps ).mTarget.mPath.View() == "../lamp/bulb" );
    f.GetHistory().Redo();
    CHECK( f.FollowOf( f.mProps ).mTarget.mPath.View() == "../bulb" );
}

TEST_CASE( "A link inside what moves, to what moves with it, stays as it is when relative" )
{
    Fixture f;
    f.Follows( f.mLamp, "bulb", { "/lamp/bulb" } );
    REQUIRE( TryMoveEntity( f.mDocument, f.mLamp, f.mProps ) );
    CHECK( f.FollowOf( f.mLamp ).mTarget.mPath.View() == "bulb" );
    CHECK( Fixture::Texts( f.FollowOf( f.mLamp ).mWaypoints ) == vector<string>{ "/props/lamp/bulb" } );
}
