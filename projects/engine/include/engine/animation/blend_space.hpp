#pragma once
#include "engine/types/number.hpp"
#include "engine/types/string.hpp"
#include "engine/types/array.hpp"

namespace bubble
{
// A clip placed on a line: "walk" at 1.5, "run" at 4.
struct BlendPoint
{
    string mClip;
    f32 mValue = 0.0f;

    bool operator==( const BlendPoint& ) const = default;
};


// A one dimensional blend space: clips along a parameter, the two either
// side of the parameter's value blended by where it falls between them.
// Outside the range the nearest end plays alone.
//
// Every clip in the space runs on the same normalized phase, so a walk and a
// run blended half and half both plant the same foot at the same moment -
// which is what makes the blend read as one gait rather than two clips
// fighting. The phase advances at a rate set by the weighted duration, so a
// blend at "mostly run" cycles about as fast as the run does.
struct BlendSpace
{
    // Kept sorted by value; Add keeps it so.
    vector<BlendPoint> mPoints;

    void Add( string clip, f32 value );
    bool Empty() const { return mPoints.empty(); }

    struct Weight
    {
        u32 mPoint = 0;
        f32 mWeight = 0.0f;
    };
    // The one or two points that contribute at `value`, weights summing to
    // one. Empty for an empty space.
    vector<Weight> Weights( f32 value ) const;

    bool operator==( const BlendSpace& ) const = default;
};

}
