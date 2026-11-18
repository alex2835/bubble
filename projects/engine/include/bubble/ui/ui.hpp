#pragma once
#include "bubble/assets/asset_registry.hpp"
#include "bubble/assets/font_asset.hpp"
#include "bubble/assets/texture_asset.hpp"
#include "bubble/core/color.hpp"
#include "bubble/core/name.hpp"
#include "bubble/input/input_event.hpp"
#include "bubble/scene/entity.hpp"
#include "bubble/types/matrix.hpp"
#include "bubble/world/module.hpp"

namespace bubble
{
// A game's UI is entities of the scene, as Godot's Controls are: a canvas,
// and under it entities with ui_rect - laid out by flexbox (Yoga) - drawing
// images and text, some of them buttons, toggles and sliders. Prefabs,
// operators, the inspector, undo and Luau take it as they take the rest.
// Lengths are in the canvas's pixels.

// Where a UI is: on the screen, its height the screen's - a canvas `height`
// pixels high fills it, as wide as the screen's shape makes it -, or in the
// world at its entity, `width` by `height` pixels about it, `pixels_per_unit`
// to a unit - a health bar over an enemy. With `navigation` the keyboard's
// arrows and a gamepad's pad move between its buttons; a HUD leaves them to
// the game. Canvases of a higher `order` are over the others.
struct UiCanvas
{
    f32 mWidth = 1280;
    f32 mHeight = 720;
    bool mWorld = false;
    f32 mPixelsPerUnit = 100;
    bool mNavigation = true;
    i32 mOrder = 0;
};

// An element's box, as flexbox lays it out among its siblings. A length is
// "auto", a number of pixels - "120" - or a share of the parent - "50%";
// empty is not set. `position` is "relative", in the flow, or "absolute":
// placed by `left`, `top`, `right` and `bottom`, which is how an element is
// pinned to a corner or stretched across. Its children go down its
// `direction` - "column" or "row" -, set out along it by `justify` - "start",
// "center", "end", "space_between", "space_around", "space_evenly" -, and
// across it by `align` - "stretch", "start", "center", "end" -, which
// `align_self` - "auto" or one of those - overrides for this one.
struct UiRect
{
    string mWidth = "auto";
    string mHeight = "auto";
    string mMinWidth;
    string mMinHeight;
    Name mPosition = Name::Declare( "relative" );
    string mLeft;
    string mTop;
    string mRight;
    string mBottom;
    f32 mMargin = 0;
    f32 mPadding = 0;
    Name mDirection = Name::Declare( "column" );
    Name mJustify = Name::Declare( "start" );
    Name mAlign = Name::Declare( "stretch" );
    Name mAlignSelf = Name::Declare( "auto" );
    f32 mGrow = 0;
    f32 mShrink = 1;
    f32 mGap = 0;
    // Hidden, it takes no room, and neither does anything under it.
    bool mVisible = true;
};

// Its box filled: with an image, or its color alone when it has none. The
// image is tinted by the color. With `slice`, its corners keep that many of
// the image's pixels whatever the box's size - 9-slice -, its edges and middle
// stretched between.
struct UiImage
{
    AssetId mImage;
    Color mColor;
    f32 mSlice = 0;
};

// Text in its box, in a font as the importer made it, `size` pixels from line
// to line - as an em -, put to the box's "start", "center" or "end", and
// wrapped at the box's width when `wrap` is on. Its box, when not given, is
// as large as the text.
struct UiText
{
    string mText;
    AssetId mFont;
    f32 mSize = 24;
    Color mColor;
    Name mAlign = Name::Declare( "start" );
    bool mWrap = true;
};

// What its entity's image is tinted by as the pointer or the focus is on it,
// as it is held, and when it is disabled. A click on it calls the entity's
// script's on_click( self ).
struct UiButton
{
    bool mDisabled = false;
    Color mHovered = Color{ 0.85f, 0.85f, 0.85f, 1.0f };
    Color mPressed = Color{ 0.65f, 0.65f, 0.65f, 1.0f };
    Color mDisabledColor = Color{ 0.5f, 0.5f, 0.5f, 0.6f };
};

// A button that holds `on`, a click turning it over; on_change( self, on ).
struct UiToggle
{
    bool mOn = false;
    bool mDisabled = false;
    // What its image is tinted by while it is on.
    Color mOnColor = Color{ 0.6f, 0.85f, 0.6f, 1.0f };
};

// A value from `min` to `max` - by `step`, when it is above 0 - set by
// dragging across its box or by the arrows with the focus on it; its box's
// image is the track, filled from the left as far as the value with `fill`.
// on_change( self, value ).
struct UiSlider
{
    f32 mValue = 0;
    f32 mMin = 0;
    f32 mMax = 1;
    f32 mStep = 0;
    bool mDisabled = false;
    Color mFill = Color{ 0.4f, 0.6f, 0.9f, 1.0f };
};

// A box laid out, in its canvas's pixels from its top left.
struct UiBox
{
    u32 mCanvas = 0;
    vec2 mMin{ 0.0f };
    vec2 mMax{ 0.0f };
};

// A canvas as the renderer takes it: how big it is in its pixels, and, in
// the world, where its pixels go - Y down, its middle at its entity.
struct UiCanvasView
{
    bool mWorld = false;
    vec2 mSize{ 0.0f };
    mat4 mToWorld{ 1.0f };
};

enum class UiQuadKind : u8
{
    // The image's texels - or white, without one - times the color.
    Image,
    // A glyph of the font's atlas, its color the quad's.
    Glyph,
};

// One rectangle to draw, in its canvas's pixels, in the order they are drawn.
struct UiQuad
{
    u32 mCanvas = 0;
    UiQuadKind mKind = UiQuadKind::Image;
    vec2 mMin{ 0.0f };
    vec2 mMax{ 0.0f };
    vec2 mUvMin{ 0.0f };
    vec2 mUvMax{ 1.0f };
    // In linear light.
    vec4 mColor{ 1.0f };
    AssetRef<TextureAsset> mImage = {};
    AssetRef<FontAsset> mFont = {};
};

// What there is of the UI to draw this frame, as the ui module lays it out
// (Phase::Ui): a world's service, the renderer's to draw - the screen's
// canvases over everything, the world's among the scene's surfaces.
struct UiScene
{
    vector<UiCanvasView> mCanvases;
    vector<UiQuad> mQuads;
};

// A world's UI as the game's loop and tools reach it: a service.
class UiWorld
{
public:
    virtual ~UiWorld() = default;

    // The screen's size in pixels, what screen canvases fill and pointers'
    // positions are in: the window's, each frame before the world's. Until
    // it is set, 1280 by 720.
    virtual void SetScreen( ivec2 size ) = 0;
    // An input event, before the game's actions see it: true when the UI
    // took it - a press on a button or on a panel, a navigation key while a
    // canvas navigates -, and the game should not. A click calls a script's
    // callback through the world's events, heard at its next turn.
    virtual bool Take( const InputEvent& event ) = 0;
    // An element's box as last laid out; nothing for one not laid out.
    virtual opt<UiBox> TryBox( Entity entity ) const = 0;
    // Where the pointer, the keyboard and the gamepad act: Entity::Null for
    // none.
    virtual Entity Hovered() const = 0;
    virtual Entity Focused() const = 0;
    // A button, toggle or slider - or Entity::Null for none.
    virtual void Focus( Entity entity ) = 0;
};

// The ui_* components, UiScene and UiWorld; images and fonts come from
// `assets`, which outlives every world.
ModuleType UiModule( AssetRegistry& assets );
}
