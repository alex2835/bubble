#include "bubble/editor/viewport.hpp"
#include "bubble/core/log.hpp"
#include "bubble/editor/assets_panel.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/panel.hpp"
#include "bubble/physics/physics.hpp"
#include "bubble/render/debug_draw.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/prefab_link.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/matrix.hpp"
#include "bubble/ui/ui.hpp"
#include "bubble/world/transform.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

// After ImGui's own header, which it takes as given.
#include <ImGuizmo.h>

namespace bubble
{
namespace
{
constexpr Color cSelected{ .mR = 1.0f, .mG = 0.6f, .mB = 0.1f, .mA = 1.0f };
constexpr Color cCollider{ .mR = 0.35f, .mG = 1.0f, .mB = 0.45f, .mA = 1.0f };
constexpr Color cLight{ .mR = 1.0f, .mG = 0.9f, .mB = 0.4f, .mA = 1.0f };
constexpr Color cCamera{ .mR = 0.85f, .mG = 0.85f, .mB = 0.85f, .mA = 1.0f };
// What marks a light or a camera that is not selected, and how large.
constexpr Color cIcon{ .mR = 0.75f, .mG = 0.7f, .mB = 0.45f, .mA = 1.0f };
constexpr f32 cIconSize = 0.2f;
// A press that came up nearer than this, in the window's units, is a click.
constexpr f32 cClickSlop = 4.0f;
constexpr int cCircleSegments = 24;

Value::Record EntitiesArgs( const Scene& scene, Entity entity )
{
    return Value::Record{ ValueField{ .mName = "entities"_n,
                                      .mValue = Value( Value::List{ Value( PathOf( scene, entity ) ) } ) } };
}

// A circle about `centre` in the plane of `u` and `v`, which are as long as
// its radius.
void Circle( DebugDraw& draw, vec3 centre, vec3 u, vec3 v, Color color )
{
    vec3 last = centre + u;
    for ( int at = 1; at <= cCircleSegments; ++at )
    {
        const f32 angle = 6.2831853f * static_cast<f32>( at ) / cCircleSegments;
        const vec3 next = centre + u * cos( angle ) + v * sin( angle );
        draw.Line( last, next, color );
        last = next;
    }
}

// Upright along `world`'s Y, `height` from end to end, its centre `lift`
// above the origin: two rings, their caps, four sides.
void Capsule( DebugDraw& draw, const mat4& world, f32 radius, f32 height, f32 lift, Color color )
{
    const vec3 x = glm::normalize( vec3( world[0] ) ) * radius;
    const vec3 z = glm::normalize( vec3( world[2] ) ) * radius;
    const f32 half = max( height * 0.5f - radius, 0.0f );
    const vec3 low( world * vec4( 0.0f, lift - half, 0.0f, 1.0f ) );
    const vec3 high( world * vec4( 0.0f, lift + half, 0.0f, 1.0f ) );
    Circle( draw, low, x, z, color );
    Circle( draw, high, x, z, color );
    for ( const vec3 side : { x, -x, z, -z } )
        draw.Line( low + side, high + side, color );
    draw.Sphere( low, radius, color );
    draw.Sphere( high, radius, color );
}

mat4 WorldOf( const Scene& scene, Entity entity )
{
    if ( const auto worldMaybe = scene.TryGet<WorldTransform>( entity ) )
        return worldMaybe->mMatrix;
    return ComputeWorldMatrix( scene, entity );
}
}

Decomposed Decompose( const mat4& matrix )
{
    const vec3 x( matrix[0] );
    const vec3 y( matrix[1] );
    const vec3 z( matrix[2] );
    Decomposed made;
    made.mPosition = vec3( matrix[3] );
    made.mScale = vec3( glm::length( x ), glm::length( y ), glm::length( z ) );
    // A mirror is a negative scale on one axis.
    if ( glm::dot( glm::cross( x, y ), z ) < 0.0f )
        made.mScale.x = -made.mScale.x;
    const auto safe = [&]( f32 length ) { return abs( length ) < 1e-6f ? ( length < 0 ? -1e-6f : 1e-6f ) : length; };
    made.mRotation = glm::normalize(
        glm::quat_cast( mat3( x / safe( made.mScale.x ), y / safe( made.mScale.y ), z / safe( made.mScale.z ) ) ) );
    return made;
}

Entity PickTarget( const Scene& scene, const Selection& selection, Entity picked )
{
    const auto linkMaybe = scene.TryGet<PrefabLink>( picked );
    if ( not linkMaybe or not scene.Alive( linkMaybe->mInstance ) )
        return picked;
    const Entity root = linkMaybe->mInstance;
    const Entity active = selection.Active();
    const auto activeLinkMaybe = scene.TryGet<PrefabLink>( active );
    const bool inside = active == root or ( activeLinkMaybe and activeLinkMaybe->mInstance == root );
    return inside ? picked : root;
}

vec3 AnglesNear( const quat& rotation, vec3 previous )
{
    constexpr f32 cTurn = glm::two_pi<f32>();
    const vec3 angles = AnglesOf( rotation );
    // The same turn by Y, X, Z the other way over.
    const vec3 other( glm::pi<f32>() - angles.x, angles.y + glm::pi<f32>(), angles.z + glm::pi<f32>() );
    const auto near = [&]( vec3 triple ) {
        for ( int axis = 0; axis < 3; ++axis )
            triple[axis] -= cTurn * glm::round( ( triple[axis] - previous[axis] ) / cTurn );
        return triple;
    };
    const vec3 first = near( angles );
    const vec3 second = near( other );
    const auto distance = [&]( vec3 triple ) {
        const vec3 apart = glm::abs( triple - previous );
        return apart.x + apart.y + apart.z;
    };
    return distance( second ) < distance( first ) ? second : first;
}

vector<pair<Entity, Decomposed>> MovedTogether( const Scene& scene,
                                                span<const Entity> selected,
                                                Entity active,
                                                const mat4& before,
                                                const mat4& after,
                                                GizmoTool tool )
{
    const mat4 delta = after * glm::inverse( before );
    const auto parentWorld = [&]( Entity entity ) {
        const Entity parent = ParentOf( scene, entity );
        return parent != Entity::Null ? ComputeWorldMatrix( scene, parent ) : mat4( 1.0f );
    };
    const Decomposed activeBefore = Decompose( glm::inverse( parentWorld( active ) ) * before );
    const Decomposed activeAfter = Decompose( glm::inverse( parentWorld( active ) ) * after );
    vector<pair<Entity, Decomposed>> moves;
    for ( const Entity entity : selected )
    {
        if ( not scene.Alive( entity ) or not scene.Has<Transform>( entity ) )
            continue;
        // Under another one selected, it goes with that one.
        if ( ranges::any_of( selected, [&]( Entity other ) {
                 return other != entity and scene.Alive( other ) and IsAncestor( scene, other, entity );
             } ) )
            continue;
        if ( entity == active )
        {
            moves.emplace_back( entity, activeAfter );
            continue;
        }
        Decomposed local =
            Decompose( glm::inverse( parentWorld( entity ) ) * delta * ComputeWorldMatrix( scene, entity ) );
        // A scale is the entity's own, along its own axes: the same share of
        // it as the active one's.
        if ( tool == GizmoTool::Scale )
        {
            const Transform& transform = scene.Get<Transform>( entity );
            local = Decomposed{ .mPosition = transform.mPosition,
                                .mRotation = quat( 1.0f, 0.0f, 0.0f, 0.0f ),
                                .mScale = transform.mScale * activeAfter.mScale / activeBefore.mScale };
        }
        moves.emplace_back( entity, local );
    }
    return moves;
}

vector<Entity> InRectangle( span<const pair<Entity, pair<vec3, vec3>>> boxes,
                            const mat4& viewProjection,
                            vec2 size,
                            vec2 from,
                            vec2 to )
{
    const vec2 low = glm::min( from, to );
    const vec2 high = glm::max( from, to );
    vector<Entity> inside;
    for ( const auto& [entity, box] : boxes )
    {
        // The box's corners on the picture; those behind the eye left out.
        vec2 seenLow( numeric_limits<f32>::max() );
        vec2 seenHigh( -numeric_limits<f32>::max() );
        bool seen = false;
        for ( u32 corner = 0; corner < 8; ++corner )
        {
            const vec3 at( corner & 1 ? box.second.x : box.first.x, corner & 2 ? box.second.y : box.first.y,
                           corner & 4 ? box.second.z : box.first.z );
            const vec4 clip = viewProjection * vec4( at, 1.0f );
            if ( clip.w <= 0.0f )
                continue;
            const vec2 ndc( clip.x / clip.w, clip.y / clip.w );
            const vec2 onPicture( ( ndc.x + 1.0f ) * 0.5f * size.x, ( 1.0f - ndc.y ) * 0.5f * size.y );
            seenLow = glm::min( seenLow, onPicture );
            seenHigh = glm::max( seenHigh, onPicture );
            seen = true;
        }
        if ( seen and seenLow.x <= high.x and seenHigh.x >= low.x and seenLow.y <= high.y and seenHigh.y >= low.y )
            inside.push_back( entity );
    }
    return inside;
}

vector<DebugLine> FloorGrid( const EditorCamera& camera )
{
    constexpr int cHalf = 50;
    constexpr int cPieces = 20;
    constexpr Color cMinor{ .mR = 0.3f, .mG = 0.3f, .mB = 0.3f, .mA = 1.0f };
    constexpr Color cMajor{ .mR = 0.45f, .mG = 0.45f, .mB = 0.45f, .mA = 1.0f };
    constexpr Color cAxisX{ .mR = 0.75f, .mG = 0.3f, .mB = 0.3f, .mA = 1.0f };
    constexpr Color cAxisZ{ .mR = 0.3f, .mG = 0.4f, .mB = 0.8f, .mA = 1.0f };
    const f32 step = pow( 10.0f, floor( log10( max( camera.mDistance, 1e-3f ) / 5.0f ) ) );
    const vec2 centre( round( camera.mFocus.x / step ) * step, round( camera.mFocus.z / step ) * step );
    const f32 reach = step * static_cast<f32>( cHalf );
    const vec2 focus( camera.mFocus.x, camera.mFocus.z );
    // A line at `at` along the other axis: the axis itself, every tenth, or
    // any other.
    const auto colorOf = [&]( f32 at, const Color& axis ) {
        if ( abs( at ) < step * 0.5f )
            return axis;
        const f32 tens = at / ( step * 10.0f );
        return abs( tens - round( tens ) ) < 1e-3f ? cMajor : cMinor;
    };
    // Each line in pieces, fainter the farther from the focus, gone at the
    // reach: the far lines would flicker as they crowd to the horizon, and
    // the grid would end in an edge.
    vector<DebugLine> lines;
    const auto add = [&]( vec3 from, vec3 to, Color color ) {
        for ( int piece = 0; piece < cPieces; ++piece )
        {
            const vec3 start = glm::mix( from, to, static_cast<f32>( piece ) / cPieces );
            const vec3 end = glm::mix( from, to, static_cast<f32>( piece + 1 ) / cPieces );
            const vec3 middle = ( start + end ) * 0.5f;
            const f32 away = glm::length( vec2( middle.x, middle.z ) - focus ) / reach;
            color.mA = 1.0f - glm::smoothstep( 0.2f, 1.0f, away );
            if ( color.mA > 0.0f )
                lines.push_back( DebugLine{ .mFrom = start, .mTo = end, .mColor = color } );
        }
    };
    for ( int line = -cHalf; line <= cHalf; ++line )
    {
        const f32 x = centre.x + static_cast<f32>( line ) * step;
        const f32 z = centre.y + static_cast<f32>( line ) * step;
        // Along Z at x, the Z axis at x = 0; along X at z, the X axis.
        add( vec3( x, 0.0f, centre.y - reach ), vec3( x, 0.0f, centre.y + reach ), colorOf( x, cAxisZ ) );
        add( vec3( centre.x - reach, 0.0f, z ), vec3( centre.x + reach, 0.0f, z ), colorOf( z, cAxisX ) );
    }
    return lines;
}

Viewport::Viewport( EditorProject& project, Gpu& gpu, Renderer& renderer )
    : mProject( project ),
      mGpu( gpu ),
      mRenderer( renderer )
{
}

void Viewport::Prepare( OptRef<Document> documentMaybe, OptRef<World> worldMaybe )
{
    // A drag of the gizmo ends when the button comes up, the panel shown or
    // not.
    if ( mGizmoHeld and not ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
    {
        mGizmoHeld = false;
        if ( documentMaybe and documentMaybe->GetHistory().InGroup() )
            documentMaybe->GetHistory().EndGroup();
    }
    if ( not documentMaybe or not worldMaybe )
        return;
    if ( const auto pickedMaybe = mView.TryPicked() )
        Picked( *documentMaybe, *pickedMaybe );
    Helpers( *documentMaybe, *worldMaybe );
}

void Viewport::Picked( Document& document, Entity entity )
{
    // The pick is of this document, whatever was made active while the GPU
    // answered.
    mProject.GetWorkspace().Activate( document );
    const Scene& scene = document.GetScene();
    const Selection& selection = document.GetSelection();
    if ( entity == Entity::Null or not scene.Alive( entity ) or not InTree( scene, entity ) )
    {
        if ( not mPickToggles )
            mProject.Run( "selection.clear" );
        return;
    }
    mProject.Run( mPickToggles ? "selection.toggle" : "selection.set",
                  EntitiesArgs( scene, PickTarget( scene, selection, entity ) ) );
}

void Viewport::Icons( const Scene& scene, DebugDraw& draw, vec3 eye )
{
    const auto axes = [&]( Entity entity ) {
        const mat4 matrix = WorldOf( scene, entity );
        return tuple<vec3, vec3, vec3, vec3>( vec3( matrix[3] ), glm::normalize( vec3( matrix[0] ) ),
                                              glm::normalize( vec3( matrix[1] ) ),
                                              glm::normalize( -vec3( matrix[2] ) ) );
    };
    // A light: a star of three crossed lines, a sun's with its arrow.
    const auto star = [&]( Entity entity, bool arrow ) {
        if ( not InTree( scene, entity ) )
            return;
        const auto [position, x, y, forward] = axes( entity );
        for ( const vec3 axis : { x, y, forward } )
            draw.Line( position - axis * cIconSize, position + axis * cIconSize, cIcon );
        if ( arrow )
            draw.Line( position, position + forward * cIconSize * 3.0f, cIcon );
    };
    for ( const Entity entity : scene.EntitiesWith<DirectionalLight>() )
        star( entity, true );
    for ( const Entity entity : scene.EntitiesWith<PointLight>() )
        star( entity, false );
    for ( const Entity entity : scene.EntitiesWith<SpotLight>() )
        star( entity, true );
    // A camera: a small pyramid looking where it looks.
    for ( const Entity entity : scene.EntitiesWith<Camera>() )
    {
        if ( not InTree( scene, entity ) )
            continue;
        const auto [position, x, y, forward] = axes( entity );
        // Looked through - the view started where it is -, it would fill it.
        if ( glm::length( position - eye ) < cIconSize * 4.0f )
            continue;
        const vec3 centre = position + forward * cIconSize * 2.0f;
        const array<vec3, 4> corners{ centre + ( -x - y ) * cIconSize, centre + ( x - y ) * cIconSize,
                                      centre + ( x + y ) * cIconSize, centre + ( -x + y ) * cIconSize };
        for ( size_t at = 0; at < 4; ++at )
        {
            draw.Line( position, corners[at], cIcon );
            draw.Line( corners[at], corners[( at + 1 ) % 4], cIcon );
        }
    }
}

void Viewport::Helpers( Document& document, World& world )
{
    const Scene& scene = document.GetScene();
    const vector<Entity> selected = document.GetSelection().All();
    DebugDraw& draw = world.Get<DebugDraw>();
    mBoxes.clear();
    mAllBoxes.clear();
    // The models of what is selected and of what is under it, as the last
    // frame drew them.
    for ( const RenderItem& item : world.Get<RenderScene>().mItems )
    {
        const auto modelMaybe = item.mModel.TryGet();
        if ( not modelMaybe or not scene.Alive( item.mEntity ) )
            continue;
        mAllBoxes.emplace_back( item.mEntity, BoundsOf( item.mWorld, modelMaybe->mMin, modelMaybe->mMax ) );
        const bool shown = ranges::any_of( selected, [&]( Entity entity ) {
            return entity == item.mEntity or IsAncestor( scene, entity, item.mEntity );
        } );
        if ( not shown )
            continue;
        draw.Box( item.mWorld, modelMaybe->mMin, modelMaybe->mMax, cSelected );
        mBoxes.emplace_back( item.mEntity, BoundsOf( item.mWorld, modelMaybe->mMin, modelMaybe->mMax ) );
    }

    // The screen UI's elements where they were laid out over the picture.
    mUiBoxes.clear();
    if ( mUi )
    {
        const UiWorld& ui = world.Get<UiWorld>();
        for ( const Entity entity : scene.EntitiesWith<UiRect>() )
        {
            if ( not InTree( scene, entity ) )
                continue;
            Entity canvas = entity;
            while ( canvas != Entity::Null and not scene.Has<UiCanvas>( canvas ) )
                canvas = ParentOf( scene, canvas );
            if ( canvas == Entity::Null or scene.Get<UiCanvas>( canvas ).mWorld )
                continue;
            // Laid out in the canvas's pixels, its height the picture's.
            const f32 toPicture = mSize.y / max( scene.Get<UiCanvas>( canvas ).mHeight, 1.0f );
            if ( const auto boxMaybe = ui.TryBox( entity ) )
                mUiBoxes.push_back( UiBoxShown{ .mEntity = entity,
                                                .mMin = boxMaybe->mMin * toPicture,
                                                .mMax = boxMaybe->mMax * toPicture,
                                                .mToCanvas = 1.0f / toPicture } );
        }
    }

    // Every light and camera, selected or not, marked where it is - they
    // have no model to be seen by -; not in a game, which shows itself.
    if ( not document.IsPlayed() )
        Icons( scene, draw, mEye );

    const f32 aspect = mPicture ? static_cast<f32>( mPicture.Size().x ) / static_cast<f32>( mPicture.Size().y ) : 1.0f;
    for ( const Entity entity : selected )
    {
        if ( not scene.Alive( entity ) )
            continue;
        const mat4 matrix = WorldOf( scene, entity );
        const vec3 position( matrix[3] );
        const vec3 scale( glm::length( vec3( matrix[0] ) ), glm::length( vec3( matrix[1] ) ),
                          glm::length( vec3( matrix[2] ) ) );
        const vec3 forward = glm::normalize( -vec3( matrix[2] ) );
        // The box in the entity's space: the matrix scales it.
        if ( const auto boxMaybe = scene.TryGet<BoxCollider>( entity ) )
            draw.Box( matrix, -boxMaybe->mSize * 0.5f, boxMaybe->mSize * 0.5f, cCollider );
        if ( const auto sphereMaybe = scene.TryGet<SphereCollider>( entity ) )
            draw.Sphere( position, sphereMaybe->mRadius * max( scale.x, max( scale.y, scale.z ) ), cCollider );
        if ( const auto capsuleMaybe = scene.TryGet<CapsuleCollider>( entity ) )
            Capsule( draw, matrix, capsuleMaybe->mRadius * max( scale.x, scale.z ), capsuleMaybe->mHeight * scale.y,
                     0.0f, cCollider );
        // A character stands on the bottom of its capsule.
        if ( const auto characterMaybe = scene.TryGet<CharacterController>( entity ) )
            Capsule( draw, matrix, characterMaybe->mRadius, characterMaybe->mHeight, characterMaybe->mHeight * 0.5f,
                     cCollider );
        if ( const auto pointMaybe = scene.TryGet<PointLight>( entity ) )
            draw.Sphere( position, pointMaybe->mRange, cLight );
        if ( const auto spotMaybe = scene.TryGet<SpotLight>( entity ) )
        {
            const vec3 centre = position + forward * spotMaybe->mRange;
            const f32 radius = spotMaybe->mRange * tan( spotMaybe->mOuterAngle );
            const vec3 u = glm::normalize( vec3( matrix[0] ) ) * radius;
            const vec3 v = glm::normalize( vec3( matrix[1] ) ) * radius;
            Circle( draw, centre, u, v, cLight );
            for ( const vec3 side : { u, -u, v, -v } )
                draw.Line( position, centre + side, cLight );
        }
        if ( scene.Has<DirectionalLight>( entity ) )
        {
            const vec3 tip = position + forward * 2.0f;
            const vec3 side = glm::normalize( vec3( matrix[0] ) ) * 0.25f;
            draw.Line( position, tip, cLight );
            draw.Line( tip, tip - forward * 0.4f + side, cLight );
            draw.Line( tip, tip - forward * 0.4f - side, cLight );
        }
        if ( const auto cameraMaybe = scene.TryGet<Camera>( entity ) )
        {
            // Its view out to ten meters, or its far plane if nearer.
            const vec3 x = glm::normalize( vec3( matrix[0] ) );
            const vec3 y = glm::normalize( vec3( matrix[1] ) );
            array<vec3, 8> corners{};
            const array<f32, 2> distances{ cameraMaybe->mNear, min( cameraMaybe->mFar, 10.0f ) };
            for ( size_t plane = 0; plane < 2; ++plane )
            {
                const f32 halfHeight = distances[plane] * tan( cameraMaybe->mFov * 0.5f );
                const f32 halfWidth = halfHeight * aspect;
                const vec3 centre = position + forward * distances[plane];
                corners[plane * 4 + 0] = centre - x * halfWidth - y * halfHeight;
                corners[plane * 4 + 1] = centre + x * halfWidth - y * halfHeight;
                corners[plane * 4 + 2] = centre + x * halfWidth + y * halfHeight;
                corners[plane * 4 + 3] = centre - x * halfWidth + y * halfHeight;
            }
            for ( size_t at = 0; at < 4; ++at )
            {
                draw.Line( corners[at], corners[( at + 1 ) % 4], cCamera );
                draw.Line( corners[4 + at], corners[4 + ( at + 1 ) % 4], cCamera );
                draw.Line( corners[at], corners[4 + at], cCamera );
            }
        }
    }
}

void Viewport::Show( OptRef<Document> documentMaybe, EditorCamera& camera )
{
    // The platform's moves since the last frame: they turn the camera, on
    // through a held cursor, where ImGui's mouse stands still.
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    const vec2 moved = mMouseMoved * vec2( scale.x, scale.y );
    mMouseMoved = vec2( 0.0f );
    const ImVec2 area = ImGui::GetContentRegionAvail();
    if ( area.x < 1 or area.y < 1 )
    {
        StopSteering();
        return;
    }
    // Shown beside a data document's window, the scene is the active
    // document again once it is turned to: a click, a drop, a key.
    if ( documentMaybe and ClickedInWindow() )
        mProject.GetWorkspace().Activate( *documentMaybe );
    mEye = camera.Position();
    const ivec2 pixels( static_cast<i32>( area.x * scale.x + 0.5f ), static_cast<i32>( area.y * scale.y + 0.5f ) );
    if ( not mPicture or mPicture.Size() != pixels )
        mPicture =
            GpuTexture( mGpu, GpuTextureOptions{ .mSize = pixels,
                                                 .mMips = 1,
                                                 .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                                 .mUsage = WGPUTextureUsage_RenderAttachment |
                                                           WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc,
                                                 .mLabel = "viewport" } );

    // The picture under a button that takes every mouse button, so a drag
    // that starts on it stays its own wherever the mouse goes - but not over
    // the gizmo, which only starts a drag where no widget is under the mouse,
    // a disabled one too: there the space is kept and nothing hovers.
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    mCorner = vec2( corner.x, corner.y );
    mSize = vec2( area.x, area.y );
    mGamePlayed = documentMaybe and documentMaybe->IsPlayed();
    // A game is seen through its camera, or looked around in with the
    // editor's - picked in, moved - as a scene is.
    mPlayed = mGamePlayed and not mFreeCamera;
    if ( mPlayed )
    {
        // The game's: a click gives the game the input; nothing here is
        // picked or moved.
        StopSteering();
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton( "picture", area,
                                ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                    ImGuiButtonFlags_MouseButtonMiddle );
        ImGui::GetWindowDrawList()->AddImage( ImGuiRenderer::TextureId( mPicture.View() ), corner,
                                              ImVec2( corner.x + area.x, corner.y + area.y ) );
        mGridShown = false;
        const char* label = "Free Camera";
        ImGui::SetCursorScreenPos( ImVec2( corner.x + area.x - ImGui::CalcTextSize( label ).x - 16, corner.y + 6 ) );
        if ( ImGui::SmallButton( label ) )
            mFreeCamera = true;
        if ( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "Look around the game with the editor's camera, pick in it, move what is picked" );
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    // The gizmo takes the left button where it is drawn; with alt, or a
    // button that steers the camera, the mouse is the camera's.
    if ( ImGuizmo::IsUsing() or
         ( ImGuizmo::IsOver() and not io.KeyAlt and not Steers() and
           not ImGui::IsMouseDown( ImGuiMouseButton_Right ) and not ImGui::IsMouseDown( ImGuiMouseButton_Middle ) ) )
        ImGui::Dummy( area );
    else
    {
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton( "picture", area,
                                ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                    ImGuiButtonFlags_MouseButtonMiddle );
    }
    ImGui::GetWindowDrawList()->AddImage( ImGuiRenderer::TextureId( mPicture.View() ), corner,
                                          ImVec2( corner.x + area.x, corner.y + area.y ) );
    if ( documentMaybe and ImGui::BeginDragDropTarget() )
    {
        if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( cAssetPayload ) )
            if ( const auto pathMaybe = AssetPath::TryFrom( string_view( static_cast<const char*>( payload->Data ),
                                                                         static_cast<size_t>( payload->DataSize ) ) ) )
            {
                const vec3 ground = GroundAt( camera, vec2( ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y ),
                                              vec2( corner.x, corner.y ), vec2( area.x, area.y ) );
                mProject.GetWorkspace().Activate( *documentMaybe );
                if ( auto placedMaybe =
                         TryPlaceAsset( mProject, *pathMaybe, EntityPath{ *Text::TryFrom( "/" ) }, ground );
                     not placedMaybe )
                    LogError( "{}", placedMaybe.error() );
            }
        ImGui::EndDragDropTarget();
    }

    Steer( documentMaybe, camera, moved );
    if ( ImGui::IsItemActive() and ImGui::IsMouseDown( ImGuiMouseButton_Middle ) and not Steers() )
        camera.Pan( vec2( io.MouseDelta.x * scale.x, io.MouseDelta.y * scale.y ), static_cast<f32>( pixels.y ) );
    if ( mFlying and io.MouseWheel != 0 )
        camera.Faster( io.MouseWheel );
    else if ( ImGui::IsItemHovered() and io.MouseWheel != 0 )
    {
        // Toward what is under the mouse.
        const vec2 share = ( vec2( io.MousePos.x, io.MousePos.y ) - mCorner ) / mSize * 2.0f - vec2( 1.0f );
        camera.Zoom( io.MouseWheel, vec2( share.x, -share.y ), mSize.x / mSize.y );
    }
    if ( mFlying )
    {
        const string speed = format( "{:.3g} m/s - the wheel sets it, Shift four times as fast", camera.mFlySpeed );
        ImGui::GetWindowDrawList()->AddText( ImVec2( corner.x + 8, corner.y + area.y - ImGui::GetFontSize() - 6 ),
                                             IM_COL32( 255, 255, 255, 220 ), speed.c_str() );
    }

    // A click picks what is drawn under it, the GPU answering a frame or so
    // later; a preview has nothing to pick. With alt it is the camera's.
    if ( documentMaybe and ImGui::IsItemActivated() and ImGui::IsMouseClicked( ImGuiMouseButton_Left ) and
         not io.KeyAlt )
    {
        // A selected element of the UI placed by left and top is dragged
        // there, one step; anything else is a click or a rectangle.
        const auto boxMaybe = mUiShown ? TryUiAt( vec2( io.MousePos.x, io.MousePos.y ) - mCorner ) : nullopt;
        const auto rectMaybe = boxMaybe ? documentMaybe->GetScene().TryGet<UiRect>( boxMaybe->mEntity ) : nullopt;
        f32 left = 0;
        f32 top = 0;
        const auto number = []( const string& text, f32& into ) {
            return not text.empty() and
                   from_chars( text.data(), text.data() + text.size(), into ).ptr == text.data() + text.size();
        };
        if ( rectMaybe and ranges::contains( documentMaybe->GetSelection().All(), boxMaybe->mEntity ) and
             rectMaybe->mPosition == "absolute"_n and number( rectMaybe->mLeft, left ) and
             number( rectMaybe->mTop, top ) )
        {
            documentMaybe->GetHistory().BeginGroup( "Move UI" );
            mUiDragMaybe = UiDragging{ .mEntity = boxMaybe->mEntity,
                                       .mFrom = vec2( io.MousePos.x, io.MousePos.y ),
                                       .mLeft = left,
                                       .mTop = top,
                                       .mToCanvas = boxMaybe->mToCanvas };
        }
        else
        {
            mPressedAt = vec2( io.MousePos.x, io.MousePos.y );
            mPickToggles = io.KeyCtrl;
        }
    }
    if ( documentMaybe )
        UiDrag( *documentMaybe );
    if ( mPressedAt and ImGui::IsMouseDown( ImGuiMouseButton_Left ) and
         glm::length( vec2( io.MousePos.x, io.MousePos.y ) - *mPressedAt ) >= cClickSlop )
    {
        // The rectangle being drawn.
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddRectFilled( ImVec2( mPressedAt->x, mPressedAt->y ), io.MousePos, IM_COL32( 255, 160, 40, 40 ) );
        draw.AddRect( ImVec2( mPressedAt->x, mPressedAt->y ), io.MousePos, IM_COL32( 255, 160, 40, 200 ) );
    }
    if ( mPressedAt and ImGui::IsMouseReleased( ImGuiMouseButton_Left ) )
    {
        const vec2 at( io.MousePos.x, io.MousePos.y );
        if ( glm::length( at - *mPressedAt ) < cClickSlop )
        {
            // The UI over the picture first; what is drawn behind it else.
            if ( const auto boxMaybe = mUiShown ? TryUiAt( at - mCorner ) : nullopt; boxMaybe and documentMaybe )
                Picked( *documentMaybe, boxMaybe->mEntity );
            else
                mView.Pick( ivec2( static_cast<i32>( ( at.x - corner.x ) * scale.x ),
                                   static_cast<i32>( ( at.y - corner.y ) * scale.y ) ) );
        }
        else if ( documentMaybe )
            Rectangle( *documentMaybe, camera, *mPressedAt - mCorner, at - mCorner );
        mPressedAt.reset();
    }

    mGridShown = mGrid and documentMaybe;
    mUiShown = mUi and documentMaybe;
    // The selected UI elements outlined where they lie.
    if ( mUiShown )
        for ( const UiBoxShown& box : mUiBoxes )
            if ( ranges::contains( documentMaybe->GetSelection().All(), box.mEntity ) )
                ImGui::GetWindowDrawList()->AddRect( ImVec2( corner.x + box.mMin.x, corner.y + box.mMin.y ),
                                                     ImVec2( corner.x + box.mMax.x, corner.y + box.mMax.y ),
                                                     IM_COL32( 255, 160, 40, 255 ), 0.0f, 0, 2.0f );
    if ( documentMaybe )
    {
        // The tools over the picture's corner, the last widgets of the panel.
        ImGui::SetCursorScreenPos( ImVec2( corner.x + 6, corner.y + 6 ) );
        Tools();
        if ( ImGui::IsWindowHovered() )
            Keys( *documentMaybe, camera );
        Gizmo( *documentMaybe, camera, vec2( corner.x, corner.y ), vec2( area.x, area.y ) );
    }
}

vec3 Viewport::GroundAt( const EditorCamera& editorCamera, vec2 mouse, vec2 corner, vec2 size )
{
    const RenderCamera camera = editorCamera.ToRenderCamera();
    const f32 spread = tan( camera.mFov * 0.5f );
    const vec2 share = ( mouse - corner ) / size * 2.0f - vec2( 1.0f );
    const vec3 along( share.x * spread * size.x / size.y, -share.y * spread, -1.0f );
    const vec3 direction = glm::normalize( mat3( camera.mWorld ) * along );
    const vec3 origin( camera.mWorld[3] );
    if ( direction.y >= -1e-4f )
        return editorCamera.mFocus;
    return origin + direction * ( -origin.y / direction.y );
}

void Viewport::Tools()
{
    const auto tool = [&]( const char* label, GizmoTool which, const char* keys ) {
        const bool on = mTool == which;
        if ( on )
            ImGui::PushStyleColor( ImGuiCol_Button, ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) );
        if ( ImGui::SmallButton( label ) )
            mTool = which;
        if ( on )
            ImGui::PopStyleColor();
        if ( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "%s", keys );
        ImGui::SameLine();
    };
    tool( "Move", GizmoTool::Move, "W" );
    tool( "Turn", GizmoTool::Turn, "E" );
    tool( "Scale", GizmoTool::Scale, "R" );
    if ( ImGui::SmallButton( mLocal ? "Local" : "World" ) )
        mLocal = not mLocal;
    if ( ImGui::IsItemHovered() )
        ImGui::SetTooltip( "X: the entity's own axes or the world's" );
    ImGui::SameLine();
    const auto toggle = [&]( const char* label, bool& on, const char* tip ) {
        const bool was = on;
        if ( was )
            ImGui::PushStyleColor( ImGuiCol_Button, ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) );
        if ( ImGui::SmallButton( label ) )
            on = not on;
        if ( was )
            ImGui::PopStyleColor();
        if ( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "%s", tip );
    };
    toggle( "Grid", mGrid, "The floor's grid" );
    ImGui::SameLine();
    toggle( "UI", mUi, "The scene's UI: its screen canvases over the picture, as the game shows them" );
    if ( mGamePlayed )
    {
        ImGui::SameLine();
        if ( ImGui::SmallButton( "Game Camera" ) )
            mFreeCamera = false;
        if ( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "Back to the game's own camera, the game's input with it" );
    }
}

