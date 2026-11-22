#pragma once
#include "bubble/render/render_scene.hpp"
#include "bubble/types/math.hpp"

namespace bubble
{
// The editor's own camera in a viewport, the scene's cameras left as they
// are: it looks at a point from around it - turned about the world's up,
// tilted over it - and comes closer, backs off, slides across, turns where
// it stands and flies.
struct EditorCamera
{
    vec3 mFocus{ 0.0f };
    // About +Y, radians; 0 looks down -Z, as a camera does unturned.
    f32 mYaw = 0.6f;
    // Over the horizon, radians: below 0 looks down.
    f32 mPitch = -0.45f;
    f32 mDistance = 12.0f;
    f32 mFov = 1.0f;
    // Meters a second, flying; the wheel changes it while it flies.
    f32 mFlySpeed = 5.0f;

    // The mouse's move across a viewport, in its pixels: turns about the
    // focus, never over the top.
    void Orbit( vec2 pixels );
    // The same turn about `pivot`: the camera goes round it as if fixed to
    // it, what it sees of it kept - the focus too.
    void OrbitAbout( vec2 pixels, vec3 pivot );
    // The same turn where the camera stands: it looks elsewhere.
    void Look( vec2 pixels );
    // Flies for `seconds` along `keys` - x to the right, y up the world's
    // up, z ahead where it looks -, `fast` four times as fast.
    void Fly( vec3 keys, f32 seconds, bool fast );
    // A wheel's notches flying: up faster, by a share of the speed.
    void Faster( f32 notches );
    // Slides the focus with the mouse, so what is under it stays under it:
    // `height` is the viewport's, in the same pixels.
    void Pan( vec2 pixels, f32 height );
    // A wheel's notches: up comes closer, by a share of the distance, toward
    // `at` - where the mouse is, in shares of the picture from its middle,
    // -1 to 1 rightwards and upwards, its width `aspect` times its height -,
    // so what is under the mouse stays under it. Near the focus it goes on
    // through it, the focus ahead of it, never stuck.
    void Zoom( f32 notches, vec2 at = vec2( 0.0f ), f32 aspect = 1.0f );
    // Where a camera of the scene is - its matrix to the world - looking as
    // it looks, at a point `distance` ahead of it.
    void LookFrom( const mat4& camera, f32 distance );
    // Turned as it is, backed off until the box from `low` to `high` fits
    // the view, its middle the focus.
    void Frame( vec3 low, vec3 high );

    // Where the camera is, and as the renderer takes it.
    vec3 Position() const;
    // Its axes in the world: to the right of the view, up it, ahead along it.
    vec3 Right() const;
    vec3 Up() const;
    vec3 Forward() const;
    RenderCamera ToRenderCamera() const;
};

// The box along the world's axes around a model's box from `low` to `high`
// as `world` places it.
pair<vec3, vec3> BoundsOf( const mat4& world, vec3 low, vec3 high );
}
