#pragma once
#include "bubble/core/color.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/matrix.hpp"

namespace bubble
{
// A line of the debug drawing, in the world, its color as picked.
struct DebugLine
{
    vec3 mFrom{ 0.0f };
    vec3 mTo{ 0.0f };
    Color mColor;
};

// What a world draws to show what it does - a collider, a path, a ray -
// for one frame: lines, and shapes made of them. Physics, the editor and
// scripts draw through it, at any phase; the render module takes what was
// drawn at the frame's end (Phase::Render), and it goes. A world's
// service: world.Get<DebugDraw>().
class DebugDraw
{
public:
    void Line( vec3 from, vec3 to, Color color );
    // A box along the world's axes.
    void Box( vec3 min, vec3 max, Color color );
    // A box in `world`'s space: a model's, where its entity is.
    void Box( const mat4& world, vec3 min, vec3 max, Color color );
    // Three circles, one about each axis.
    void Sphere( vec3 centre, f32 radius, Color color );
    // `world`'s X, Y and Z, `length` long: red, green, blue.
    void Axes( const mat4& world, f32 length );

    // What was drawn since the last Take; drawing starts over.
    vector<DebugLine> Take();
    size_t Count() const { return mLines.size(); }

private:
    vector<DebugLine> mLines;
};
}