void Viewport::Steer( OptRef<Document> documentMaybe, EditorCamera& camera, vec2 moved )
{
    const ImGuiIO& io = ImGui::GetIO();
    const bool active = ImGui::IsItemActive();
    // The right button flies: the mouse turns the view where the camera
    // stands, W, A, S, D move it, Q and E down and up. The first frame's move
    // came before the press.
    const bool flying = active and ImGui::IsMouseDown( ImGuiMouseButton_Right );
    if ( flying and mFlying )
        camera.Look( moved );
    mFlying = flying;
    if ( mFlying )
    {
        const auto axis = []( ImGuiKey plus, ImGuiKey minus ) {
            return ( ImGui::IsKeyDown( plus ) ? 1.0f : 0.0f ) - ( ImGui::IsKeyDown( minus ) ? 1.0f : 0.0f );
        };
        camera.Fly(
            vec3( axis( ImGuiKey_D, ImGuiKey_A ), axis( ImGuiKey_E, ImGuiKey_Q ), axis( ImGuiKey_W, ImGuiKey_S ) ),
            io.DeltaTime, io.KeyShift );
    }
    // Alt and the left button go round the selection - round the focus when
    // nothing is selected -, it staying where it is seen.
    if ( mOrbitPivotMaybe and not( active and ImGui::IsMouseDown( ImGuiMouseButton_Left ) ) )
        mOrbitPivotMaybe.reset();
    if ( mOrbitPivotMaybe )
        camera.OrbitAbout( moved, *mOrbitPivotMaybe );
    else if ( io.KeyAlt and ImGui::IsItemActivated() and ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
    {
        const auto boxMaybe = documentMaybe ? TrySelectionBox( *documentMaybe ) : nullopt;
        mOrbitPivotMaybe = boxMaybe ? ( boxMaybe->first + boxMaybe->second ) * 0.5f : camera.mFocus;
    }
}

void Viewport::Keys( Document& document, EditorCamera& camera )
{
    // Flying, the letters move the camera.
    if ( ImGui::GetIO().WantTextInput or ImGui::GetIO().KeyCtrl or Steers() )
        return;
    if ( ImGui::IsKeyPressed( ImGuiKey_W, false ) )
        mTool = GizmoTool::Move;
    if ( ImGui::IsKeyPressed( ImGuiKey_E, false ) )
        mTool = GizmoTool::Turn;
    if ( ImGui::IsKeyPressed( ImGuiKey_R, false ) )
        mTool = GizmoTool::Scale;
    if ( ImGui::IsKeyPressed( ImGuiKey_X, false ) )
        mLocal = not mLocal;
    if ( ImGui::IsKeyPressed( ImGuiKey_F, false ) )
        Frame( document, camera );
    if ( ImGui::IsKeyPressed( ImGuiKey_Delete, false ) )
    {
        mProject.GetWorkspace().Activate( document );
        if ( mProject.GetOperators().TryPoll( mProject.GetWorkspace(), "entity.destroy" ) )
            mProject.Run( "entity.destroy" );
    }
}

opt<pair<vec3, vec3>> Viewport::TrySelectionBox( const Document& document ) const
{
    const Scene& scene = document.GetScene();
    const Entity active = document.GetSelection().Active();
    if ( active == Entity::Null or not scene.Alive( active ) )
        return nullopt;
    if ( mBoxes.empty() )
    {
        const vec3 at( WorldOf( scene, active )[3] );
        return pair<vec3, vec3>( at, at );
    }
    vec3 low( numeric_limits<f32>::max() );
    vec3 high( -numeric_limits<f32>::max() );
    for ( const auto& [entity, box] : mBoxes )
    {
        low = glm::min( low, box.first );
        high = glm::max( high, box.second );
    }
    return pair<vec3, vec3>( low, high );
}

void Viewport::Frame( Document& document, EditorCamera& camera )
{
    if ( const auto boxMaybe = TrySelectionBox( document ) )
        camera.Frame( boxMaybe->first, boxMaybe->second );
}

void Viewport::Gizmo( Document& document, const EditorCamera& editorCamera, vec2 corner, vec2 size )
{
    const Scene& scene = document.GetScene();
    const Entity active = document.GetSelection().Active();
    if ( active == Entity::Null or not scene.Alive( active ) or not scene.Has<Transform>( active ) )
        return;
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect( corner.x, corner.y, size.x, size.y );
    ImGuizmo::SetOrthographic( false );
    const RenderCamera camera = editorCamera.ToRenderCamera();
    const mat4 view = glm::inverse( camera.mWorld );
    const mat4 projection = glm::perspective( camera.mFov, size.x / size.y, camera.mNear, camera.mFar );
    mat4 matrix = ComputeWorldMatrix( scene, active );
    const mat4 before = matrix;
    const ImGuizmo::OPERATION operation = mTool == GizmoTool::Move   ? ImGuizmo::TRANSLATE
                                          : mTool == GizmoTool::Turn ? ImGuizmo::ROTATE
                                                                     : ImGuizmo::SCALE;
    // A scale is along the entity's own axes whatever the switch says.
    const ImGuizmo::MODE mode = mLocal or mTool == GizmoTool::Scale ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    const vec3 snap( mTool == GizmoTool::Move ? 0.25f : mTool == GizmoTool::Turn ? 15.0f : 0.1f );
    const bool moved =
        ImGuizmo::Manipulate( glm::value_ptr( view ), glm::value_ptr( projection ), operation, mode,
                              glm::value_ptr( matrix ), nullptr, ImGui::GetIO().KeyCtrl ? &snap.x : nullptr );
    if ( ImGuizmo::IsUsing() and not mGizmoHeld )
    {
        // However many frames the drag takes, one step.
        document.GetHistory().BeginGroup( mTool == GizmoTool::Move   ? "Move"
                                          : mTool == GizmoTool::Turn ? "Turn"
                                                                     : "Scale" );
        mGizmoHeld = true;
    }
    if ( not moved )
        return;
    // Every one selected goes with the active one; the values worked out
    // before any is set, as a parent's move would change a child's world.
    const vector<pair<Entity, Decomposed>> moves =
        MovedTogether( scene, document.GetSelection().All(), active, before, matrix, mTool );
    for ( const auto& [entity, local] : moves )
    {
        const auto set = [&, target = entity]( const char* path, Value value ) {
            Value::Record args = EntitiesArgs( scene, target );
            args.push_back( ValueField{ .mName = "component"_n, .mValue = Value( "transform"_n ) } );
            args.push_back( ValueField{ .mName = "path"_n, .mValue = Value( *Text::TryFrom( path ) ) } );
            args.push_back( ValueField{ .mName = "value"_n, .mValue = std::move( value ) } );
            mProject.Run( "property.set", args );
        };
        switch ( mTool )
        {
            case GizmoTool::Move: set( "position", Value( local.mPosition ) ); break;
            case GizmoTool::Turn:
                // About the active one: the others go round it.
                if ( entity != active )
                    set( "position", Value( local.mPosition ) );
                set( "rotation", Value( AnglesNear( local.mRotation, scene.Get<Transform>( entity ).mRotation ) ) );
                break;
            case GizmoTool::Scale: set( "scale", Value( local.mScale ) ); break;
        }
    }
}

OptRef<const Viewport::UiBoxShown> Viewport::TryUiAt( vec2 at ) const
{
    // The smallest box around the point: the innermost element there.
    OptRef<const UiBoxShown> found;
    f32 area = numeric_limits<f32>::max();
    for ( const UiBoxShown& box : mUiBoxes )
    {
        if ( at.x < box.mMin.x or at.y < box.mMin.y or at.x > box.mMax.x or at.y > box.mMax.y )
            continue;
        const vec2 size = box.mMax - box.mMin;
        if ( size.x * size.y < area )
        {
            area = size.x * size.y;
            found = box;
        }
    }
    return found;
}

void Viewport::UiDrag( Document& document )
{
    const ImGuiIO& io = ImGui::GetIO();
    if ( not mUiDragMaybe )
        return;
    const Scene& scene = document.GetScene();
    if ( not ImGui::IsMouseDown( ImGuiMouseButton_Left ) or not scene.Alive( mUiDragMaybe->mEntity ) )
    {
        if ( document.GetHistory().InGroup() )
            document.GetHistory().EndGroup();
        mUiDragMaybe.reset();
        return;
    }
    // In the canvas's pixels, whole ones, as a person writes them.
    const vec2 moved = ( vec2( io.MousePos.x, io.MousePos.y ) - mUiDragMaybe->mFrom ) * mUiDragMaybe->mToCanvas;
    const auto set = [&]( const char* path, f32 value ) {
        Value::Record args = EntitiesArgs( scene, mUiDragMaybe->mEntity );
        args.push_back( ValueField{ .mName = "component"_n, .mValue = Value( "ui_rect"_n ) } );
        args.push_back( ValueField{ .mName = "path"_n, .mValue = Value( *Text::TryFrom( path ) ) } );
        args.push_back(
            ValueField{ .mName = "value"_n, .mValue = Value( *Text::TryFrom( format( "{:.0f}", value ) ) ) } );
        mProject.Run( "property.set", args );
    };
    set( "left", mUiDragMaybe->mLeft + moved.x );
    set( "top", mUiDragMaybe->mTop + moved.y );
}

void Viewport::Rectangle( Document& document, const EditorCamera& editorCamera, vec2 from, vec2 to )
{
    const Scene& scene = document.GetScene();
    const RenderCamera camera = editorCamera.ToRenderCamera();
    const mat4 viewProjection =
        glm::perspective( camera.mFov, mSize.x / max( mSize.y, 1.0f ), camera.mNear, camera.mFar ) *
        glm::inverse( camera.mWorld );
    // An instance of a prefab whole, as a click takes it; each once.
    vector<Entity> taken;
    for ( const Entity entity : InRectangle( mAllBoxes, viewProjection, mSize, from, to ) )
    {
        const auto linkMaybe = scene.TryGet<PrefabLink>( entity );
        const Entity target = linkMaybe and scene.Alive( linkMaybe->mInstance ) ? linkMaybe->mInstance : entity;
        if ( not ranges::contains( taken, target ) )
            taken.push_back( target );
    }
    if ( taken.empty() and mPickToggles )
        return;
    Value::List paths;
    for ( const Entity entity : taken )
        paths.push_back( Value( PathOf( scene, entity ) ) );
    mProject.GetWorkspace().Activate( document );
    if ( taken.empty() )
        mProject.Run( "selection.clear" );
    else
        mProject.Run( mPickToggles ? "selection.add" : "selection.set",
                      Value::Record{ ValueField{ .mName = "entities"_n, .mValue = Value( std::move( paths ) ) } } );
}

void Viewport::Draw( World& world, const EditorCamera& camera )
{
    if ( not mPicture )
        return;
    RenderScene& scene = world.Get<RenderScene>();
    if ( mPlayed )
    {
        // Through the game's first camera, as bubble_runner shows it; the
        // editor's while it has none.
        mRenderer.Draw(
            mView, scene, scene.mCameras.empty() ? camera.ToRenderCamera() : scene.mCameras.front(),
            RenderTarget{ .mView = mPicture.View(), .mSize = mPicture.Size(), .mFormat = mPicture.Format() },
            world.Get<UiScene>() );
        return;
    }
    // Among the lines the world gathered, which it gathers anew next frame.
    if ( mGridShown )
        ranges::copy( FloorGrid( camera ), back_inserter( scene.mLines ) );
    mRenderer.Draw( mView, scene, camera.ToRenderCamera(),
                    RenderTarget{ .mView = mPicture.View(), .mSize = mPicture.Size(), .mFormat = mPicture.Format() },
                    mUiShown ? OptRef<const UiScene>( world.Get<UiScene>() ) : nullopt );
}
}
