// The ui module: canvases of entities laid out by flexbox, images and text
// as quads to draw, the pointer, the keyboard and the gamepad on buttons,
// toggles and sliders, scripts hearing them.
#include "bubble/import/import_cache.hpp"
#include "bubble/import/script_importer.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_declarations.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/ui/ui.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include "log_watch.hpp"
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

namespace
{
// A font made by hand: "A" half an em wide, its glyph a whole cell of an
// atlas that is inside everywhere; a space a quarter.
FontAsset HandFont()
{
    FontAsset font{ .mAtlas =
                        TextureAsset{ .mSrgb = false,
                                      .mMips = { TextureMip{
                                          .mWidth = 8, .mHeight = 8, .mPixels = vector<u8>( 8 * 8 * 4, 255 ) } } },
                    .mSize = 32,
                    .mRange = 4,
                    .mAscender = 0.8f,
                    .mDescender = -0.2f,
                    .mLineHeight = 1.0f,
                    .mGlyphs = {},
                    .mKerning = {} };
    font.mGlyphs.push_back( FontGlyph{ .mCodepoint = ' ', .mAdvance = 0.25f } );
    font.mGlyphs.push_back( FontGlyph{ .mCodepoint = 'A',
                                       .mAdvance = 0.5f,
                                       .mPlane = vec4( 0.0f, 0.0f, 0.5f, 0.7f ),
                                       .mAtlas = vec4( 0.0f, 0.0f, 8.0f, 8.0f ) } );
    return font;
}

struct UiFixture
{
    hmap<string, string> mFiles;
    AssetCatalog mCatalog;
    App mApp{ AppOptions{ .mThreads = 1 } };
    AssetRegistry mAssets{ mCatalog, AssetRegistry::AtOnce( [this]( const AssetId& assetId, string_view kind ) {
                               const auto found = mFiles.find( ImportedPath( assetId, kind ).View() );
                               if ( found == mFiles.end() )
                                   return expected<string, string>( unexpected( "no such file"s ) );
                               return expected<string, string>( found->second );
                           } ),
                           mApp.GetJobs() };
    vector<string> mRecorded;

    UiFixture()
    {
        RegisterScriptLoader( mAssets );
        RegisterTextureLoader( mAssets );
        RegisterFontLoader( mAssets );
        mApp.AddModule( TransformModule() );
        mApp.AddModule(
            ScriptModule( mAssets, ScriptModuleOptions{ .mGlobals =
                                                            [this]( LuaState& lua, World& ) {
                                                                lua.Globals()["record"] = [this]( string text ) {
                                                                    mRecorded.push_back( std::move( text ) );
                                                                };
                                                            },
                                                        .mAliases = {},
                                                        .mTimeLimit = 1.0,
                                                        .mProgress = {},
                                                        .mSaves = {},
                                                        .mGameVersion = {} } ) );
        mApp.AddModule( UiModule( mAssets ) );
    }

    AssetId Add( string_view path, string bytes, string_view kind )
    {
        const AssetPath assetPath = *AssetPath::TryFrom( path );
        if ( not mCatalog.TryIdOf( assetPath ) )
            mCatalog.Add( AssetId::Generate(), assetPath );
        const AssetId assetId = *mCatalog.TryIdOf( assetPath );
        mFiles[ImportedPath( assetId, kind ).String()] = std::move( bytes );
        return assetId;
    }

    AssetId Font() { return Add( "fonts/hand.ttf", WriteFontAsset( HandFont() ), cFontKind ); }

    AssetId Image( u32 size )
    {
        return Add( "images/frame.png",
                    WriteTextureAsset( TextureAsset{
                        .mSrgb = true,
                        .mMips = { TextureMip{
                            .mWidth = size, .mHeight = size, .mPixels = vector<u8>( size * size * 4, 255 ) } } } ),
                    cTextureKind );
    }

    AssetId Script( string_view path, string_view source )
    {
        mFiles[string( path )] = string( source );
        const AssetPath assetPath = *AssetPath::TryFrom( path );
        if ( not mCatalog.TryIdOf( assetPath ) )
            mCatalog.Add( AssetId::Generate(), assetPath );
        const AssetId assetId = *mCatalog.TryIdOf( assetPath );
        const auto importedMaybe = TryImportAsset( ProjectFiles::InMemory( mFiles ), ScriptImporter(), assetPath,
                                                   assetId, Json( Json::Object() ) );
        REQUIRE_MESSAGE( importedMaybe, importedMaybe.error() );
        return assetId;
    }

