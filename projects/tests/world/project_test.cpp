// project.bubble: a project's name, its start level and its actions, read
// strictly - a typo is an error that says where it is and what there is.
#include "bubble/world/project.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "A project's settings give its name, its start level and its actions with their bindings" )
{
    const auto settingsMaybe = TryReadProjectSettings( R"({
        "version": 1,
        "name": "sandbox",
        "start_level": { "asset": "9c1e0f3a5b7d4e2c8a6f1b3d5e7c9a0b", "path": "levels/main.scene" },
        "actions": {
            "move_x": [ { "key": "a", "scale": -1 }, { "key": "d" }, { "gamepad_axis": "left_x" } ],
            "jump": [ { "key": "space" }, { "gamepad_button": "south" }, { "touch": [ 0.5, 0, 1, 1 ] } ],
            "look_x": [ { "mouse_axis": "x", "scale": 0.004 } ],
            "fire": [ { "mouse_button": "left" } ]
        }
    })" );
    REQUIRE_MESSAGE( settingsMaybe, settingsMaybe.error() );
    const ProjectSettings& settings = *settingsMaybe;
    CHECK( settings.mName == "sandbox" );
    CHECK( settings.mStartLevelId == *AssetId::TryParse( "9c1e0f3a5b7d4e2c8a6f1b3d5e7c9a0b" ) );
    CHECK( settings.mStartLevelPathMaybe == *AssetPath::TryFrom( "levels/main.scene" ) );
    CHECK( settings.mActions.Names() == "move_x, jump, look_x, fire" );
    const span<const Action> actions = settings.mActions.List();
    REQUIRE( actions[0].mBindings.size() == 3 );
    CHECK( actions[0].mBindings[0].mInput == ActionInput( Key::A ) );
    CHECK( actions[0].mBindings[0].mScale == -1.0f );
    CHECK( actions[0].mBindings[2].mInput == ActionInput( GamepadAxis::LeftX ) );
    REQUIRE( actions[1].mBindings.size() == 3 );
    CHECK( actions[1].mBindings[1].mInput == ActionInput( GamepadButton::South ) );
    CHECK( actions[1].mBindings[2].mInput ==
           ActionInput( TouchArea{ .mMin = { 0.5f, 0.0f }, .mMax = { 1.0f, 1.0f } } ) );
    CHECK( actions[2].mBindings[0].mInput == ActionInput( MouseAxis::X ) );
    CHECK( actions[2].mBindings[0].mScale == 0.004f );
    CHECK( actions[3].mBindings[0].mInput == ActionInput( MouseButton::Left ) );
}

TEST_CASE( "The start level is found by its id, else by its path, and is nothing when the project lacks it" )
{
    AssetCatalog catalog;
    const AssetId level = AssetId::Generate();
    catalog.Add( level, *AssetPath::TryFrom( "levels/main.scene" ) );

    ProjectSettings settings;
    CHECK( settings.TryStartLevel( catalog ) == nullopt );
    settings.mStartLevelPathMaybe = *AssetPath::TryFrom( "levels/main.scene" );
    CHECK( settings.TryStartLevel( catalog ) == level );
    // An id the catalog does not know - the level was made again - leaves
    // the path to find it.
    settings.mStartLevelId = AssetId::Generate();
    CHECK( settings.TryStartLevel( catalog ) == level );
    settings.mStartLevelPathMaybe = *AssetPath::TryFrom( "levels/gone.scene" );
    CHECK( settings.TryStartLevel( catalog ) == nullopt );
}

TEST_CASE( "A mistake in a project's settings says where it is and what there is instead" )
{
    const auto error = []( string_view text ) {
        const auto settingsMaybe = TryReadProjectSettings( text );
        REQUIRE_FALSE( settingsMaybe );
        return settingsMaybe.error();
    };
    CHECK( error( R"({ "version": 1, "actions": { "jump": [ { "key": "spce" } ] } })" ) ==
           "1:49: actions.jump[0].key: no key spce" );
    CHECK( error( R"({ "version": 1, "actions": { "look": [ { "mouse_axis": "z" } ] } })" ) ==
           "1:56: actions.look[0].mouse_axis: no mouse axis z (axes: x, y, wheel)" );
    CHECK( error( R"({ "version": 1, "actions": { "jump": [ { "key": "space", "key2": "w" } ] } })" )
               .ends_with( "actions.jump[0].key2: not an input (inputs: key, mouse_button, mouse_axis, "
                           "gamepad_button, gamepad_axis, touch; and scale)" ) );
    CHECK( error( R"({ "version": 1, "actions": { "jump": [ { "key": "space", "gamepad_button": "south" } ] } })" )
               .ends_with( "actions.jump[0]: one input a binding" ) );
    CHECK( error( R"({ "version": 1, "actions": { "Jump": [] } })" )
               .ends_with( "actions.Jump: an action's name is snake_case" ) );
    CHECK( error( R"({ "version": 2 })" ) == "1:14: version: this engine reads version 1, not 2" );
    CHECK( error( R"({ "name": "sandbox" })" ).ends_with( "a project's settings say their version: \"version\": 1" ) );
    CHECK( error( R"({ "version": 1, "title": "sandbox" })" )
               .ends_with( "title: not a field (fields: version, name, start_level, actions)" ) );
}
