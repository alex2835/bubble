#include "bubble/editor/editor_camera.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/matrix.hpp"

namespace bubble
{
namespace
{
// A turn of the mouse across this many pixels is a radian.
constexpr f32 cPixelsARadian = 200.0f;
// Short of straight up and down, where yaw would mean nothing.
constexpr f32 cPitchLimit = 1.55f;

quat TurnOf( const EditorCamera& camera )
{
    return glm::angleAxis( camera.mYaw, vec3( 0.0f, 1.0f, 0.0f ) ) *
           glm::angleAxis( camera.mPitch, vec3( 1.0f, 0.0f, 0.0f ) );
}
}

void EditorCamera::Orbit( vec2 pixels )
{
    mYaw -= pixels.x / cPixelsARadian;
    mPitch = clamp( mPitch - pixels.y / cPixelsARadian, -cPitchLimit, cPitchLimit );
}

void EditorCamera::Pan( vec2 pixels, f32 height )
{
    if ( height <= 0 )
        return;
    // How much of the world a pixel spans at the focus.
    const f32 perPixel = 2.0f * mDistance * tan( mFov * 0.5f ) / height;
    const quat turn = TurnOf( *this );
    mFocus += ( turn * vec3( -pixels.x, pixels.y, 0.0f ) ) * perPixel;
}

void EditorCamera::Zoom( f32 notches )
{
    mDistance = clamp( mDistance * pow( 0.85f, notches ), 0.05f, 100000.0f );
}

void EditorCamera::LookFrom( const mat4& camera, f32 distance )
{
    const vec3 position( camera[3] );
    const vec3 forward = glm::normalize( -vec3( camera[2] ) );
    // Unturned, the camera looks down -Z; the yaw turns that about +Y, the
    // pitch lifts it.
    mYaw = atan2( -forward.x, -forward.z );
    mPitch = clamp( asin( clamp( forward.y, -1.0f, 1.0f ) ), -cPitchLimit, cPitchLimit );
    mDistance = distance;
    mFocus = position + forward * distance;
}

vec3 EditorCamera::Position() const
{
    return mFocus + TurnOf( *this ) * vec3( 0.0f, 0.0f, mDistance );
}

RenderCamera EditorCamera::ToRenderCamera() const
{
    // The far plane well behind the focus, the near one in proportion: the
    // depth is reversed, so a wide range costs little.
    return RenderCamera{ .mWorld = glm::translate( mat4( 1.0f ), Position() ) * glm::mat4_cast( TurnOf( *this ) ),
                         .mFov = mFov,
                         .mNear = max( mDistance * 0.001f, 0.01f ),
                         .mFar = max( mDistance * 100.0f, 1000.0f ) };
}
}
