#include "bubble/core/log.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/scene/component_types.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_api.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/ui/ui.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include "text_layout.hpp"
#include <yoga/Yoga.h>

namespace bubble
{
namespace
{
// A Yoga node, freed as it goes - the one place its C pointer is held.
class LayoutNode
{
public:
    explicit LayoutNode( YGConfigConstRef config ) : mNode( YGNodeNewWithConfig( config ) ) {}
    ~LayoutNode() { YGNodeFree( mNode ); }
    LayoutNode( const LayoutNode& ) = delete;
    LayoutNode& operator=( const LayoutNode& ) = delete;
    YGNodeRef Get() const { return mNode; }

private:
    YGNodeRef mNode;
};

// What a text's node is measured by: the text as it was laid out this frame.
struct TextMeasure
{
    string mText;
    f32 mSize = 0;
    bool mWrap = true;
    AssetRef<FontAsset> mFont;
};

// An element's runtime: its node, what it draws with, and where it was laid
// out. Its node and measure on the heap: Yoga holds their addresses.
struct UiElement
{
    Scope<LayoutNode> mNode;
    Scope<TextMeasure> mMeasure;
    AssetId mImageId;
    AssetRef<TextureAsset> mImage;
    AssetId mFontId;
    AssetRef<FontAsset> mFont;
    opt<UiBox> mBoxMaybe;
    string mLogged;
};

// Something the pointer and the focus act on, as last laid out.
struct Target
{
    Entity mEntity;
    UiBox mBox;
    bool mInteractive = false;
    bool mNavigable = false;
    bool mScreen = true;
};

YGSize MeasureText( YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode )
{
    const auto& measure = *static_cast<const TextMeasure*>( YGNodeGetContext( node ) );
    const auto fontMaybe = measure.mFont.TryGet();
    if ( not fontMaybe )
        return YGSize{ 0.0f, measure.mSize };
    const f32 wrapWidth =
        measure.mWrap and widthMode != YGMeasureModeUndefined ? width : numeric_limits<f32>::infinity();
    const TextLayout layout = LayOutText( measure.mText, *fontMaybe, measure.mSize, wrapWidth, 0, TextAlign::Start );
    YGSize size{ layout.mSize.x, layout.mSize.y };
    if ( widthMode == YGMeasureModeExactly )
        size.width = width;
    else if ( widthMode == YGMeasureModeAtMost )
        size.width = min( size.width, width );
    if ( heightMode == YGMeasureModeExactly )
        size.height = height;
    else if ( heightMode == YGMeasureModeAtMost )
        size.height = min( size.height, height );
    return size;
}

template <typename Enum>
struct Choice
{
    string_view mName;
    Enum mValue;
};

// A name among those a field takes: the value, or the error naming them.
template <typename Enum, size_t N>
expected<Enum, string> TryChoice( Name name, string_view field, const array<Choice<Enum>, N>& choices )
{
    const string text = name.ToString();
    string names;
    for ( const Choice<Enum>& choice : choices )
    {
        if ( choice.mName == text )
            return choice.mValue;
        names += ( names.empty() ? "" : ", " ) + string( choice.mName );
    }
    return unexpected( format( "{}: {} is not one of {}", field, text, names ) );
}

constexpr array<Choice<YGFlexDirection>, 2> cDirections{ Choice<YGFlexDirection>{ "column", YGFlexDirectionColumn },
                                                         Choice<YGFlexDirection>{ "row", YGFlexDirectionRow } };
constexpr array<Choice<YGJustify>, 6> cJustifies{ Choice<YGJustify>{ "start", YGJustifyFlexStart },
                                                  Choice<YGJustify>{ "center", YGJustifyCenter },
                                                  Choice<YGJustify>{ "end", YGJustifyFlexEnd },
                                                  Choice<YGJustify>{ "space_between", YGJustifySpaceBetween },
                                                  Choice<YGJustify>{ "space_around", YGJustifySpaceAround },
                                                  Choice<YGJustify>{ "space_evenly", YGJustifySpaceEvenly } };
constexpr array<Choice<YGAlign>, 4> cAligns{ Choice<YGAlign>{ "stretch", YGAlignStretch },
                                             Choice<YGAlign>{ "start", YGAlignFlexStart },
                                             Choice<YGAlign>{ "center", YGAlignCenter },
                                             Choice<YGAlign>{ "end", YGAlignFlexEnd } };
constexpr array<Choice<YGAlign>, 5> cSelfAligns{ Choice<YGAlign>{ "auto", YGAlignAuto },
                                                 Choice<YGAlign>{ "stretch", YGAlignStretch },
                                                 Choice<YGAlign>{ "start", YGAlignFlexStart },
                                                 Choice<YGAlign>{ "center", YGAlignCenter },
                                                 Choice<YGAlign>{ "end", YGAlignFlexEnd } };
constexpr array<Choice<YGPositionType>, 2> cPositions{ Choice<YGPositionType>{ "relative", YGPositionTypeRelative },
                                                       Choice<YGPositionType>{ "absolute", YGPositionTypeAbsolute } };
constexpr array<Choice<TextAlign>, 3> cTextAligns{ Choice<TextAlign>{ "start", TextAlign::Start },
                                                   Choice<TextAlign>{ "center", TextAlign::Center },
                                                   Choice<TextAlign>{ "end", TextAlign::End } };

// A length as a field holds it: "auto", pixels, a share, or not set.
struct Length
{
    enum class Kind : u8
    {
        Unset,
        Auto,
        Points,
        Percent,
    };
    Kind mKind = Kind::Unset;
    f32 mValue = 0;
};

expected<Length, string> TryLength( string_view text, string_view field )
{
    if ( text.empty() )
        return Length{};
    if ( text == "auto" )
        return Length{ .mKind = Length::Kind::Auto, .mValue = 0 };
    const bool percent = text.ends_with( '%' );
    const string_view number = percent ? text.substr( 0, text.size() - 1 ) : text;
    f32 value = 0;
    const auto [end, error] = from_chars( number.data(), number.data() + number.size(), value );
    if ( error != errc() or end != number.data() + number.size() or not isfinite( value ) )
        return unexpected(
            format( "ui_rect.{}: \"{}\" is not \"auto\", pixels (\"120\") or a share (\"50%\")", field, text ) );
    return Length{ .mKind = percent ? Length::Kind::Percent : Length::Kind::Points, .mValue = value };
}

f32 Snap( const UiSlider& slider, f32 value )
{
    const f32 low = min( slider.mMin, slider.mMax );
    const f32 high = max( slider.mMin, slider.mMax );
    value = clamp( value, low, high );
    if ( slider.mStep > 0 )
        value = clamp( low + round( ( value - low ) / slider.mStep ) * slider.mStep, low, high );
    return value;
}

class Ui final : public Module, public UiWorld
{
public:
    Ui( World& world, AssetRegistry& assets ) : mWorld( world ), mAssets( assets ), mConfig( YGConfigNew() )
    {
        // Layout in the canvas's own pixels, unrounded: a canvas is scaled to
        // the screen after.
        YGConfigSetPointScaleFactor( mConfig, 0.0f );
        for ( const auto drop : { &Ui::DropElement<UiRect>, &Ui::DropElement<UiImage>, &Ui::DropElement<UiText>,
                                  &Ui::DropElement<UiButton>, &Ui::DropElement<UiToggle>, &Ui::DropElement<UiSlider>,
                                  &Ui::DropElement<UiCanvas> } )
            ( this->*drop )();
        world.AddStep( Phase::Ui, [this]( f32 ) { LayOut(); } );
        world.GetServices().Add( mScene );
        world.GetServices().Add<UiWorld>( *this );
    }

