#include "bubble/editor/editor_preferences.hpp"
#include "bubble/assets/asset_catalog.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/data/json.hpp"
#include "bubble/data/value_json.hpp"
#include "bubble/types/stream.hpp"

namespace bubble
{
namespace
{
void Describe( Reflection& reflection )
{
    using Flags = FieldInfo::Flags;
    reflection.Type<AppearancePreferences>( "appearance" )
        .Field<&AppearancePreferences::mFont>(
            "font", FieldInfo{ .mTooltip = "A .ttf or .otf file for the text; empty is the built-in Roboto" } )
        .Field<&AppearancePreferences::mFontSize>(
            "font_size", FieldInfo{ .mMin = 10, .mMax = 28, .mFlags = Flags::Slider, .mTooltip = "In pixels" } )
        .Field<&AppearancePreferences::mScale>(
            "scale", FieldInfo{ .mMin = 0.5f,
                                .mMax = 2.5f,
                                .mFlags = Flags::Slider,
                                .mTooltip = "Everything larger or smaller, on top of the display's own scale" } )
        .Field<&AppearancePreferences::mAccent>(
            "accent", FieldInfo{ .mTooltip = "The colour of what is selected, on, turned to" } )
        .Field<&AppearancePreferences::mTextIcon>(
            "text_icon", FieldInfo{ .mMin = 10,
                                    .mMax = 32,
                                    .mFlags = Flags::Slider,
                                    .mTooltip = "An icon among the text, as in the scene's tree, in pixels" } )
        .Field<&AppearancePreferences::mMarkerIcon>(
            "marker_icon", FieldInfo{ .mMin = 12,
                                      .mMax = 48,
                                      .mFlags = Flags::Slider,
                                      .mTooltip = "The icon the viewport marks a light, a camera, a sound with" } );
    reflection.Type<CameraPreferences>( "camera" )
        .Field<&CameraPreferences::mTurnSpeed>(
            "turn_speed", FieldInfo{ .mMin = 0.1f,
                                     .mMax = 5,
                                     .mFlags = Flags::Slider,
                                     .mTooltip = "How fast the mouse turns the view, flying and going round" } )
        .Field<&CameraPreferences::mInvertY>( "invert_y", FieldInfo{ .mTooltip = "The mouse up looks down" } )
        .Field<&CameraPreferences::mFastFactor>(
            "fast_factor",
            FieldInfo{
                .mMin = 1, .mMax = 20, .mFlags = Flags::Slider, .mTooltip = "How many times as fast Shift flies" } )
        .Field<&CameraPreferences::mZoomSpeed>( "zoom_speed",
                                                FieldInfo{ .mMin = 0.1f,
                                                           .mMax = 5,
                                                           .mFlags = Flags::Slider,
                                                           .mTooltip = "How far a notch of the wheel comes closer" } )
        .Field<&CameraPreferences::mFov>( "fov", FieldInfo{ .mMin = 20,
                                                            .mMax = 120,
                                                            .mFlags = Flags::Slider | Flags::Angle,
                                                            .mTooltip = "The field of view, up and down" } );
    reflection.Type<ViewportPreferences>( "viewport" )
        .Field<&ViewportPreferences::mGrid>( "grid", FieldInfo{ .mTooltip = "The floor's grid shown at first" } )
        .Field<&ViewportPreferences::mUi>( "ui",
                                           FieldInfo{ .mTooltip = "The scene's UI shown over the picture at first" } );
    reflection.Type<McpPreferences>( "mcp" )
        .Field<&McpPreferences::mServe>(
            "serve", FieldInfo{ .mTooltip = "Serve the Model Context Protocol to agents on 127.0.0.1, as --mcp does" } )
        .Field<&McpPreferences::mPort>( "port", FieldInfo{ .mMin = 1024, .mMax = 65535 } );
    reflection.Type<EditorPreferences>( "preferences" )
        .Field<&EditorPreferences::mAppearance>( "appearance" )
        .Field<&EditorPreferences::mCamera>( "camera" )
        .Field<&EditorPreferences::mViewport>( "viewport" )
        .Field<&EditorPreferences::mMcp>( "mcp" );
}

ValueType TypeOfPreferences( const Reflection& reflection )
{
    auto typeMaybe = reflection.TryValueTypeOf( reflection.TypeOf<EditorPreferences>() );
    if ( not typeMaybe )
        throw logic_error( format( "the editor's preferences: {}", typeMaybe.error() ) );
    return std::move( *typeMaybe );
}
}

Preferences::Preferences( opt<OsPath> fileMaybe ) : mFileMaybe( std::move( fileMaybe ) )
{
    Describe( mReflection );
    if ( not mFileMaybe )
        return;
    ifstream file( *mFileMaybe, ios::binary );
    // None yet: the defaults, written at the first change.
    if ( not file )
        return;
    ostringstream text;
    text << file.rdbuf();
    if ( auto readMaybe = TryParse( text.str() ); readMaybe )
        mValues = std::move( *readMaybe );
    else
        LogError( "{}: {}; the defaults instead", PathToUtf8( *mFileMaybe ), readMaybe.error() );
}

expected<EditorPreferences, string> Preferences::TryParse( string_view text ) const
{
    auto jsonMaybe = Json::TryParse( text );
    if ( not jsonMaybe )
        return unexpected( jsonMaybe.error() );
    const AssetCatalog none;
    auto valueMaybe = TryValueFromJson( *jsonMaybe, TypeOfPreferences( mReflection ), "", none );
    if ( not valueMaybe )
        return unexpected( valueMaybe.error() );
    auto madeMaybe = mReflection.TryFromValue( *valueMaybe, mReflection.TypeOf<EditorPreferences>() );
    if ( not madeMaybe )
        return unexpected( madeMaybe.error() );
    return madeMaybe->cast<EditorPreferences>();
}

string Preferences::ToJson( const EditorPreferences& preferences ) const
{
    EditorPreferences copy = preferences;
    auto valueMaybe = mReflection.TryToValue( mReflection.Ref( copy ) );
    if ( not valueMaybe )
        throw logic_error( format( "the editor's preferences: {}", valueMaybe.error() ) );
    const AssetCatalog none;
    auto jsonMaybe = TryValueToJson( *valueMaybe, TypeOfPreferences( mReflection ), "", none );
    if ( not jsonMaybe )
        throw logic_error( format( "the editor's preferences: {}", jsonMaybe.error() ) );
    return jsonMaybe->Write();
}

void Preferences::Save() const
{
    if ( not mFileMaybe )
        return;
    // Whole or not at all: a new file put in the old one's place.
    OsPath written = *mFileMaybe;
    written += ".new";
    {
        ofstream file( written, ios::binary | ios::trunc );
        file << ToJson( mValues );
        if ( not file )
        {
            LogError( "{}: could not be written", PathToUtf8( written ) );
            return;
        }
    }
    error_code error;
    fs::rename( written, *mFileMaybe, error );
    if ( error )
        LogError( "{}: {}", PathToUtf8( *mFileMaybe ), error.message() );
}
}
