#pragma once
#include "bubble/core/color.hpp"
#include "bubble/reflection/reflection.hpp"
#include "bubble/types/filesystem.hpp"
#include "bubble/types/string.hpp"

namespace bubble
{
// One person's settings of the editor, for every project: editor.json in
// the user's folder (SDL's pref path, %APPDATA%\bubble\editor on Windows),
// edited in Edit > Preferences and taking effect at once. What a project
// shares - its game's settings - is project.bubble's; what the editor
// remembers of a project for itself is in the project's .bubble.

// How the editor looks.
struct AppearancePreferences
{
    // A TrueType or OpenType file for the text; none is the built-in Roboto.
    string mFont;
    f32 mFontSize = 15.0f;
    // On top of the display's own scale: everything larger or smaller.
    f32 mScale = 1.0f;
    // The one colour that marks what is selected, on, turned to.
    Color mAccent{ .mR = 0.96f, .mG = 0.58f, .mB = 0.18f, .mA = 1.0f };
    // An icon among the text - a row of the scene's tree.
    f32 mTextIcon = 17.0f;
    // The icon the viewport marks a light, a camera, a sound with.
    f32 mMarkerIcon = 24.0f;

    bool operator==( const AppearancePreferences& ) const = default;
};

// How the editor's camera follows the mouse and the keys.
struct CameraPreferences
{
    // How fast the mouse turns the view: flying, and going round.
    f32 mTurnSpeed = 1.0f;
    bool mInvertY = false;
    // How many times as fast Shift flies.
    f32 mFastFactor = 4.0f;
    // How far a notch of the wheel comes closer.
    f32 mZoomSpeed = 1.0f;
    // The field of view, up and down, radians.
    f32 mFov = 1.0f;
};

// How a viewport starts.
struct ViewportPreferences
{
    bool mGrid = true;
    bool mUi = true;
};

// The MCP server for agents, which --mcp starts too.
struct McpPreferences
{
    bool mServe = false;
    u32 mPort = 8752;
};

struct EditorPreferences
{
    AppearancePreferences mAppearance;
    CameraPreferences mCamera;
    ViewportPreferences mViewport;
    McpPreferences mMcp;
};

// The preferences, the reflection that reads, writes and shows them, and
// their file.
class Preferences
{
public:
    // Read from `fileMaybe` when it is there: what it gets wrong is logged
    // and left at its default. None: kept in memory alone, as tests do.
    explicit Preferences( opt<OsPath> fileMaybe );

    EditorPreferences& Get() { return mValues; }
    const EditorPreferences& Get() const { return mValues; }
    const Reflection& GetReflection() const { return mReflection; }
    Reflection& GetReflection() { return mReflection; }
    MetaAny Ref() { return mReflection.Ref( mValues ); }

    // Written to the file whole; a failure logged.
    void Save() const;

    // The preferences a file holds, every field it leaves out at its
    // default: "camera.turn_speed: a number expected, got "fast"".
    expected<EditorPreferences, string> TryParse( string_view text ) const;
    // As a file holds them.
    string ToJson( const EditorPreferences& preferences ) const;

    // The file in the user's folder, as Platform::TrySaveFolder gives it.
    static constexpr string_view cFileName = "editor.json";

private:
    Reflection mReflection;
    EditorPreferences mValues;
    opt<OsPath> mFileMaybe;
};
}
