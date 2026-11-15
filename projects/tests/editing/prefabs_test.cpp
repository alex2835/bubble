// Prefabs in the editor: a subtree made a prefab, instances made, unpacked,
// reverted and applied; a prefab's own document keeping its nodes' ids; and
// a prefab saved reaching the instances open, what they changed kept.
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

// A level with /house, whose door has a light and health 50, and a window;
// made a prefab, props/house.prefab, when a test wants one.
struct Fixture
{
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    Operators mOperators;
    hmap<string, string> mFiles;
    Workspace mWorkspace{ mComponentTypes, ProjectFiles::InMemory( mFiles ) };
    Document& mLevel = mWorkspace.Open();

    Fixture()
    {
        mReflection.Type<Light>( "light" ).Field<&Light::mBrightness>( "brightness" );
        mReflection.Type<Health>( "health" ).Field<&Health::mValue>( "value" );
        mComponentTypes.Add<Light>();
        mComponentTypes.Add<Health>();
        AddBuiltinOperators( mOperators );
        Run( "entity.create", R"({ "name": "house" })" );
        Run( "entity.create", R"({ "name": "door", "parent": "/house" })" );
        Run( "component.add", R"({ "entities": [ "/house/door" ], "component": "light" })" );
        Run( "component.add", R"({ "entities": [ "/house/door" ], "component": "health" })" );
        Run( "property.set",
             R"({ "entities": [ "/house/door" ], "component": "health", "path": "value", "value": 50 })" );
        Run( "entity.create", R"({ "name": "window", "parent": "/house" })" );
        Run( "document.save", R"({ "path": "levels/street.scene" })" );
    }

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

    void MakePrefab() { Run( "prefab.create", R"({ "entity": "/house", "path": "props/house.prefab" })" ); }

    Entity Find( const Document& document, string_view path ) const
    {
        const auto foundMaybe = TryResolve( document.GetScene(), EntityPath{ Make( path ) } );
        REQUIRE_MESSAGE( foundMaybe, foundMaybe.error() );
        return *foundMaybe;
    }

    i32 HealthOf( const Document& document, string_view path ) const
    {
        return document.GetScene().Get<Health>( Find( document, path ) ).mValue;
    }

    f32 BrightnessOf( const Document& document, string_view path ) const
    {
        return document.GetScene().Get<Light>( Find( document, path ) ).mBrightness;
    }
};
}

TEST_CASE( "A subtree made a prefab is an instance of it in its place, and undo makes it plain again" )
{
    Fixture f;
    f.MakePrefab();
    REQUIRE( f.mFiles.contains( "props/house.prefab" ) );
    const Entity house = f.Find( f.mLevel, "/house" );
    CHECK( f.mLevel.GetScene().Get<PrefabInstance>( house ).mPrefab.View() == "props/house.prefab" );
    CHECK( f.mLevel.GetScene().Has<PrefabLink>( f.Find( f.mLevel, "/house/door" ) ) );
    CHECK( f.HealthOf( f.mLevel, "/house/door" ) == 50 );
    CHECK( f.mLevel.GetSelection().Active() == house );
    CHECK( f.mLevel.GetHistory().UndoLabel() == "Make Prefab" );

    // The level keeps the instance as the prefab and nothing else.
    f.Run( "document.save" );
    const string level = f.mFiles["levels/street.scene"];
    CHECK( level.find( R"("prefab": "props/house.prefab")" ) != string::npos );
    CHECK( level.find( "door" ) == string::npos );

    f.Run( "edit.undo" );
    CHECK_FALSE( f.mLevel.GetScene().Has<PrefabInstance>( f.Find( f.mLevel, "/house" ) ) );
    CHECK( f.HealthOf( f.mLevel, "/house/door" ) == 50 );
    f.Run( "edit.redo" );
    CHECK( f.mLevel.GetScene().Has<PrefabInstance>( f.Find( f.mLevel, "/house" ) ) );
}

TEST_CASE( "An instance keeps its prefab's names and places, and is saved and opened back with what it changed" )
{
    Fixture f;
    f.MakePrefab();
    f.Run( "prefab.instantiate", R"({ "prefab": "props/house.prefab", "parent": "/" })" );
    CHECK( f.Find( f.mLevel, "/house_2/window" ) != Entity::Null );
    CHECK( f.Why( "entity.rename", R"({ "entity": "/house_2/door", "name": "gate" })" ) ==
           "entity.rename: /house_2/door is a node of props/house.prefab: its name and its place are the prefab's - "
           "change them there" );
    CHECK( f.Why( "entity.move", R"({ "entities": [ "/house_2/door" ], "parent": "/" })" )
               .starts_with( "entity.move: /house_2/door is a node of props/house.prefab" ) );
    // The root is the level's to name and place.
    f.Run( "entity.rename", R"({ "entity": "/house_2", "name": "barn" })" );
    f.Run( "property.set", R"({ "entities": [ "/barn/door" ], "component": "health", "path": "value", "value": 75 })" );
    f.Run( "entity.destroy", R"({ "entities": [ "/barn/window" ] })" );
    f.Run( "entity.create", R"({ "name": "sign", "parent": "/barn" })" );
    f.Run( "document.save" );
    f.Run( "document.close" );

    f.Run( "document.open", R"({ "path": "levels/street.scene" })" );
    const Document& level = *f.mWorkspace.TryActive();
    CHECK( f.HealthOf( level, "/barn/door" ) == 75 );
    CHECK( f.HealthOf( level, "/house/door" ) == 50 );
    CHECK_FALSE( TryResolve( level.GetScene(), EntityPath{ Make( "/barn/window" ) } ) );
    CHECK( f.Find( level, "/barn/sign" ) != Entity::Null );
    CHECK_FALSE( level.IsDirty() );
}