    // The world lets go of its scene - the nodes - before its modules.
    ~Ui() override { YGConfigFree( mConfig ); }

    Ui( const Ui& ) = delete;
    Ui& operator=( const Ui& ) = delete;

    void Restoring( OptRef<const ModuleSnapshot> ) override
    {
        mHovered = Entity::Null;
        mPressed = Entity::Null;
        mFocused = Entity::Null;
        mTargets.clear();
    }

    void SetScreen( ivec2 size ) override { mScreen = glm::max( size, ivec2( 1 ) ); }

    opt<UiBox> TryBox( Entity entity ) const override
    {
        const auto elementMaybe = mWorld.GetScene().TryGet<UiElement>( entity );
        return elementMaybe ? elementMaybe->mBoxMaybe : nullopt;
    }

    Entity Hovered() const override { return mHovered; }
    Entity Focused() const override { return mFocused; }

    void Focus( Entity entity ) override
    {
        const Scene& scene = mWorld.GetScene();
        mFocused = entity != Entity::Null and Interactive( scene, entity ) ? entity : Entity::Null;
    }

    bool Take( const InputEvent& event ) override
    {
        if ( const auto* move = get_if<MouseMove>( &event ) )
            return PointerMoved( move->mPosition );
        if ( const auto* down = get_if<MouseDown>( &event ) )
            return down->mButton == MouseButton::Left and PointerDown( down->mPosition );
        if ( const auto* up = get_if<MouseUp>( &event ) )
            return up->mButton == MouseButton::Left and PointerUp( up->mPosition );
        // A finger is a pointer that is there only while it touches.
        const vec2 screen( mScreen );
        if ( const auto* touch = get_if<TouchDown>( &event ) )
        {
            PointerMoved( touch->mPosition * screen );
            return PointerDown( touch->mPosition * screen );
        }
        if ( const auto* touch = get_if<TouchMove>( &event ) )
            return PointerMoved( touch->mPosition * screen );
        if ( const auto* touch = get_if<TouchUp>( &event ) )
        {
            const bool taken = PointerUp( touch->mPosition * screen );
            mHovered = Entity::Null;
            return taken;
        }
        if ( const auto* key = get_if<KeyDown>( &event ) )
            return KeyPressed( key->mKey );
        if ( const auto* key = get_if<KeyUp>( &event ) )
            return mTakenKeys.erase( static_cast<u32>( key->mKey ) ) > 0;
        if ( const auto* button = get_if<GamepadButtonDown>( &event ) )
            return GamepadPressed( button->mButton );
        if ( const auto* button = get_if<GamepadButtonUp>( &event ) )
            return mTakenButtons.erase( static_cast<u32>( button->mButton ) ) > 0;
        return false;
    }

private:
    template <typename T>
    void DropElement()
    {
        mWorld.OnRemoved<T>( [this]( Entity entity ) {
            Scene& scene = mWorld.GetScene();
            if ( not IsElement( scene, entity ) and not scene.Has<UiCanvas>( entity ) )
                scene.Remove<UiElement>( entity );
        } );
    }

    static bool IsElement( const Scene& scene, Entity entity )
    {
        return scene.Has<UiRect>( entity ) or scene.Has<UiImage>( entity ) or scene.Has<UiText>( entity ) or
               scene.Has<UiButton>( entity ) or scene.Has<UiToggle>( entity ) or scene.Has<UiSlider>( entity );
    }

    static bool Interactive( const Scene& scene, Entity entity )
    {
        if ( const auto buttonMaybe = scene.TryGet<UiButton>( entity ) )
            return not buttonMaybe->mDisabled;
        if ( const auto toggleMaybe = scene.TryGet<UiToggle>( entity ) )
            return not toggleMaybe->mDisabled;
        if ( const auto sliderMaybe = scene.TryGet<UiSlider>( entity ) )
            return not sliderMaybe->mDisabled;
        return false;
    }