    static Entity Child( Scene& scene, Entity parent, string_view name )
    {
        return CreateChild( scene, parent, *Text::TryFrom( name ) );
    }

    static Entity Canvas( Scene& scene, UiCanvas canvas = {} )
    {
        const Entity entity = Child( scene, scene.Root(), "menu" );
        scene.Add<UiCanvas>( entity, canvas );
        return entity;
    }

    static Entity Box( Scene& scene, Entity parent, string_view name, UiRect rect )
    {
        const Entity entity = Child( scene, parent, name );
        scene.Add<UiRect>( entity, std::move( rect ) );
        return entity;
    }

    // Frames with no time: loads come, the UI is laid out.
    void Frames( World& world, u32 count = 4 )
    {
        for ( u32 frame = 0; frame < count; ++frame )
        {
            mApp.GetJobs().RunMainQueue();
            world.Frame( 0.0 );
        }
    }

    static UiBox BoxOf( World& world, Entity entity )
    {
        const auto boxMaybe = world.Get<UiWorld>().TryBox( entity );
        REQUIRE( boxMaybe );
        return *boxMaybe;
    }

    static UiRect Sized( string_view width, string_view height )
    {
        UiRect rect;
        rect.mWidth = string( width );
        rect.mHeight = string( height );
        return rect;
    }

    // A click of the left button at a point of the screen.
    static bool Click( World& world, vec2 at )
    {
        UiWorld& ui = world.Get<UiWorld>();
        ui.Take( MouseMove{ .mPosition = at, .mDelta = vec2( 0.0f ) } );
        const bool down = ui.Take( MouseDown{ .mButton = MouseButton::Left, .mPosition = at } );
        const bool up = ui.Take( MouseUp{ .mButton = MouseButton::Left, .mPosition = at } );
        return down and up;
    }
};
}

TEST_CASE( "A canvas lays its elements out by flexbox: sizes, shares, rows, growth, padding, pinned corners" )
{
    UiFixture f;
    Scene scene;
    const Entity canvas = UiFixture::Canvas( scene );
    UiRect panelRect = UiFixture::Sized( "50%", "200" );
    panelRect.mPadding = 10;
    panelRect.mDirection = Name::Declare( "row" );
    panelRect.mGap = 20;
    const Entity panel = UiFixture::Box( scene, canvas, "panel", panelRect );
    const Entity fixed = UiFixture::Box( scene, panel, "fixed", UiFixture::Sized( "100", "auto" ) );
    UiRect growing = UiFixture::Sized( "auto", "50" );
    growing.mGrow = 1;
    const Entity grows = UiFixture::Box( scene, panel, "grows", growing );
    UiRect corner = UiFixture::Sized( "40", "30" );
    corner.mPosition = Name::Declare( "absolute" );
    corner.mRight = "0";
    corner.mBottom = "0";
    const Entity pinned = UiFixture::Box( scene, canvas, "pinned", corner );
    World world( f.mApp, std::move( scene ) );
    f.Frames( world );

    // A screen as large as the canvas: its pixels are the screen's.
    CHECK( UiFixture::BoxOf( world, canvas ).mMax == vec2( 1280.0f, 720.0f ) );
    const UiBox panelBox = UiFixture::BoxOf( world, panel );
    CHECK( panelBox.mMin == vec2( 0.0f ) );
    CHECK( panelBox.mMax == vec2( 640.0f, 200.0f ) );
    // In a row inside the padding: the fixed one stretched across, the other
    // taking what is left after the gap.
    CHECK( UiFixture::BoxOf( world, fixed ).mMin == vec2( 10.0f, 10.0f ) );
    CHECK( UiFixture::BoxOf( world, fixed ).mMax == vec2( 110.0f, 190.0f ) );
    CHECK( UiFixture::BoxOf( world, grows ).mMin == vec2( 130.0f, 10.0f ) );
    CHECK( UiFixture::BoxOf( world, grows ).mMax == vec2( 630.0f, 60.0f ) );
    CHECK( UiFixture::BoxOf( world, pinned ).mMin == vec2( 1240.0f, 690.0f ) );

    // A wider screen: the canvas keeps its height and grows across, its
    // pinned corner with it.
    world.Get<UiWorld>().SetScreen( ivec2( 1920, 720 ) );
    f.Frames( world, 1 );
    CHECK( UiFixture::BoxOf( world, canvas ).mMax == vec2( 1920.0f, 720.0f ) );
    CHECK( UiFixture::BoxOf( world, pinned ).mMin == vec2( 1880.0f, 690.0f ) );
    // Half as tall: the canvas scaled, its pixels the same.
    world.Get<UiWorld>().SetScreen( ivec2( 960, 360 ) );
    f.Frames( world, 1 );
    CHECK( UiFixture::BoxOf( world, canvas ).mMax == vec2( 1920.0f, 720.0f ) );

    // Hidden: no box, for it and what is under it.
    world.GetScene().Get<UiRect>( panel ).mVisible = false;
    f.Frames( world, 1 );
    CHECK_FALSE( world.Get<UiWorld>().TryBox( panel ) );
    CHECK_FALSE( world.Get<UiWorld>().TryBox( fixed ) );
}

