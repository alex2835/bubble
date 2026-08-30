#include "engine/pch/pch.hpp"
#include "engine/types/string.hpp"
#include "engine/scripting/bindings/window_input_bindings.hpp"
#include "engine/window/window.hpp"
#include <sol/sol.hpp>

namespace bubble
{
constexpr string_view keyboardKeys = R"(
KeyboardKey =
{
    unknown = -1,
    space = 32,
    apostrophe = 39,
    comma = 44,
    minus = 45,
    period = 46,
    slash = 47,
    zero = 48,
    one = 49,
    two = 50,
    three = 51,
    four = 52,
    five = 53,
    six = 54,
    seven = 55,
    eight = 56,
    nine = 57,
    semicolon = 59,
    equal = 61,
    a = 65,
    b = 66,
    c = 67,
    d = 68,
    e = 69,
    f = 70,
    g = 71,
    h = 72,
    i = 73,
    j = 74,
    k = 75,
    l = 76,
    m = 77,
    n = 78,
    o = 79,
    p = 80,
    q = 81,
    r = 82,
    s = 83,
    t = 84,
    u = 85,
    v = 86,
    w = 87,
    x = 88,
    y = 89,
    z = 90,
    left_bracket = 91,  
    backslash = 92,  
    right_bracket = 93,  
    grave_accent = 96,  
    world_1 = 161,
    world_2 = 162,
    
    escape = 256,
    enter = 257,
    tab = 258,
    backspace = 259,
    insert = 260,
    del = 261,
    right = 262,
    left = 263,
    down = 264,
    up = 265,
    page_up = 266,
    page_down = 267,
    home = 268,
    end_key = 269,
    caps_lock = 280,
    scroll_lock = 281,
    num_lock = 282,
    print_screen = 283,
    pause = 284,
    f1 = 290,
    f2 = 291,
    f3 = 292,
    f4 = 293,
    f5 = 294,
    f6 = 295,
    f7 = 296,
    f8 = 297,
    f9 = 298,
    f10 = 299,
    f11 = 300,
    f12 = 301,
    f13 = 302,
    f14 = 303,
    f15 = 304,
    f16 = 305,
    f17 = 306,
    f18 = 307,
    f19 = 308,
    f20 = 309,
    f21 = 310,
    f22 = 311,
    f23 = 312,
    f24 = 313,
    f25 = 314,
    kp_0 = 320,
    kp_1 = 321,
    kp_2 = 322,
    kp_3 = 323,
    kp_4 = 324,
    kp_5 = 325,
    kp_6 = 326,
    kp_7 = 327,
    kp_8 = 328,
    kp_9 = 329,
    kp_decimal = 330,
    kp_divide = 331,
    kp_multiply = 332,
    kp_subtract = 333,
    kp_add = 334,
    kp_enter = 335,
    kp_equal = 336,
    left_shift = 340,
    left_control = 341,
    left_alt = 342,
    left_super = 343,
    right_shift = 344,
    right_control = 345,
    right_alt = 346,
    right_super = 347,
    menu = 348
}
)";

constexpr string_view mouseKeys = R"(
MouseKey = 
{
    unknown   = -1,
    one       = 0,
    two       = 1,
    three     = 2,
    four      = 3,
    five      = 4,
    six       = 5,
    seven     = 6,
    eight     = 7,
    last      = 7,
    left      = 0,
    right     = 1,
    middle    = 2
}
)";

// Taking sol::object rather than int so a nil argument produces a sentence a
// script author can act on. Indexing a key that does not exist yields nil
// silently in Lua, so KeyboardKey.SPACE (a name from before the API became
// snake_case) reaches the binding as nil, and sol2's own diagnostic for that
// talks about stack indices and integer conversion.
static int KeyArgument( string_view function, const sol::object& key )
{
    if ( key.is<int>() )
        return key.as<int>();

    if ( key == sol::nil )
        throw std::runtime_error( std::format(
            "{}: key is nil - the name indexed out of KeyboardKey/MouseKey does not exist. "
            "The Lua API is snake_case: KeyboardKey.space, KeyboardKey.left_shift, "
            "KeyboardKey.end_key, MouseKey.left.", function ) );

    throw std::runtime_error( std::format(
        "{}: expected a KeyboardKey.* or MouseKey.* value, got a {}.",
        function, sol::type_name( key.lua_state(), key.get_type() ) ) );
}

constexpr string_view cursorModes = R"(
CursorMode =
{
    normal = 0,
    hidden = 1,
    locked = 2,
}
)"sv;

void CreateWindowInputBindings( WindowInput& input, sol::state& lua )
{
    lua.set( "is_key_clicked", [&]( const sol::object& keyObj ) {
        const int key = KeyArgument( "is_key_clicked"sv, keyObj );
        if ( key <= (int)MouseKey::LAST )
            return input.IsKeyClicked( MouseKey( key ) );
        return input.IsKeyClicked( KeyboardKey( key ) );
    } );

    lua.set( "is_key_pressed", [&]( const sol::object& keyObj ) {
        const int key = KeyArgument( "is_key_pressed"sv, keyObj );
        if ( key <= (int)MouseKey::LAST )
            return input.IsKeyPressed( MouseKey( key ) );
        return input.IsKeyPressed( KeyboardKey( key ) );
    } );

    // Mouse position (normalized 0..1 relative to window)
    lua.set( "mouse_pos_x", [&]() { return input.MousePos().x; } );
    lua.set( "mouse_pos_y", [&]() { return input.MousePos().y; } );

    // Mouse delta since last frame
    lua.set( "mouse_offset_x", [&]() { return input.MouseOffset().x; } );
    lua.set( "mouse_offset_y", [&]() { return input.MouseOffset().y; } );

    lua.safe_script( keyboardKeys );
    lua.safe_script( mouseKeys );
}

void CreateWindowBindings( Window& window, sol::state& lua )
{
    lua.set( "set_cursor_mode", [&]( i32 mode ) {
        if ( mode < (i32)CursorMode::Normal or mode > (i32)CursorMode::Locked )
            throw std::runtime_error(
                std::format( "set_cursor_mode: expected a CursorMode.* value, got {}.", mode ) );
        window.SetCursorMode( CursorMode( mode ) );
    } );
    lua.set( "get_cursor_mode", [&]() { return (i32)window.GetCursorMode(); } );

    lua.set( "lock_cursor", [&]( bool lock ) { window.LockCursor( lock ); } );
    lua.set( "hide_cursor", [&]( bool hide ) { window.HideCursor( hide ); } );

    lua.set( "center_cursor", [&]() { window.CenterCursor(); } );
    lua.set( "get_cursor_pos", [&]() { return window.GetCursorPos(); } );
    lua.set( "set_cursor_pos", [&]( const vec2& position ) { window.SetCursorPos( position ); } );

    lua.safe_script( cursorModes );
}

}