    UiElement& ElementOf( Entity entity )
    {
        Scene& scene = mWorld.GetScene();
        if ( const auto elementMaybe = scene.TryGet<UiElement>( entity ) )
            return *elementMaybe;
        UiElement element;
        element.mNode = CreateScope<LayoutNode>( mConfig );
        return scene.Add<UiElement>( entity, std::move( element ) );
    }

    // Says what is wrong with an element, once until it changes.
    void Report( Entity entity, UiElement& element, string message )
    {
        if ( message == element.mLogged )
            return;
        element.mLogged = message;
        if ( not message.empty() )
            LogError( "{}: {}", Describe( mWorld.GetScene(), entity ), message );
    }

    void SetLength( YGNodeRef node,
                    const Length& length,
                    void ( *points )( YGNodeRef, float ),
                    void ( *percent )( YGNodeRef, float ),
                    void ( *automatic )( YGNodeRef ) )
    {
        switch ( length.mKind )
        {
            case Length::Kind::Unset: points( node, YGUndefined ); break;
            case Length::Kind::Auto:
                if ( automatic != nullptr )
                    automatic( node );
                else
                    points( node, YGUndefined );
                break;
            case Length::Kind::Points: points( node, length.mValue ); break;
            case Length::Kind::Percent: percent( node, length.mValue ); break;
        }
    }

    // The node's style as the element's ui_rect says, or the defaults.
    void Style( Entity entity, UiElement& element )
    {
        const Scene& scene = mWorld.GetScene();
        const auto rectMaybe = scene.TryGet<UiRect>( entity );
        const UiRect rect = rectMaybe ? *rectMaybe : UiRect{};
        const YGNodeRef node = element.mNode->Get();
        string problems;
        const auto note = [&]( const string& problem ) { problems += ( problems.empty() ? "" : "; " ) + problem; };
        const auto length = [&]( const string& text, string_view field ) {
            auto lengthMaybe = TryLength( text, field );
            if ( not lengthMaybe )
            {
                note( lengthMaybe.error() );
                return Length{};
            }
            return *lengthMaybe;
        };
        SetLength( node, length( rect.mWidth, "width" ), &YGNodeStyleSetWidth, &YGNodeStyleSetWidthPercent,
                   &YGNodeStyleSetWidthAuto );
        SetLength( node, length( rect.mHeight, "height" ), &YGNodeStyleSetHeight, &YGNodeStyleSetHeightPercent,
                   &YGNodeStyleSetHeightAuto );
        SetLength( node, length( rect.mMinWidth, "min_width" ), &YGNodeStyleSetMinWidth, &YGNodeStyleSetMinWidthPercent,
                   nullptr );
        SetLength( node, length( rect.mMinHeight, "min_height" ), &YGNodeStyleSetMinHeight,
                   &YGNodeStyleSetMinHeightPercent, nullptr );
        const array<pair<const string*, YGEdge>, 4> edges{ pair<const string*, YGEdge>{ &rect.mLeft, YGEdgeLeft },
                                                           pair<const string*, YGEdge>{ &rect.mTop, YGEdgeTop },
                                                           pair<const string*, YGEdge>{ &rect.mRight, YGEdgeRight },
                                                           pair<const string*, YGEdge>{ &rect.mBottom, YGEdgeBottom } };
        for ( const auto& [text, edge] : edges )
        {
            const Length at = length( *text, edge == YGEdgeLeft    ? "left"
                                             : edge == YGEdgeTop   ? "top"
                                             : edge == YGEdgeRight ? "right"
                                                                   : "bottom" );
            if ( at.mKind == Length::Kind::Percent )
                YGNodeStyleSetPositionPercent( node, edge, at.mValue );
            else if ( at.mKind == Length::Kind::Points )
                YGNodeStyleSetPosition( node, edge, at.mValue );
            else
                YGNodeStyleSetPosition( node, edge, YGUndefined );
        }
        const auto choose = [&]( Name name, string_view field, const auto& choices, auto fallback ) {
            auto valueMaybe = TryChoice( name, format( "ui_rect.{}", field ), choices );
            if ( not valueMaybe )
            {
                note( valueMaybe.error() );
                return fallback;
            }
            return *valueMaybe;
        };
        YGNodeStyleSetPositionType( node, choose( rect.mPosition, "position", cPositions, YGPositionTypeRelative ) );
        YGNodeStyleSetFlexDirection( node, choose( rect.mDirection, "direction", cDirections, YGFlexDirectionColumn ) );
        YGNodeStyleSetJustifyContent( node, choose( rect.mJustify, "justify", cJustifies, YGJustifyFlexStart ) );
        YGNodeStyleSetAlignItems( node, choose( rect.mAlign, "align", cAligns, YGAlignStretch ) );
        YGNodeStyleSetAlignSelf( node, choose( rect.mAlignSelf, "align_self", cSelfAligns, YGAlignAuto ) );
        YGNodeStyleSetMargin( node, YGEdgeAll, rect.mMargin );
        YGNodeStyleSetPadding( node, YGEdgeAll, rect.mPadding );
        YGNodeStyleSetFlexGrow( node, rect.mGrow );
        YGNodeStyleSetFlexShrink( node, rect.mShrink );
        YGNodeStyleSetGap( node, YGGutterAll, rect.mGap );
        YGNodeStyleSetDisplay( node, rect.mVisible ? YGDisplayFlex : YGDisplayNone );

        // What it draws with, loaded as its ids change.
        const auto imageMaybe = scene.TryGet<UiImage>( entity );
        const AssetId imageId = imageMaybe ? imageMaybe->mImage : AssetId{};
        if ( imageId != element.mImageId )
        {
            element.mImageId = imageId;
            element.mImage = imageId ? mAssets.Load<TextureAsset>( imageId ) : AssetRef<TextureAsset>();
        }
        const auto textMaybe = scene.TryGet<UiText>( entity );
        const AssetId fontId = textMaybe ? textMaybe->mFont : AssetId{};
        if ( fontId != element.mFontId )
        {
            element.mFontId = fontId;
            element.mFont = fontId ? mAssets.Load<FontAsset>( fontId ) : AssetRef<FontAsset>();
        }
        for ( const AssetEntryBase* entry :
              { element.mImage ? &element.mImage.Entry() : nullptr, element.mFont ? &element.mFont.Entry() : nullptr } )
            if ( entry != nullptr and entry->State() == AssetState::Failed )
                note( entry->Error() );

        // A text is as large as it is, unless its box says otherwise. Only a
        // leaf is measured: Yoga lays out a node's children or measures it.
        const bool measured = textMaybe and ChildrenOf( scene, entity ).empty();
        if ( measured )
        {
            if ( not element.mMeasure )
            {
                element.mMeasure = CreateScope<TextMeasure>();
                YGNodeSetContext( node, element.mMeasure.get() );
                YGNodeSetMeasureFunc( node, &MeasureText );
            }
            *element.mMeasure = TextMeasure{
                .mText = textMaybe->mText, .mSize = textMaybe->mSize, .mWrap = textMaybe->mWrap, .mFont = element.mFont
            };
            YGNodeMarkDirty( node );
        }
        else if ( element.mMeasure )
        {
            YGNodeSetMeasureFunc( node, nullptr );
            YGNodeSetContext( node, nullptr );
            element.mMeasure.reset();
        }
        if ( textMaybe and not measured )
            note( "ui_text: an element with children is not measured by its text" );
        if ( textMaybe )
            if ( auto alignMaybe = TryChoice( textMaybe->mAlign, "ui_text.align", cTextAligns ); not alignMaybe )
                note( alignMaybe.error() );
        Report( entity, element, problems );
    }