TEST_CASE( "Images and text are quads to draw: tinted, sliced in nine, glyphs as large as the text, wrapped" )
{
    UiFixture f;
    const AssetId font = f.Font();
    const AssetId image = f.Image( 16 );
    Scene scene;
    const Entity canvas = UiFixture::Canvas( scene );
    const Entity panel = UiFixture::Box( scene, canvas, "panel", UiFixture::Sized( "200", "100" ) );
    scene.Add<UiImage>( panel, UiImage{ .mImage = AssetId{}, .mColor = Color{ 1.0f, 0.0f, 0.0f, 1.0f }, .mSlice = 0 } );
    const Entity frame = UiFixture::Box( scene, canvas, "frame", UiFixture::Sized( "100", "100" ) );
    scene.Add<UiImage>( frame, UiImage{ .mImage = image, .mColor = Color{}, .mSlice = 4 } );
    // As large as its text: "AA A" is 0.5 + 0.5 + 0.25 + 0.5 ems of 20 pixels.
    const Entity label = UiFixture::Box( scene, canvas, "label", UiFixture::Sized( "auto", "auto" ) );
    UiRect labelRect = UiFixture::Sized( "auto", "auto" );
    labelRect.mAlignSelf = Name::Declare( "start" );
    scene.Get<UiRect>( label ) = labelRect;
    scene.Add<UiText>( label, UiText{ .mText = "AA A",
                                      .mFont = font,
                                      .mSize = 20,
                                      .mColor = Color{},
                                      .mAlign = Name::Declare( "start" ),
                                      .mWrap = true } );
    // Narrower than its text: wrapped at the space.
    const Entity wrapped = UiFixture::Box( scene, canvas, "wrapped", UiFixture::Sized( "30", "auto" ) );
    scene.Add<UiText>( wrapped, UiText{ .mText = "AA A",
                                        .mFont = font,
                                        .mSize = 20,
                                        .mColor = Color{},
                                        .mAlign = Name::Declare( "end" ),
                                        .mWrap = true } );
    World world( f.mApp, std::move( scene ) );
    f.Frames( world );

    const UiScene& drawn = world.Get<UiScene>();
    REQUIRE( drawn.mCanvases.size() == 1 );
    CHECK_FALSE( drawn.mCanvases[0].mWorld );
    // The panel's color in linear light.
    const auto count = [&]( UiQuadKind kind ) {
        return ranges::count_if( drawn.mQuads, [&]( const UiQuad& quad ) { return quad.mKind == kind; } );
    };
    REQUIRE( not drawn.mQuads.empty() );
    CHECK( drawn.mQuads[0].mColor == vec4( 1.0f, 0.0f, 0.0f, 1.0f ) );
    CHECK( drawn.mQuads[0].mMax == vec2( 200.0f, 100.0f ) );
    // Nine for the frame, its corners 4 pixels and a quarter of the image.
    CHECK( drawn.mQuads[1].mMax == drawn.mQuads[1].mMin + vec2( 4.0f ) );
    CHECK( drawn.mQuads[1].mUvMax == vec2( 0.25f ) );
    CHECK( count( UiQuadKind::Image ) == 10 );

    const UiBox labelBox = UiFixture::BoxOf( world, label );
    CHECK( labelBox.mMax - labelBox.mMin == vec2( 35.0f, 20.0f ) );
    const UiBox wrappedBox = UiFixture::BoxOf( world, wrapped );
    CHECK( wrappedBox.mMax.y - wrappedBox.mMin.y == 40.0f );
    // Six glyphs of A; the second text's last line, one A, against its end.
    CHECK( count( UiQuadKind::Glyph ) == 6 );
    const UiQuad& last = drawn.mQuads.back();
    CHECK( last.mMax.x == doctest::Approx( wrappedBox.mMax.x ) );
    CHECK( last.mMin.y == doctest::Approx( wrappedBox.mMin.y + 20.0f + ( 0.8f - 0.7f ) * 20.0f ) );
}

