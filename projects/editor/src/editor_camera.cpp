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
// A notch of the wheel takes the distance to this share of it, and the speed
// flying from it.
constexpr f32 cNotch = 0.85f;
// Never nearer the focus than this: closer, a notch takes the focus ahead.
constexpr f32 cNearest = 0.3f;
// Nor less far than this a notch, however near: the wheel never sticks.
constexpr f32 cLeastStep = 0.2f;
constexpr f32 cFarthest = 100000.0f;

quat TurnOf( f32 yaw, f32 pitch )
{
    return glm::angleAxis( yaw, vec3( 0.0f, 1.0f, 0.0f ) ) * glm::angleAxis( pitch, vec3( 1.0f, 0.0f, 0.0f ) );
}
}

void EditorCamera::Orbit( vec2 pixels )
{
    OrbitAbout( pixels, mFocus );
}

void EditorCamera::OrbitAbout( vec2 pixels, vec3 pivot )
{
    const f32 yaw = mYaw - pixels.x / cPixelsARadian;
    const f32 pitch = clamp( mPitch - pixels.y / cPixelsARadian, -cPitchLimit, cPitchLimit );
    // The turn from the old view to the new, about the pivot: the focus goes
    // round it, and the camera, as far behind the focus as it was, with it.
    const quat change = TurnOf( yaw, pitch ) * glm::inverse( TurnOf( mYaw, mPitch ) );
    mFocus = pivot + change * ( mFocus - pivot );
    mYaw = yaw;
    mPitch = pitch;
}

void EditorCamera::Look( vec2 pixels )
{
    OrbitAbout( pixels, Position() );
}

void EditorCamera::Fly( vec3 keys, f32 seconds, f32 speedUp )
{
    vec3 along = Right() * keys.x + vec3( 0.0f, keys.y, 0.0f ) + Forward() * keys.z;
    const f32 length = glm::length( along );
    if ( length < 1e-6f )
        return;
    // Two keys at once go no faster than one.
    if ( length > 1.0f )
        along /= length;
    mFocus += along * ( mFlySpeed * speedUp * seconds );
}

void EditorCamera::Faster( f32 notches )
{
    mFlySpeed = clamp( mFlySpeed / pow( cNotch, notches ), 0.05f, 1000.0f );
}

void EditorCamera::Pan( vec2 pixels, f32 height )
{
    if ( height <= 0 )
        return;
    // How much of the world a pixel spans at the focus.
    const f32 perPixel = 2.0f * mDistance * tan( mFov * 0.5f ) / height;
    mFocus += ( Up() * pixels.y - Right() * pixels.x ) * perPixel;
}

void EditorCamera::Zoom( f32 notches, vec2 at, f32 aspect )
{
    if ( notches == 0 )
        return;
    // What is under the mouse as far ahead as the focus: the camera goes
    // toward it, or away, so that it stays under the mouse.
    const vec3 under = mFocus + ( Right() * ( at.x * aspect ) + Up() * at.y ) * ( tan( mFov * 0.5f ) * mDistance );
    const vec3 eye = Position();
    const vec3 forward = Forward();
    const vec3 toward = under - eye;
    const f32 reach = glm::length( toward );
    const vec3 direction = toward / reach;
    const f32 share = pow( cNotch, notches );
    const f32 step = notches > 0 ? max( reach * ( 1.0f - share ), cLeastStep * notches ) : reach * ( 1.0f - share );
    // As far ahead as the focus's plane is from where the camera comes to:
    // the distance's share, until the nearest, where the focus goes on ahead.
    mDistance = clamp( mDistance - step * glm::dot( direction, forward ), cNearest, cFarthest );
    mFocus = eye + direction * step + forward * mDistance;
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

void EditorCamera::Frame( vec3 low, vec3 high )
{
    const f32 radius = max( glm::length( high - low ) * 0.5f, 0.5f );
    mFocus = ( low + high ) * 0.5f;
    mDistance = radius / sin( mFov * 0.5f ) * 1.1f;
}

pair<vec3, vec3> BoundsOf( const mat4& world, vec3 low, vec3 high )
{
    vec3 worldLow( numeric_limits<f32>::max() );
    vec3 worldHigh( -numeric_limits<f32>::max() );
    for ( int corner = 0; corner < 8; ++corner )
    {
        const vec3 local( corner & 1 ? high.x : low.x, corner & 2 ? high.y : low.y, corner & 4 ? high.z : low.z );
        const vec3 at( world * vec4( local, 1.0f ) );
        worldLow = glm::min( worldLow, at );
        worldHigh = glm::max( worldHigh, at );
    }
    return pair<vec3, vec3>( worldLow, worldHigh );
}

vec3 EditorCamera::Position() const
{
    return mFocus - Forward() * mDistance;
}

vec3 EditorCamera::Right() const
{
    return TurnOf( mYaw, mPitch ) * vec3( 1.0f, 0.0f, 0.0f );
}

vec3 EditorCamera::Up() const
{
    return TurnOf( mYaw, mPitch ) * vec3( 0.0f, 1.0f, 0.0f );
}

vec3 EditorCamera::Forward() const
{
    return TurnOf( mYaw, mPitch ) * vec3( 0.0f, 0.0f, -1.0f );
}

RenderCamera EditorCamera::ToRenderCamera() const
{
    // The far plane well behind the focus, the near one in proportion: the
    // depth is reversed, so a wide range costs little.
    return RenderCamera{ .mWorld =
                             glm::translate( mat4( 1.0f ), Position() ) * glm::mat4_cast( TurnOf( mYaw, mPitch ) ),
                         .mFov = mFov,
                         .mNear = max( mDistance * 0.001f, 0.01f ),
                         .mFar = max( mDistance * 100.0f, 1000.0f ) };
}
}