    // The elements under `parent`, as nodes of its node, in the order of the
    // tree.
    void Build( Entity parent, UiElement& parentElement )
    {
        Scene& scene = mWorld.GetScene();
        const YGNodeRef node = parentElement.mNode->Get();
        YGNodeRemoveAllChildren( node );
        size_t index = 0;
        // A copy: ElementOf may add runtime components while it walks.
        const vector<Entity> children( ChildrenOf( scene, parent ).begin(), ChildrenOf( scene, parent ).end() );
        for ( const Entity child : children )
        {
            if ( not IsElement( scene, child ) or scene.Has<UiCanvas>( child ) )
                continue;
            UiElement& element = ElementOf( child );
            Style( child, element );
            Build( child, element );
            YGNodeInsertChild( node, element.mNode->Get(), index++ );
        }
    }

    vec4 TintOf( Entity entity ) const
    {
        const Scene& scene = mWorld.GetScene();
        const bool lit = entity == mHovered or entity == mFocused;
        const bool held = entity == mPressed;
        if ( const auto buttonMaybe = scene.TryGet<UiButton>( entity ) )
            return ToLinear( buttonMaybe->mDisabled ? buttonMaybe->mDisabledColor
                             : held                 ? buttonMaybe->mPressed
                             : lit                  ? buttonMaybe->mHovered
                                                    : Color{} );
        if ( const auto toggleMaybe = scene.TryGet<UiToggle>( entity ) )
        {
            const vec4 on = toggleMaybe->mOn ? ToLinear( toggleMaybe->mOnColor ) : vec4( 1.0f );
            return on * ( toggleMaybe->mDisabled ? vec4( 0.5f, 0.5f, 0.5f, 0.6f )
                          : held                 ? vec4( 0.65f, 0.65f, 0.65f, 1.0f )
                          : lit                  ? vec4( 0.85f, 0.85f, 0.85f, 1.0f )
                                                 : vec4( 1.0f ) );
        }
        if ( const auto sliderMaybe = scene.TryGet<UiSlider>( entity ) )
            return sliderMaybe->mDisabled ? vec4( 0.5f, 0.5f, 0.5f, 0.6f )
                   : lit or held          ? vec4( 0.85f, 0.85f, 0.85f, 1.0f )
                                          : vec4( 1.0f );
        return vec4( 1.0f );
    }

    void AddQuad( UiQuad quad )
    {
        if ( quad.mMax.x > quad.mMin.x and quad.mMax.y > quad.mMin.y and quad.mColor.a > 0 )
            mScene.mQuads.push_back( std::move( quad ) );
    }

    // An image across a box: whole, or in nine parts keeping its corners.
    void AddImage( u32 canvas, const UiBox& box, const UiImage& image, const UiElement& element, vec4 color )
    {
        UiQuad quad{ .mCanvas = canvas, .mKind = UiQuadKind::Image, .mColor = color, .mImage = element.mImage };
        const auto textureMaybe = element.mImage.TryGet();
        // An image still loading is not drawn: its color alone would flash.
        if ( element.mImage and not textureMaybe )
            return;
        if ( image.mSlice <= 0 or not textureMaybe )
        {
            quad.mMin = box.mMin;
            quad.mMax = box.mMax;
            AddQuad( std::move( quad ) );
            return;
        }
        const vec2 size( static_cast<f32>( textureMaybe->mMips[0].mWidth ),
                         static_cast<f32>( textureMaybe->mMips[0].mHeight ) );
        const f32 slice =
            min( { image.mSlice, ( box.mMax.x - box.mMin.x ) * 0.5f, ( box.mMax.y - box.mMin.y ) * 0.5f } );
        const array<f32, 4> xs{ box.mMin.x, box.mMin.x + slice, box.mMax.x - slice, box.mMax.x };
        const array<f32, 4> ys{ box.mMin.y, box.mMin.y + slice, box.mMax.y - slice, box.mMax.y };
        const array<f32, 4> us{ 0.0f, image.mSlice / size.x, 1.0f - image.mSlice / size.x, 1.0f };
        const array<f32, 4> vs{ 0.0f, image.mSlice / size.y, 1.0f - image.mSlice / size.y, 1.0f };
        for ( size_t row = 0; row < 3; ++row )
            for ( size_t column = 0; column < 3; ++column )
            {
                quad.mMin = vec2( xs[column], ys[row] );
                quad.mMax = vec2( xs[column + 1], ys[row + 1] );
                quad.mUvMin = vec2( us[column], vs[row] );
                quad.mUvMax = vec2( us[column + 1], vs[row + 1] );
                AddQuad( quad );
            }
    }