TEST_CASE( "A click on a button calls its script's on_click; a press on a panel is the UI's, past it the game's" )
{
    UiFixture f;
    const AssetId script = f.Script( "scripts/start.luau", R"(
function on_click( self )
    record( "clicked " .. self.entity.name )
end
)" );
    Scene scene;
    const Entity canvas = UiFixture::Canvas( scene );
    const Entity panel = UiFixture::Box( scene, canvas, "panel", UiFixture::Sized( "400", "300" ) );
    scene.Add<UiImage>( panel );
    const Entity start = UiFixture::Box( scene, panel, "start", UiFixture::Sized( "200", "50" ) );
    scene.Add<UiImage>( start );
    scene.Add<UiButton>( start );
    scene.Add<Script>( start, Script{ .mAsset = script } );
    const Entity off = UiFixture::Box( scene, panel, "off", UiFixture::Sized( "200", "50" ) );
    scene.Add<UiButton>( off, UiButton{ .mDisabled = true } );
    World world( f.mApp, std::move( scene ) );
    f.Frames( world );
    UiWorld& ui = world.Get<UiWorld>();

    // Hovered, then pressed: its image tinted so.
    ui.Take( MouseMove{ .mPosition = vec2( 100.0f, 25.0f ), .mDelta = vec2( 0.0f ) } );
    CHECK( ui.Hovered() == start );
    f.Frames( world, 1 );
    const auto tintOf = [&]( Entity ) {
        for ( const UiQuad& quad : world.Get<UiScene>().mQuads )
            if ( quad.mMin == vec2( 0.0f ) and quad.mMax == vec2( 200.0f, 50.0f ) )
                return quad.mColor;
        return vec4( -1.0f );
    };
    CHECK( tintOf( start ).r < 1.0f );

    CHECK( UiFixture::Click( world, vec2( 100.0f, 25.0f ) ) );
    f.Frames( world, 1 );
    CHECK( f.mRecorded == vector<string>{ "clicked start" } );
    // Pressed on the button, let go off it: no click.
    ui.Take( MouseDown{ .mButton = MouseButton::Left, .mPosition = vec2( 100.0f, 25.0f ) } );
    ui.Take( MouseUp{ .mButton = MouseButton::Left, .mPosition = vec2( 300.0f, 250.0f ) } );
    // A disabled one is no button at all; the panel under it takes the press,
    // and its release.
    CHECK( UiFixture::Click( world, vec2( 100.0f, 75.0f ) ) );
    f.Frames( world, 1 );
    CHECK( f.mRecorded.size() == 1 );
    // Past the UI: the game's.
    CHECK_FALSE( ui.Take( MouseDown{ .mButton = MouseButton::Left, .mPosition = vec2( 900.0f, 600.0f ) } ) );
    CHECK_FALSE( ui.Take( MouseUp{ .mButton = MouseButton::Left, .mPosition = vec2( 900.0f, 600.0f ) } ) );
    // A finger, its positions shares of the screen.
    CHECK( ui.Take( TouchDown{ .mFinger = 1, .mPosition = vec2( 100.0f / 1280.0f, 25.0f / 720.0f ) } ) );
    CHECK( ui.Take( TouchUp{ .mFinger = 1, .mPosition = vec2( 100.0f / 1280.0f, 25.0f / 720.0f ) } ) );
    f.Frames( world, 1 );
    CHECK( f.mRecorded == vector<string>{ "clicked start", "clicked start" } );
}

