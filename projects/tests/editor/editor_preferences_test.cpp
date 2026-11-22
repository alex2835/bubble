// The editor's preferences: read from their file - what it leaves out at
// the default, what it gets wrong told by the field -, written back the
// same, and the file in the user's folder.
#include "bubble/editor/editor_preferences.hpp"
#include "bubble/editor/preferences_panel.hpp"
#include "bubble/types/stream.hpp"
#include "log_watch.hpp"
#include <doctest.h>

using namespace bubble;
using bubble::test::LogWatch;

TEST_CASE( "The editor's preferences read what the file says and keep the defaults for the rest" )
{
    const Preferences preferences( nullopt );
    const auto readMaybe = preferences.TryParse( R"({
        "appearance": { "font_size": 18, "accent": [0.2, 0.7, 0.6, 1] },
        "camera": { "invert_y": true, "fov": 1.2 }
    })" );
    REQUIRE_MESSAGE( readMaybe, readMaybe.error() );
    CHECK( readMaybe->mAppearance.mFontSize == 18.0f );
    CHECK( readMaybe->mAppearance.mAccent.mG == doctest::Approx( 0.7f ) );
    CHECK( readMaybe->mAppearance.mScale == 1.0f );
    CHECK( readMaybe->mAppearance.mFont.empty() );
    CHECK( readMaybe->mCamera.mInvertY );
    CHECK( readMaybe->mCamera.mFov == doctest::Approx( 1.2f ) );
    CHECK( readMaybe->mCamera.mTurnSpeed == 1.0f );
    CHECK( readMaybe->mMcp.mPort == 8752 );

    const auto emptyMaybe = preferences.TryParse( "{}" );
    REQUIRE( emptyMaybe );
    CHECK( emptyMaybe->mViewport.mGrid );
}

TEST_CASE( "The editor's preferences written are read back as they were" )
{
    const Preferences preferences( nullopt );
    EditorPreferences changed;
    changed.mAppearance.mFont = "C:/Fonts/Inter.ttf";
    changed.mAppearance.mMarkerIcon = 30.0f;
    changed.mCamera.mFastFactor = 8.0f;
    changed.mViewport.mUi = false;
    changed.mMcp = McpPreferences{ .mServe = true, .mPort = 9000 };

    const auto readMaybe = preferences.TryParse( preferences.ToJson( changed ) );
    REQUIRE_MESSAGE( readMaybe, readMaybe.error() );
    CHECK( readMaybe->mAppearance == changed.mAppearance );
    CHECK( readMaybe->mCamera.mFastFactor == 8.0f );
    CHECK( not readMaybe->mViewport.mUi );
    CHECK( readMaybe->mMcp.mServe );
    CHECK( readMaybe->mMcp.mPort == 9000 );
}

TEST_CASE( "A preferences file that gets a field wrong says which" )
{
    const Preferences preferences( nullopt );
    const auto wrongMaybe = preferences.TryParse( R"({ "camera": { "turn_speed": "fast" } })" );
    REQUIRE_FALSE( wrongMaybe );
    CHECK_MESSAGE( wrongMaybe.error().find( "turn_speed" ) != string::npos, wrongMaybe.error() );
    const auto unknownMaybe = preferences.TryParse( R"({ "camera": { "speeed": 2 } })" );
    REQUIRE_FALSE( unknownMaybe );
    CHECK_MESSAGE( unknownMaybe.error().find( "speeed" ) != string::npos, unknownMaybe.error() );
}

TEST_CASE( "The editor's preferences are kept in their file, and a broken one leaves the defaults" )
{
    const OsPath folder = fs::temp_directory_path() / "bubble_preferences_test";
    fs::create_directories( folder );
    const OsPath file = folder / Preferences::cFileName;
    fs::remove( file );
    {
        Preferences preferences( file );
        CHECK( preferences.Get().mAppearance.mFontSize == 15.0f );
        preferences.Get().mAppearance.mFontSize = 20.0f;
        preferences.Save();
    }
    {
        const Preferences preferences( file );
        CHECK( preferences.Get().mAppearance.mFontSize == 20.0f );
    }
    ofstream( file, ios::binary | ios::trunc ) << "{ \"appearance\": ";
    {
        LogWatch watch;
        const Preferences preferences( file );
        CHECK( preferences.Get().mAppearance.mFontSize == 15.0f );
        CHECK( watch.Saw( LogLevel::Error, { "editor.json", "the defaults instead" } ) );
    }
    fs::remove_all( folder );
}

TEST_CASE( "A preference's name reads as words" )
{
    CHECK( PreferenceLabel( "font_size" ) == "Font size" );
    CHECK( PreferenceLabel( "fov" ) == "Fov" );
}