    // Each element's box from its node, and what it draws, in the order of
    // the tree: a parent under its children.
    void Collect( Entity parent, u32 canvas, vec2 origin, bool screen, bool navigation )
    {
        Scene& scene = mWorld.GetScene();
        for ( const Entity child : ChildrenOf( scene, parent ) )
        {
            const auto elementMaybe = scene.TryGet<UiElement>( child );
            if ( not elementMaybe or not IsElement( scene, child ) or scene.Has<UiCanvas>( child ) )
                continue;
            UiElement& element = *elementMaybe;
            const auto rectMaybe = scene.TryGet<UiRect>( child );
            if ( rectMaybe and not rectMaybe->mVisible )
            {
                Forget( child );
                continue;
            }
            const YGNodeRef node = element.mNode->Get();
            const vec2 min = origin + vec2( YGNodeLayoutGetLeft( node ), YGNodeLayoutGetTop( node ) );
            const UiBox box{ .mCanvas = canvas,
                             .mMin = min,
                             .mMax = min + vec2( YGNodeLayoutGetWidth( node ), YGNodeLayoutGetHeight( node ) ) };
            element.mBoxMaybe = box;
            const bool interactive = Interactive( scene, child );
            const vec4 tint = TintOf( child );
            if ( const auto imageMaybe = scene.TryGet<UiImage>( child ) )
                AddImage( canvas, box, *imageMaybe, element, ToLinear( imageMaybe->mColor ) * tint );
            if ( const auto sliderMaybe = scene.TryGet<UiSlider>( child ) )
            {
                const f32 span = sliderMaybe->mMax - sliderMaybe->mMin;
                const f32 share =
                    span != 0 ? clamp( ( sliderMaybe->mValue - sliderMaybe->mMin ) / span, 0.0f, 1.0f ) : 0;
                AddQuad( UiQuad{ .mCanvas = canvas,
                                 .mMin = box.mMin,
                                 .mMax = vec2( box.mMin.x + ( box.mMax.x - box.mMin.x ) * share, box.mMax.y ),
                                 .mColor = ToLinear( sliderMaybe->mFill ) * tint } );
            }
            if ( const auto textMaybe = scene.TryGet<UiText>( child ) )
                AddText( canvas, box, element, *textMaybe, scene.TryGet<UiRect>( child ) );
            if ( IsElement( scene, child ) )
                mTargets.push_back( Target{ .mEntity = child,
                                            .mBox = box,
                                            .mInteractive = interactive,
                                            .mNavigable = interactive and navigation and screen,
                                            .mScreen = screen } );
            Collect( child, canvas, min, screen, navigation );
        }
    }

    // Its text in its box, inside the padding; a wrong align - said by
    // Style - puts it to the start.
    void AddText( u32 canvas,
                  const UiBox& box,
                  const UiElement& element,
                  const UiText& text,
                  OptRef<const UiRect> rectMaybe )
    {
        const auto fontMaybe = element.mFont.TryGet();
        if ( not fontMaybe or text.mText.empty() )
            return;
        const f32 padding = rectMaybe ? rectMaybe->mPadding : 0.0f;
        const vec2 min = box.mMin + vec2( padding );
        const f32 width = max( box.mMax.x - box.mMin.x - 2.0f * padding, 0.0f );
        const auto alignMaybe = TryChoice( text.mAlign, "ui_text.align", cTextAligns );
        const TextLayout layout = LayOutText( text.mText, *fontMaybe, text.mSize,
                                              text.mWrap ? width + 0.01f : numeric_limits<f32>::infinity(), width,
                                              alignMaybe.value_or( TextAlign::Start ) );
        const vec4 color = ToLinear( text.mColor );
        for ( const PlacedGlyph& glyph : layout.mGlyphs )
            AddQuad( UiQuad{ .mCanvas = canvas,
                             .mKind = UiQuadKind::Glyph,
                             .mMin = min + glyph.mMin,
                             .mMax = min + glyph.mMax,
                             .mUvMin = glyph.mUvMin,
                             .mUvMax = glyph.mUvMax,
                             .mColor = color,
                             .mFont = element.mFont } );
    }

    // An element hidden, or no longer laid out: no box, no focus.
    void Forget( Entity entity )
    {
        Scene& scene = mWorld.GetScene();
        if ( const auto elementMaybe = scene.TryGet<UiElement>( entity ) )
            elementMaybe->mBoxMaybe.reset();
        for ( const Entity child : ChildrenOf( scene, entity ) )
            Forget( child );
    }

    vec2 CanvasSize( const UiCanvas& canvas ) const
    {
        if ( canvas.mWorld )
            return vec2( canvas.mWidth, canvas.mHeight );
        const f32 height = max( canvas.mHeight, 1.0f );
        return vec2( static_cast<f32>( mScreen.x ) * height / static_cast<f32>( mScreen.y ), height );
    }