TEST_CASE( "The arrows and a gamepad's pad move the focus between buttons, Enter and south press them, Tab goes round" )
{
    UiFixture f;
    const AssetId script = f.Script( "scripts/item.luau", R"(
function on_click( self )
    record( self.entity.name )
end
)" );
    Scene scene;
    const Entity canvas = UiFixture::Canvas( scene );
    vector<Entity> items;
    for ( const string_view name : { "play", "options", "quit" } )
    {
        const Entity item = UiFixture::Box( scene, canvas, name, UiFixture::Sized( "200", "40" ) );
        scene.Add<UiButton>( item );
        scene.Add<Script>( item, Script{ .mAsset = script } );
        items.push_back( item );
    }
    const Entity hud = UiFixture::Canvas( scene, UiCanvas{ .mNavigation = false } );
    const Entity ammo = UiFixture::Box( scene, hud, "ammo", UiFixture::Sized( "50", "50" ) );
    scene.Add<UiButton>( ammo );
    World world( f.mApp, std::move( scene ) );
    f.Frames( world );
    UiWorld& ui = world.Get<UiWorld>();

    CHECK( ui.Focused() == Entity::Null );
    CHECK( ui.Take( KeyDown{ .mKey = Key::Down } ) );
    CHECK( ui.Take( KeyUp{ .mKey = Key::Down } ) );
    CHECK( ui.Focused() == items[0] );
    ui.Take( KeyDown{ .mKey = Key::Down } );
    ui.Take( GamepadButtonDown{ .mGamepad = 0, .mButton = GamepadButton::DpadDown } );
    CHECK( ui.Focused() == items[2] );
    // Nothing further down: it stays.
    ui.Take( KeyDown{ .mKey = Key::Down } );
    CHECK( ui.Focused() == items[2] );
    ui.Take( KeyDown{ .mKey = Key::Up } );
    CHECK( ui.Take( KeyDown{ .mKey = Key::Enter } ) );
    CHECK( ui.Take( GamepadButtonDown{ .mGamepad = 0, .mButton = GamepadButton::South } ) );
    ui.Take( KeyDown{ .mKey = Key::Tab } );
    ui.Take( KeyDown{ .mKey = Key::Tab } );
    CHECK( ui.Focused() == items[0] );
    f.Frames( world, 1 );
    CHECK( f.mRecorded == vector<string>{ "options", "options" } );
    // Keys the UI does not use are the game's.
    CHECK_FALSE( ui.Take( KeyDown{ .mKey = Key::W } ) );
    // The HUD's button is no stop on the way.
    ui.Focus( items[2] );
    ui.Take( KeyDown{ .mKey = Key::Right } );
    CHECK( ui.Focused() == items[2] );
}

TEST_CASE( "A toggle turns over and a slider slides, by the pointer and the arrows, each telling on_change" )
{
    UiFixture f;
    const AssetId script = f.Script( "scripts/setting.luau", R"(
function on_change( self, value: number | boolean )
    record( self.entity.name .. " " .. tostring( value ) )
end
)" );
    Scene scene;
    const Entity canvas = UiFixture::Canvas( scene );
    const Entity sound = UiFixture::Box( scene, canvas, "sound", UiFixture::Sized( "40", "40" ) );
    scene.Add<UiToggle>( sound );
    scene.Add<Script>( sound, Script{ .mAsset = script } );
    const Entity volume = UiFixture::Box( scene, canvas, "volume", UiFixture::Sized( "200", "20" ) );
    scene.Add<UiSlider>( volume, UiSlider{ .mValue = 0.5f, .mMin = 0, .mMax = 10, .mStep = 1 } );
    scene.Add<UiImage>( volume );
    scene.Add<Script>( volume, Script{ .mAsset = script } );
    World world( f.mApp, std::move( scene ) );
    f.Frames( world );
    UiWorld& ui = world.Get<UiWorld>();

    CHECK( UiFixture::Click( world, vec2( 20.0f, 20.0f ) ) );
    CHECK( world.GetScene().Get<UiToggle>( sound ).mOn );
    // Dragged a quarter across, by whole steps.
    ui.Take( MouseMove{ .mPosition = vec2( 10.0f, 50.0f ), .mDelta = vec2( 0.0f ) } );
    ui.Take( MouseDown{ .mButton = MouseButton::Left, .mPosition = vec2( 10.0f, 50.0f ) } );
    ui.Take( MouseMove{ .mPosition = vec2( 52.0f, 50.0f ), .mDelta = vec2( 42.0f, 0.0f ) } );
    ui.Take( MouseUp{ .mButton = MouseButton::Left, .mPosition = vec2( 52.0f, 50.0f ) } );
    CHECK( world.GetScene().Get<UiSlider>( volume ).mValue == 3.0f );
    // A step by the arrows, the focus on it.
    ui.Focus( volume );
    CHECK( ui.Take( KeyDown{ .mKey = Key::Right } ) );
    CHECK( world.GetScene().Get<UiSlider>( volume ).mValue == 4.0f );
    f.Frames( world, 2 );
    CHECK( f.mRecorded == vector<string>{ "sound true", "volume 1", "volume 3", "volume 4" } );
    // Its fill as far as its value.
    for ( const UiQuad& quad : world.Get<UiScene>().mQuads )
        if ( quad.mMin == vec2( 0.0f, 40.0f ) and quad.mMax.y == 60.0f and quad.mMax.x < 200.0f )
            CHECK( quad.mMax.x == doctest::Approx( 80.0f ) );
}

