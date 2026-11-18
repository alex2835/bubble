#include "bubble/input/controls.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "Keys go by their names and their HID codes, both ways" )
{
    CHECK( KeyName( Key::Space ) == "space" );
    CHECK( KeyName( Key::Digit1 ) == "1" );
    CHECK( KeyName( Key::Keypad0 ) == "keypad_0" );
    CHECK( KeyName( Key::Unknown ).empty() );
    CHECK( TryKeyFromName( "left_shift" ) == Key::LeftShift );
    CHECK( TryKeyFromName( "Space" ) == nullopt );
    CHECK( TryKeyFromCode( 44 ) == Key::Space );
    CHECK( TryKeyFromCode( 4 ) == Key::A );
    // The key beside Enter on some keyboards, which ours leave out.
    CHECK( TryKeyFromCode( 50 ) == nullopt );
    CHECK( TryKeyFromCode( cKeySlots ) == nullopt );

    hset<string> names;
    for ( u32 code = 0; code < cKeySlots; ++code )
        if ( const auto keyMaybe = TryKeyFromCode( code ) )
        {
            CHECK( TryKeyFromName( KeyName( *keyMaybe ) ) == *keyMaybe );
            names.insert( string( KeyName( *keyMaybe ) ) );
        }
    CHECK( names.size() == 104 );
}

TEST_CASE( "Mouse and gamepad controls have snake_case names" )
{
    CHECK( MouseButtonName( MouseButton::X2 ) == "x2" );
    CHECK( GamepadButtonName( GamepadButton::South ) == "south" );
    CHECK( GamepadButtonName( GamepadButton::DpadRight ) == "dpad_right" );
    CHECK( GamepadAxisName( GamepadAxis::RightTrigger ) == "right_trigger" );
    CHECK( MouseAxisName( MouseAxis::Wheel ) == "wheel" );
}

TEST_CASE( "Mouse and gamepad controls are found by their names, and an unknown name is nothing" )
{
    CHECK( TryMouseButtonFromName( "x2" ) == MouseButton::X2 );
    CHECK( TryMouseAxisFromName( "y" ) == MouseAxis::Y );
    CHECK( TryGamepadButtonFromName( "dpad_right" ) == GamepadButton::DpadRight );
    CHECK( TryGamepadAxisFromName( "left_trigger" ) == GamepadAxis::LeftTrigger );
    CHECK( TryMouseButtonFromName( "Left" ) == nullopt );
    CHECK( TryMouseAxisFromName( "z" ) == nullopt );
    CHECK( TryGamepadButtonFromName( "a" ) == nullopt );
    CHECK( TryGamepadAxisFromName( "" ) == nullopt );
}