    void LayOut()
    {
        BUBBLE_PROFILE_ZONE_NAMED( "Ui: layout" );
        Scene& scene = mWorld.GetScene();
        mScene.mCanvases.clear();
        mScene.mQuads.clear();
        mTargets.clear();
        for ( const Entity entity : scene.EntitiesWith<UiElement>() )
            scene.Get<UiElement>( entity ).mBoxMaybe.reset();
        vector<Entity> canvases = scene.EntitiesWith<UiCanvas>();
        stable_sort( canvases.begin(), canvases.end(), [&]( Entity a, Entity b ) {
            return scene.Get<UiCanvas>( a ).mOrder < scene.Get<UiCanvas>( b ).mOrder;
        } );
        for ( const Entity entity : canvases )
        {
            const UiCanvas canvas = scene.Get<UiCanvas>( entity );
            const vec2 size = CanvasSize( canvas );
            UiCanvasView view{ .mWorld = canvas.mWorld, .mSize = size, .mToWorld = mat4( 1.0f ) };
            if ( canvas.mWorld )
            {
                const auto worldMaybe = scene.TryGet<WorldTransform>( entity );
                const f32 unit = 1.0f / max( canvas.mPixelsPerUnit, 1e-4f );
                // Its pixels Y down about its middle.
                view.mToWorld = ( worldMaybe ? worldMaybe->mMatrix : mat4( 1.0f ) ) *
                                glm::scale( mat4( 1.0f ), vec3( unit, -unit, unit ) ) *
                                glm::translate( mat4( 1.0f ), vec3( -size * 0.5f, 0.0f ) );
            }
            const auto index = static_cast<u32>( mScene.mCanvases.size() );
            mScene.mCanvases.push_back( view );
            UiElement& root = ElementOf( entity );
            const YGNodeRef node = root.mNode->Get();
            YGNodeStyleSetWidth( node, size.x );
            YGNodeStyleSetHeight( node, size.y );
            Build( entity, root );
            YGNodeCalculateLayout( node, size.x, size.y, YGDirectionLTR );
            root.mBoxMaybe = UiBox{ .mCanvas = index, .mMin = vec2( 0.0f ), .mMax = size };
            Collect( entity, index, vec2( 0.0f ), not canvas.mWorld, canvas.mNavigation );
        }
        // What went away is no longer under the pointer or the focus.
        const auto present = [&]( Entity entity ) {
            return any_of( mTargets.begin(), mTargets.end(),
                           [&]( const Target& target ) { return target.mEntity == entity and target.mInteractive; } );
        };
        for ( Entity* held : { &mHovered, &mPressed, &mFocused } )
            if ( *held != Entity::Null and not present( *held ) )
                *held = Entity::Null;
    }

    // Where a point of the screen is in a screen canvas's pixels.
    vec2 InCanvas( u32 canvas, vec2 screen ) const
    {
        const vec2 size = mScene.mCanvases[canvas].mSize;
        return screen * size.y / static_cast<f32>( mScreen.y );
    }

    // The topmost element under a point of the screen: the last drawn.
    opt<Target> TryTargetAt( vec2 screen ) const
    {
        for ( size_t at = mTargets.size(); at-- > 0; )
        {
            const Target& target = mTargets[at];
            if ( not target.mScreen )
                continue;
            const vec2 point = InCanvas( target.mBox.mCanvas, screen );
            if ( point.x >= target.mBox.mMin.x and point.x < target.mBox.mMax.x and point.y >= target.mBox.mMin.y and
                 point.y < target.mBox.mMax.y )
                return target;
        }
        return nullopt;
    }

    void Call( Entity entity, Name callback, vector<ScriptArg> args = {} )
    {
        mWorld.GetEvents().Emit( ScriptCall{ entity, callback, std::move( args ) } );
    }

    void Slide( Entity entity, f32 value )
    {
        UiSlider& slider = mWorld.GetScene().Get<UiSlider>( entity );
        const f32 snapped = Snap( slider, value );
        if ( snapped == slider.mValue )
            return;
        slider.mValue = snapped;
        Call( entity, "on_change"_n, { ScriptArg( static_cast<f64>( snapped ) ) } );
    }

    void SlideTo( Entity entity, vec2 screen )
    {
        const auto targetMaybe = TryFind( entity );
        if ( not targetMaybe )
            return;
        const UiSlider& slider = mWorld.GetScene().Get<UiSlider>( entity );
        const vec2 point = InCanvas( targetMaybe->mBox.mCanvas, screen );
        const f32 width = max( targetMaybe->mBox.mMax.x - targetMaybe->mBox.mMin.x, 1e-4f );
        const f32 share = clamp( ( point.x - targetMaybe->mBox.mMin.x ) / width, 0.0f, 1.0f );
        Slide( entity, slider.mMin + share * ( slider.mMax - slider.mMin ) );
    }

    opt<Target> TryFind( Entity entity ) const
    {
        for ( const Target& target : mTargets )
            if ( target.mEntity == entity )
                return target;
        return nullopt;
    }

    // A click, a press of Enter or the gamepad's south button.
    void Activate( Entity entity )
    {
        Scene& scene = mWorld.GetScene();
        if ( not Interactive( scene, entity ) )
            return;
        if ( scene.Has<UiButton>( entity ) )
            Call( entity, "on_click"_n );
        if ( const auto toggleMaybe = scene.TryGet<UiToggle>( entity ) )
        {
            toggleMaybe->mOn = not toggleMaybe->mOn;
            Call( entity, "on_change"_n, { ScriptArg( toggleMaybe->mOn ) } );
        }
    }

