#pragma once
#include "bubble/render/render_scene.hpp"
#include "bubble/types/math.hpp"

namespace bubble
{
// The editor's own camera in a viewport, the scene's cameras left as they
// are: it looks at a point from around it - turned about the world's up,
// tilted over it - and comes closer, backs off and slides across.
struct EditorCamera
{
    vec3 mFocus{ 0.0f };
    // About +Y, radians; 0 looks down -Z, as a camera does unturned.
    f32 mYaw = 0.6f;
    // Over the horizon, radians: below 0 looks down.
    f32 mPitch = -0.45f;
    f32 mDistance = 12.0f;
    f32 mFov = 1.0f;

    // The mouse's move across a viewport, in its pixels: turns about the
    // focus, never over the top.
    void Orbit( vec2 pixels );
    // Slides the focus with the mouse, so what is under it stays under it:
    // `height` is the viewport's, in the same pixels.
    void Pan( vec2 pixels, f32 height );
    // A wheel's notches: up comes closer, by a share of the distance.
    void Zoom( f32 notches );
    // Where a camera of the scene is - its matrix to the world - looking as
    // it looks, at a point `distance` ahead of it.
    void LookFrom( const mat4& camera, f32 distance );

    // Where the camera is, and as the renderer takes it.
    vec3 Position() const;
    RenderCamera ToRenderCamera() const;
};
}
