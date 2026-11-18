// The UI drawn: a screen canvas over the picture, its panel and its text in
// their colors, a world canvas among the scene's surfaces. Not on the web, as
// gpu_test.cpp.
#ifndef __EMSCRIPTEN__
#include "render/render_fixture.hpp"
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

namespace
{
constexpr i32 cSize = 64;

// An atlas inside everywhere: a glyph draws its whole quad.
FontAsset SolidFont()
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
    font.mGlyphs.push_back( FontGlyph{ .mCodepoint = 'A',
                                       .mAdvance = 0.5f,
                                       .mPlane = vec4( 0.0f, 0.0f, 0.5f, 0.7f ),
                                       .mAtlas = vec4( 0.0f, 0.0f, 8.0f, 8.0f ) } );
    return font;
}

UiRect Placed( string_view left, string_view top, string_view width, string_view height )
{
    UiRect rect;
    rect.mPosition = Name::Declare( "absolute" );
    rect.mLeft = string( left );
    rect.mTop = string( top );
    rect.mWidth = string( width );
    rect.mHeight = string( height );
    return rect;
}

// Frames of the world, each drawn with its UI; the last one's pixels.
vector<u8> DrawWithUi( RenderFixture& f, World& world, u32 frames = 6 )
{
    world.Get<UiWorld>().SetScreen( ivec2( cSize ) );
    for ( u32 frame = 0; frame < frames; ++frame )
    {
        world.Frame( 1.0 / 60.0 );
        f.mApp.GetJobs().RunMainQueue();
        const RenderScene& scene = world.Get<RenderScene>();
        Required( f.mGpu->TryCheckNow( [&] {
            f.mRenderer.Draw(
                f.mView, scene, scene.mCameras.front(),
                RenderTarget{ .mView = f.mTarget.View(), .mSize = f.mTarget.Size(), .mFormat = f.mTarget.Format() },
                world.Get<UiScene>() );
        } ) );
        f.mGpu->Poll();
    }
    return Required( f.mGpu->TryReadRgba8( f.mTarget ) );
}
}

TEST_CASE( "A screen canvas's panel and text are drawn over the picture, a world canvas in the scene" )
{
    RenderFixture f( cSize );
    RegisterFontLoader( f.mAssets );
    f.mApp.AddModule( UiModule( f.mAssets ) );
    const AssetId font = f.Import( "fonts/solid.ttf", WriteFontAsset( SolidFont() ), cFontKind );

    Scene scene;
    const Entity camera = CreateChild( scene, scene.Root(), Make( "camera" ) );
    scene.Add<Transform>( camera, Transform{ .mPosition = { 0.0f, 0.0f, 5.0f } } );
    scene.Add<Camera>( camera );
    // On the screen, its pixels the screen's.
    const Entity hud = CreateChild( scene, scene.Root(), Make( "hud" ) );
    scene.Add<UiCanvas>( hud, UiCanvas{ .mWidth = 64, .mHeight = 64 } );
    const Entity panel = CreateChild( scene, hud, Make( "panel" ) );
    scene.Add<UiRect>( panel, Placed( "4", "4", "16", "16" ) );
    scene.Add<UiImage>( panel, UiImage{ .mImage = AssetId{}, .mColor = Color{ 1.0f, 0.0f, 0.0f, 1.0f }, .mSlice = 0 } );
    const Entity label = CreateChild( scene, hud, Make( "label" ) );
    scene.Add<UiRect>( label, Placed( "36", "4", "auto", "auto" ) );
    scene.Add<UiText>( label, UiText{ .mText = "A",
                                      .mFont = font,
                                      .mSize = 24,
                                      .mColor = Color{ 0.0f, 1.0f, 0.0f, 1.0f },
                                      .mAlign = Name::Declare( "start" ),
                                      .mWrap = false } );
    // In the world at the origin, a unit across, facing the camera.
    const Entity sign = CreateChild( scene, scene.Root(), Make( "sign" ) );
    scene.Add<Transform>( sign );
    scene.Add<UiCanvas>( sign, UiCanvas{ .mWidth = 100, .mHeight = 100, .mWorld = true, .mPixelsPerUnit = 100 } );
    const Entity board = CreateChild( scene, sign, Make( "board" ) );
    scene.Add<UiRect>( board, Placed( "0", "0", "100%", "100%" ) );
    scene.Add<UiImage>( board, UiImage{ .mImage = AssetId{}, .mColor = Color{ 0.0f, 0.0f, 1.0f, 1.0f }, .mSlice = 0 } );
    World world( f.mApp, std::move( scene ) );

    const vector<u8> pixels = DrawWithUi( f, world );
    const vec3 background = ColorAt( pixels, cSize, 2, 60 );
    // The panel, red as picked: the screen's UI is not tonemapped.
    CHECK( glm::length( ColorAt( pixels, cSize, 12, 12 ) - vec3( 1.0f, 0.0f, 0.0f ) ) < 0.02f );
    // The glyph, 12 wide from 36 and from 4 + 0.1 em down.
    CHECK( glm::length( ColorAt( pixels, cSize, 40, 16 ) - vec3( 0.0f, 1.0f, 0.0f ) ) < 0.05f );
    CHECK( glm::length( ColorAt( pixels, cSize, 52, 16 ) - background ) < 0.02f );
    // The sign in the middle of the picture, blue through the tonemapping.
    const vec3 middle = ColorAt( pixels, cSize, 32, 32 );
    CHECK( middle.b > 0.3f );
    CHECK( middle.b > middle.r + 0.2f );
    CHECK( glm::length( middle - background ) > 0.1f );
}
#endif