    bool PointerMoved( vec2 screen )
    {
        const auto targetMaybe = TryTargetAt( screen );
        mHovered = targetMaybe and targetMaybe->mInteractive ? targetMaybe->mEntity : Entity::Null;
        if ( mPressed != Entity::Null and mWorld.GetScene().Has<UiSlider>( mPressed ) )
            SlideTo( mPressed, screen );
        return mPressed != Entity::Null;
    }

    bool PointerDown( vec2 screen )
    {
        PointerMoved( screen );
        const auto targetMaybe = TryTargetAt( screen );
        mPointerTaken = targetMaybe.has_value();
        if ( not targetMaybe )
            return false;
        if ( targetMaybe->mInteractive )
        {
            mPressed = targetMaybe->mEntity;
            mFocused = targetMaybe->mNavigable ? targetMaybe->mEntity : Entity::Null;
            if ( mWorld.GetScene().Has<UiSlider>( mPressed ) )
                SlideTo( mPressed, screen );
        }
        // A press on a panel is the UI's as well: the game does not shoot
        // through a menu.
        return true;
    }

    // The release of a press the UI took is its too.
    bool PointerUp( vec2 screen )
    {
        const bool taken = mPointerTaken;
        mPointerTaken = false;
        if ( mPressed == Entity::Null )
            return taken;
        PointerMoved( screen );
        const Entity pressed = mPressed;
        mPressed = Entity::Null;
        if ( mHovered == pressed and not mWorld.GetScene().Has<UiSlider>( pressed ) )
            Activate( pressed );
        return true;
    }

    // The navigable element nearest the focus that way: ahead of it, by
    // distance with sideways steps counting double.
    void Navigate( vec2 way )
    {
        const auto focusedMaybe = TryFind( mFocused );
        if ( not focusedMaybe or not focusedMaybe->mNavigable )
        {
            for ( const Target& target : mTargets )
                if ( target.mNavigable )
                {
                    mFocused = target.mEntity;
                    return;
                }
            return;
        }
        const vec2 from = ( focusedMaybe->mBox.mMin + focusedMaybe->mBox.mMax ) * 0.5f;
        f32 best = numeric_limits<f32>::max();
        Entity chosen = Entity::Null;
        for ( const Target& target : mTargets )
        {
            if ( not target.mNavigable or target.mEntity == mFocused or
                 target.mBox.mCanvas != focusedMaybe->mBox.mCanvas )
                continue;
            const vec2 delta = ( target.mBox.mMin + target.mBox.mMax ) * 0.5f - from;
            const f32 ahead = glm::dot( delta, way );
            if ( ahead <= 0 )
                continue;
            const f32 aside = abs( glm::dot( delta, vec2( -way.y, way.x ) ) );
            const f32 cost = ahead + 2.0f * aside;
            if ( cost < best )
            {
                best = cost;
                chosen = target.mEntity;
            }
        }
        if ( chosen != Entity::Null )
            mFocused = chosen;
    }

    bool Navigating() const
    {
        return any_of( mTargets.begin(), mTargets.end(), []( const Target& target ) { return target.mNavigable; } );
    }

    // A direction, a press, or a step of a slider, from a key or a button;
    // whether the UI took it.
    bool Navigation( vec2 way, bool press )
    {
        if ( not Navigating() )
            return false;
        Scene& scene = mWorld.GetScene();
        if ( press )
        {
            if ( mFocused == Entity::Null )
                return false;
            Activate( mFocused );
            return true;
        }
        if ( way.y == 0 and mFocused != Entity::Null and scene.Has<UiSlider>( mFocused ) )
        {
            const UiSlider& slider = scene.Get<UiSlider>( mFocused );
            const f32 step = slider.mStep > 0 ? slider.mStep : ( slider.mMax - slider.mMin ) / 20.0f;
            Slide( mFocused, slider.mValue + step * way.x );
            return true;
        }
        Navigate( way );
        return true;
    }

    bool KeyPressed( Key key )
    {
        bool taken = false;
        switch ( key )
        {
            case Key::Up: taken = Navigation( vec2( 0.0f, -1.0f ), false ); break;
            case Key::Down: taken = Navigation( vec2( 0.0f, 1.0f ), false ); break;
            case Key::Left: taken = Navigation( vec2( -1.0f, 0.0f ), false ); break;
            case Key::Right: taken = Navigation( vec2( 1.0f, 0.0f ), false ); break;
            case Key::Enter:
            case Key::KeypadEnter:
            case Key::Space: taken = Navigation( vec2( 0.0f ), true ); break;
            case Key::Tab:
                taken = Navigating();
                if ( taken )
                    FocusNext();
                break;
            default: break;
        }
        if ( taken )
            mTakenKeys.insert( static_cast<u32>( key ) );
        return taken;
    }

    bool GamepadPressed( GamepadButton button )
    {
        bool taken = false;
        switch ( button )
        {
            case GamepadButton::DpadUp: taken = Navigation( vec2( 0.0f, -1.0f ), false ); break;
            case GamepadButton::DpadDown: taken = Navigation( vec2( 0.0f, 1.0f ), false ); break;
            case GamepadButton::DpadLeft: taken = Navigation( vec2( -1.0f, 0.0f ), false ); break;
            case GamepadButton::DpadRight: taken = Navigation( vec2( 1.0f, 0.0f ), false ); break;
            case GamepadButton::South: taken = Navigation( vec2( 0.0f ), true ); break;
            default: break;
        }
        if ( taken )
            mTakenButtons.insert( static_cast<u32>( button ) );
        return taken;
    }

    // Tab: the next navigable element in the order of the tree, around.
    void FocusNext()
    {
        vector<Entity> navigable;
        for ( const Target& target : mTargets )
            if ( target.mNavigable )
                navigable.push_back( target.mEntity );
        if ( navigable.empty() )
            return;
        const auto at = find( navigable.begin(), navigable.end(), mFocused );
        mFocused = at == navigable.end() or at + 1 == navigable.end() ? navigable.front() : *( at + 1 );
    }