TEST_CASE( "A world canvas puts its pixels about its entity, Y down; a wrong length is said once" )
{
    LogWatch log;
    UiFixture f;
    Scene scene;
    const Entity canvas =
        UiFixture::Canvas( scene, UiCanvas{ .mWidth = 200, .mHeight = 50, .mWorld = true, .mPixelsPerUnit = 100 } );
    scene.Add<Transform>( canvas, Transform{ .mPosition = { 0.0f, 2.0f, 0.0f } } );
    UiRect wrong = UiFixture::Sized( "12px", "10" );
    wrong.mDirection = Name::Declare( "diagonal" );
    const Entity bar = UiFixture::Box( scene, canvas, "bar", wrong );
    World world( f.mApp, std::move( scene ) );
    f.Frames( world, 5 );

    const UiCanvasView& view = world.Get<UiScene>().mCanvases.at( 0 );
    CHECK( view.mWorld );
    // Its top left corner a unit left and a quarter up of the entity.
    CHECK( glm::length( vec3( view.mToWorld * vec4( 0.0f, 0.0f, 0.0f, 1.0f ) ) - vec3( -1.0f, 2.25f, 0.0f ) ) < 1e-5f );
    CHECK( glm::length( vec3( view.mToWorld * vec4( 200.0f, 50.0f, 0.0f, 1.0f ) ) - vec3( 1.0f, 1.75f, 0.0f ) ) <
           1e-5f );
    size_t said = 0;
    for ( const LogEntry& entry : log.Entries() )
        said += entry.mText.find( "/menu/bar: ui_rect.width: \"12px\" is not \"auto\", pixels (\"120\") or a share "
                                  "(\"50%\"); ui_rect.direction: diagonal is not one of column, row" ) != string::npos;
    CHECK( said == 1 );
    // Laid out still, as well as it can be.
    CHECK( UiFixture::BoxOf( world, bar ).mMax.y == 10.0f );
}

TEST_CASE( "What the UI gives scripts is declared, and ui.focus moves the focus" )
{
    UiFixture f;
    const string declarations = WriteScriptDeclarations( f.mApp );
    CHECK( declarations.find( "declare ui: {\n    focus: (target: entity?) -> (),\n" ) != string::npos );
    CHECK( declarations.find( "--     on_click( self )\n" ) != string::npos );
    CHECK( declarations.find( "--     on_change( self, value: number | boolean )\n" ) != string::npos );
    CHECK( declarations.find( "declare extern type ui_text with\n" ) != string::npos );

    const AssetId script = f.Script( "scripts/menu.luau", R"(
function on_start( self )
    ui.focus( self.entity )
end
function on_update( self, dt: number )
    record( tostring( ui.focused() == self.entity ) )
end
)" );
    Scene scene;
    const Entity canvas = UiFixture::Canvas( scene );
    const Entity play = UiFixture::Box( scene, canvas, "play", UiFixture::Sized( "100", "40" ) );
    scene.Add<UiButton>( play );
    scene.Add<Script>( play, Script{ .mAsset = script } );
    World world( f.mApp, std::move( scene ) );
    f.Frames( world, 4 );
    REQUIRE( not f.mRecorded.empty() );
    CHECK( f.mRecorded.back() == "true" );
    CHECK( world.Get<UiWorld>().Focused() == play );
}