TEST_CASE( "A node of an instance destroyed and undone is the prefab's node again" )
{
    Fixture f;
    f.MakePrefab();
    f.Run( "entity.destroy", R"({ "entities": [ "/house/window" ] })" );
    f.Run( "edit.undo" );
    f.Run( "document.save" );
    CHECK( f.mFiles["levels/street.scene"].find( "remove" ) == string::npos );
}

TEST_CASE( "A prefab's document keeps its nodes' ids, and its save reaches the instances open, what they changed kept" )
{
    Fixture f;
    f.MakePrefab();
    f.Run( "property.set",
           R"({ "entities": [ "/house/door" ], "component": "health", "path": "value", "value": 75 })" );
    CHECK( f.mLevel.GetHistory().CanUndo() );

    f.Run( "document.open", R"({ "path": "props/house.prefab" })" );
    Document& prefab = *f.mWorkspace.TryActive();
    REQUIRE( prefab.TryPrefabNodes() );
    CHECK( f.HealthOf( prefab, "/door" ) == 50 );
    f.Run( "property.set", R"({ "entities": [ "/door" ], "component": "light", "path": "brightness", "value": 4 })" );
    f.Run( "entity.create", R"({ "name": "chimney", "parent": "/" })" );
    f.Run( "document.save" );
    CHECK_FALSE( prefab.IsDirty() );

    // The level follows: a brighter door and a chimney, its own health kept.
    CHECK( f.BrightnessOf( f.mLevel, "/house/door" ) == 4.0f );
    CHECK( f.HealthOf( f.mLevel, "/house/door" ) == 75 );
    CHECK( f.Find( f.mLevel, "/house/chimney" ) != Entity::Null );
    // Its steps named entities that went.
    CHECK_FALSE( f.mLevel.GetHistory().CanUndo() );
    CHECK( f.mLevel.IsDirty() );

    // Saved again, the door and the window keep the ids the level knows them by.
    const auto savedMaybe = Prefab::TryParse( f.mFiles["props/house.prefab"], f.mComponentTypes );
    REQUIRE_MESSAGE( savedMaybe, savedMaybe.error() );
    CHECK( savedMaybe->TryFind( 1 )->mName.View() == "door" );
    CHECK( savedMaybe->TryFind( 2 )->mName.View() == "window" );
    CHECK( savedMaybe->TryFind( 3 )->mName.View() == "chimney" );
    CHECK( savedMaybe->NextId() == 4 );
    CHECK( f.Why( "document.save", R"({ "path": "props/house.scene" })" ) ==
           "document.save: props/house.scene: a prefab is saved to a .prefab" );
}

TEST_CASE( "Revert takes a field, a component or a whole node back to its prefab, a step each" )
{
    Fixture f;
    f.MakePrefab();
    f.Run( "property.set",
           R"({ "entities": [ "/house/door" ], "component": "health", "path": "value", "value": 75 })" );
    f.Run( "property.set",
           R"({ "entities": [ "/house/door" ], "component": "light", "path": "brightness", "value": 3 })" );
    f.Run( "component.add", R"({ "entities": [ "/house/window" ], "component": "health" })" );

    f.Run( "prefab.revert", R"({ "entity": "/house/door", "path": "health.value" })" );
    CHECK( f.HealthOf( f.mLevel, "/house/door" ) == 50 );
    CHECK( f.BrightnessOf( f.mLevel, "/house/door" ) == 3.0f );
    CHECK( f.mLevel.GetHistory().UndoLabel() == "Revert to Prefab" );
    f.Run( "prefab.revert", R"({ "entity": "/house/door" })" );
    CHECK( f.BrightnessOf( f.mLevel, "/house/door" ) == 1.0f );
    f.Run( "prefab.revert", R"({ "entity": "/house/window" })" );
    CHECK_FALSE( f.mLevel.GetScene().Has<Health>( f.Find( f.mLevel, "/house/window" ) ) );

    f.Run( "edit.undo" );
    CHECK( f.mLevel.GetScene().Has<Health>( f.Find( f.mLevel, "/house/window" ) ) );
    CHECK( f.Why( "prefab.revert", R"({ "entity": "/" })" ) == "prefab.revert: / is no node of a prefab's instance" );
    CHECK( f.Why( "prefab.revert", R"({ "entity": "/house/door", "path": "lamp" })" )
               .starts_with( "prefab.revert: path: lamp is not a component" ) );
}