    World& mWorld;
    AssetRegistry& mAssets;
    YGConfigRef mConfig;
    UiScene mScene;
    ivec2 mScreen{ 1280, 720 };
    // This frame's elements in the order they are drawn.
    vector<Target> mTargets;
    Entity mHovered;
    Entity mPressed;
    Entity mFocused;
    bool mPointerTaken = false;
    // Keys and buttons whose press the UI took: their release is its too.
    hset<u32> mTakenKeys;
    hset<u32> mTakenButtons;
};

UiWorld& UiOf( World& world )
{
    return world.Get<UiWorld>();
}

void ScriptsOfUi( ScriptApi& api )
{
    api.AddGlobals(
        []( LuaState& lua, World& world ) {
            LuaTable ui = lua.NewTable();
            ui["focus"] = [&world]( opt<Entity> entityMaybe ) {
                UiOf( world ).Focus( entityMaybe.value_or( Entity::Null ) );
            };
            ui["focused"] = [&world]() -> opt<Entity> {
                const Entity focused = UiOf( world ).Focused();
                return focused != Entity::Null ? opt<Entity>( focused ) : nullopt;
            };
            lua.Globals()["ui"] = ui;
        },
        "declare ui: {\n"
        "    focus: (target: entity?) -> (),\n"
        "    focused: () -> entity?,\n"
        "}\n" );
    api.AddCallback( "on_click", "on_click( self )" );
    api.AddCallback( "on_change", "on_change( self, value: number | boolean )" );
}
}

ModuleType UiModule( AssetRegistry& assets )
{
    return ModuleType{
        .mName = "ui",
        .mDescribe =
            []( Reflection& reflection, ComponentTypes& componentTypes ) {
                const FieldInfo color{ .mFlags = FieldInfo::Color };
                reflection.Type<UiCanvas>( "ui_canvas" )
                    .Field<&UiCanvas::mWidth>( "width" )
                    .Field<&UiCanvas::mHeight>( "height" )
                    .Field<&UiCanvas::mWorld>( "world" )
                    .Field<&UiCanvas::mPixelsPerUnit>( "pixels_per_unit" )
                    .Field<&UiCanvas::mNavigation>( "navigation" )
                    .Field<&UiCanvas::mOrder>( "order" );
                reflection.Type<UiRect>( "ui_rect" )
                    .Field<&UiRect::mWidth>( "width" )
                    .Field<&UiRect::mHeight>( "height" )
                    .Field<&UiRect::mMinWidth>( "min_width" )
                    .Field<&UiRect::mMinHeight>( "min_height" )
                    .Field<&UiRect::mPosition>( "position" )
                    .Field<&UiRect::mLeft>( "left" )
                    .Field<&UiRect::mTop>( "top" )
                    .Field<&UiRect::mRight>( "right" )
                    .Field<&UiRect::mBottom>( "bottom" )
                    .Field<&UiRect::mMargin>( "margin" )
                    .Field<&UiRect::mPadding>( "padding" )
                    .Field<&UiRect::mDirection>( "direction" )
                    .Field<&UiRect::mJustify>( "justify" )
                    .Field<&UiRect::mAlign>( "align" )
                    .Field<&UiRect::mAlignSelf>( "align_self" )
                    .Field<&UiRect::mGrow>( "grow" )
                    .Field<&UiRect::mShrink>( "shrink" )
                    .Field<&UiRect::mGap>( "gap" )
                    .Field<&UiRect::mVisible>( "visible" );
                reflection.Type<UiImage>( "ui_image" )
                    .Field<&UiImage::mImage>( "image" )
                    .Field<&UiImage::mColor>( "color", color )
                    .Field<&UiImage::mSlice>( "slice" );
                reflection.Type<UiText>( "ui_text" )
                    .Field<&UiText::mText>( "text" )
                    .Field<&UiText::mFont>( "font" )
                    .Field<&UiText::mSize>( "size" )
                    .Field<&UiText::mColor>( "color", color )
                    .Field<&UiText::mAlign>( "align" )
                    .Field<&UiText::mWrap>( "wrap" );
                reflection.Type<UiButton>( "ui_button" )
                    .Field<&UiButton::mDisabled>( "disabled" )
                    .Field<&UiButton::mHovered>( "hovered", color )
                    .Field<&UiButton::mPressed>( "pressed", color )
                    .Field<&UiButton::mDisabledColor>( "disabled_color", color );
                reflection.Type<UiToggle>( "ui_toggle" )
                    .Field<&UiToggle::mOn>( "on" )
                    .Field<&UiToggle::mDisabled>( "disabled" )
                    .Field<&UiToggle::mOnColor>( "on_color", color );
                reflection.Type<UiSlider>( "ui_slider" )
                    .Field<&UiSlider::mValue>( "value" )
                    .Field<&UiSlider::mMin>( "min" )
                    .Field<&UiSlider::mMax>( "max" )
                    .Field<&UiSlider::mStep>( "step" )
                    .Field<&UiSlider::mDisabled>( "disabled" )
                    .Field<&UiSlider::mFill>( "fill", color );
                componentTypes.Add<UiCanvas>();
                componentTypes.Add<UiRect>();
                componentTypes.Add<UiImage>();
                componentTypes.Add<UiText>();
                componentTypes.Add<UiButton>();
                componentTypes.Add<UiToggle>();
                componentTypes.Add<UiSlider>();
            },
        .mCreate = [&assets]( World& world ) -> Scope<Module> { return CreateScope<Ui>( world, assets ); },
        .mScripts = &ScriptsOfUi,
        .mInEditor = true,
    };
}
}
