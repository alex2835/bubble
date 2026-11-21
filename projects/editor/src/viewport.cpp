#include "bubble/editor/viewport.hpp"
#include "bubble/core/log.hpp"
#include "bubble/editor/assets_panel.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/physics/physics.hpp"
#include "bubble/render/debug_draw.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/prefab_link.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/matrix.hpp"
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

void Viewport::Helpers( Document& document, World& world )
{
    const Scene& scene = document.GetScene();
    const vector<Entity> selected = document.GetSelection().All();
    DebugDraw& draw = world.Get<DebugDraw>();
    mBoxes.clear();
    // The models of what is selected and of what is under it, as the last
    // frame drew them.
    for ( const RenderItem& item : world.Get<RenderScene>().mItems )
    {
        const auto modelMaybe = item.mModel.TryGet();
        if ( not modelMaybe or not scene.Alive( item.mEntity ) )
            continue;
        const bool shown = ranges::any_of( selected, [&]( Entity entity ) {
            return entity == item.mEntity or IsAncestor( scene, entity, item.mEntity );
        } );
        if ( not shown )
            continue;
        draw.Box( item.mWorld, modelMaybe->mMin, modelMaybe->mMax, cSelected );
        vec3 low( numeric_limits<f32>::max() );
        vec3 high( -numeric_limits<f32>::max() );
        for ( int corner = 0; corner < 8; ++corner )
        {
            const vec3 local( corner & 1 ? modelMaybe->mMax.x : modelMaybe->mMin.x,
                              corner & 2 ? modelMaybe->mMax.y : modelMaybe->mMin.y,
                              corner & 4 ? modelMaybe->mMax.z : modelMaybe->mMin.z );
            const vec3 at( item.mWorld * vec4( local, 1.0f ) );
            low = glm::min( low, at );
            high = glm::max( high, at );
        }
        mBoxes.emplace_back( item.mEntity, pair<vec3, vec3>( low, high ) );
    }

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
    const ImVec2 area = ImGui::GetContentRegionAvail();
    if ( area.x < 1 or area.y < 1 )
        return;
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
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
    if ( ImGuizmo::IsUsing() or ImGuizmo::IsOver() )
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
                if ( auto placedMaybe =
                         TryPlaceAsset( mProject, *pathMaybe, EntityPath{ *Text::TryFrom( "/" ) }, ground );
                     not placedMaybe )
                    LogError( "{}", placedMaybe.error() );
            }
        ImGui::EndDragDropTarget();
    }

    const ImGuiIO& io = ImGui::GetIO();
    const vec2 delta( io.MouseDelta.x * scale.x, io.MouseDelta.y * scale.y );
    if ( ImGui::IsItemActive() and ImGui::IsMouseDown( ImGuiMouseButton_Right ) )
        camera.Orbit( delta );
    else if ( ImGui::IsItemActive() and ImGui::IsMouseDown( ImGuiMouseButton_Middle ) )
        camera.Pan( delta, static_cast<f32>( pixels.y ) );
    if ( ImGui::IsItemHovered() and io.MouseWheel != 0 )
        camera.Zoom( io.MouseWheel );

    // A click picks what is drawn under it, the GPU answering a frame or so
    // later; a preview has nothing to pick.
    if ( documentMaybe and ImGui::IsItemActivated() and ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
    {
        mPressedAt = vec2( io.MousePos.x, io.MousePos.y );
        mPickToggles = io.KeyCtrl;
    }
    if ( mPressedAt and ImGui::IsMouseReleased( ImGuiMouseButton_Left ) )
    {
        const vec2 at( io.MousePos.x, io.MousePos.y );
        if ( glm::length( at - *mPressedAt ) < cClickSlop )
            mView.Pick( ivec2( static_cast<i32>( ( at.x - corner.x ) * scale.x ),
                               static_cast<i32>( ( at.y - corner.y ) * scale.y ) ) );
        mPressedAt.reset();
    }

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
}

void Viewport::Keys( Document& document, EditorCamera& camera )
{
    if ( ImGui::GetIO().WantTextInput or ImGui::GetIO().KeyCtrl )
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
    if ( ImGui::IsKeyPressed( ImGuiKey_Delete, false ) and
         mProject.GetOperators().TryPoll( mProject.GetWorkspace(), "entity.destroy" ) )
        mProject.Run( "entity.destroy" );
}

void Viewport::Frame( Document& document, EditorCamera& camera )
{
    const Scene& scene = document.GetScene();
    const Entity active = document.GetSelection().Active();
    if ( active == Entity::Null or not scene.Alive( active ) )
        return;
    vec3 low( numeric_limits<f32>::max() );
    vec3 high( -numeric_limits<f32>::max() );
    for ( const auto& [entity, box] : mBoxes )
    {
        low = glm::min( low, box.first );
        high = glm::max( high, box.second );
    }
    if ( mBoxes.empty() )
        low = high = vec3( WorldOf( scene, active )[3] );
    const f32 radius = max( glm::length( high - low ) * 0.5f, 0.5f );
    camera.mFocus = ( low + high ) * 0.5f;
    camera.mDistance = radius / sin( camera.mFov * 0.5f ) * 1.1f;
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
    const Entity parent = ParentOf( scene, active );
    const mat4 parentWorld = parent != Entity::Null ? ComputeWorldMatrix( scene, parent ) : mat4( 1.0f );
    const Decomposed local = Decompose( glm::inverse( parentWorld ) * matrix );
    const auto set = [&]( const char* path, Value value ) {
        Value::Record args = EntitiesArgs( scene, active );
        args.push_back( ValueField{ .mName = "component"_n, .mValue = Value( "transform"_n ) } );
        args.push_back( ValueField{ .mName = "path"_n, .mValue = Value( *Text::TryFrom( path ) ) } );
        args.push_back( ValueField{ .mName = "value"_n, .mValue = std::move( value ) } );
        mProject.Run( "property.set", args );
    };
    switch ( mTool )
    {
        case GizmoTool::Move: set( "position", Value( local.mPosition ) ); break;
        case GizmoTool::Turn: set( "rotation", Value( AnglesOf( local.mRotation ) ) ); break;
        case GizmoTool::Scale: set( "scale", Value( local.mScale ) ); break;
    }
}

void Viewport::Draw( World& world, const EditorCamera& camera )
{
    if ( not mPicture )
        return;
    mRenderer.Draw( mView, world.Get<RenderScene>(), camera.ToRenderCamera(),
                    RenderTarget{ .mView = mPicture.View(), .mSize = mPicture.Size(), .mFormat = mPicture.Format() } );
}
}