TEST_CASE( "Apply writes what an instance changed into its prefab, and every instance follows" )
{
    Fixture f;
    f.MakePrefab();
    f.Run( "prefab.instantiate", R"({ "prefab": "props/house.prefab", "parent": "/", "name": "barn" })" );
    f.Run( "property.set",
           R"({ "entities": [ "/house/door" ], "component": "health", "path": "value", "value": 75 })" );
    f.Run( "property.set",
           R"({ "entities": [ "/house/door" ], "component": "light", "path": "brightness", "value": 3 })" );

    f.Run( "prefab.apply", R"({ "entity": "/house/door", "path": "health.value" })" );
    CHECK( f.HealthOf( f.mLevel, "/barn/door" ) == 75 );
    // What was not applied stays the instance's own.
    CHECK( f.BrightnessOf( f.mLevel, "/house/door" ) == 3.0f );
    CHECK( f.BrightnessOf( f.mLevel, "/barn/door" ) == 1.0f );
    CHECK( f.mFiles["props/house.prefab"].find( R"("value": 75)" ) != string::npos );

    f.Run( "prefab.apply", R"({ "entity": "/house/door" })" );
    CHECK( f.BrightnessOf( f.mLevel, "/barn/door" ) == 3.0f );
    f.Run( "document.save" );
    // Nothing differs from the prefab any more.
    CHECK( f.mFiles["levels/street.scene"].find( "set" ) == string::npos );
}

TEST_CASE( "Unpack makes an instance plain entities, and undo makes it an instance again" )
{
    Fixture f;
    f.MakePrefab();
    f.Run( "prefab.unpack", R"({ "entity": "/house" })" );
    CHECK_FALSE( f.mLevel.GetScene().Has<PrefabInstance>( f.Find( f.mLevel, "/house" ) ) );
    CHECK_FALSE( f.mLevel.GetScene().Has<PrefabLink>( f.Find( f.mLevel, "/house/door" ) ) );
    f.Run( "entity.rename", R"({ "entity": "/house/door", "name": "gate" })" );
    f.Run( "edit.undo" );
    f.Run( "edit.undo" );
    CHECK( f.mLevel.GetScene().Has<PrefabLink>( f.Find( f.mLevel, "/house/door" ) ) );
    CHECK( f.Why( "prefab.unpack", R"({ "entity": "/house/door" })" ) ==
           "prefab.unpack: /house/door is no instance of a prefab" );
}

TEST_CASE( "Prefab operators refuse what cannot be, saying why" )
{
    Fixture f;
    CHECK( f.Why( "prefab.create", R"({ "entity": "/", "path": "props/all.prefab" })" ) ==
           "prefab.create: the root is the scene: a prefab is made of what is under it" );
    CHECK( f.Why( "prefab.create", R"({ "entity": "/house", "path": "props/house.scene" })" ) ==
           "prefab.create: path: a prefab is a .prefab, not props/house.scene" );
    f.MakePrefab();
    CHECK( f.Why( "prefab.create", R"({ "entity": "/house/door", "path": "props/door.prefab" })" )
               .ends_with( "is of a prefab already: a prefab of an instance is not there yet" ) );
    f.Run( "entity.create", R"({ "name": "shed", "parent": "/" })" );
    CHECK( f.Why( "prefab.create", R"({ "entity": "/shed", "path": "props/house.prefab" })" ) ==
           "prefab.create: path: props/house.prefab is there already: give another" );
    CHECK( f.Why( "prefab.instantiate", R"({ "prefab": "props/none.prefab" })" ) ==
           "prefab.instantiate: prefab: props/none.prefab: no such file" );
    CHECK( f.Why( "document.save", R"({ "path": "levels/street.prefab" })" ) ==
           "document.save: levels/street.prefab: a level is not saved as a prefab - prefab.create makes one" );

    f.Run( "document.open", R"({ "path": "props/house.prefab" })" );
    CHECK( f.Why( "prefab.instantiate", R"({ "prefab": "props/house.prefab" })" ) ==
           "prefab.instantiate: a prefab inside a prefab is not there yet" );
    f.Run( "entity.create", R"({ "name": "porch", "parent": "/" })" );
    f.mWorkspace.Activate( f.mLevel );
    CHECK( f.Why( "prefab.apply", R"({ "entity": "/house/door" })" ) ==
           "prefab.apply: props/house.prefab is open with changes not saved: save it or close it first" );
}
