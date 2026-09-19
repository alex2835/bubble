#include "engine/pch/pch.hpp"
#include "engine/animation/blend_space.hpp"

namespace bubble
{
void BlendSpace::Add( string clip, f32 value )
{
    auto at = std::ranges::upper_bound( mPoints, value, {}, &BlendPoint::mValue );
    mPoints.insert( at, BlendPoint{ std::move( clip ), value } );
}


vector<BlendSpace::Weight> BlendSpace::Weights( f32 value ) const
{
    if ( mPoints.empty() )
        return {};
    if ( mPoints.size() == 1 or value <= mPoints.front().mValue )
        return { { 0, 1.0f } };
    if ( value >= mPoints.back().mValue )
        return { { static_cast<u32>( mPoints.size() - 1 ), 1.0f } };

    // First point at or past the value; the one before it is the other side.
    const auto upper = std::ranges::lower_bound( mPoints, value, {}, &BlendPoint::mValue );
    const u32 hi = static_cast<u32>( upper - mPoints.begin() );
    const u32 lo = hi - 1;
    const f32 span = mPoints[hi].mValue - mPoints[lo].mValue;
    // Two points at the same value: the later one wins outright.
    if ( span <= 0.0f )
        return { { hi, 1.0f } };
    const f32 t = ( value - mPoints[lo].mValue ) / span;
    if ( t <= 0.0f )
        return { { lo, 1.0f } };
    if ( t >= 1.0f )
        return { { hi, 1.0f } };
    return { { lo, 1.0f - t }, { hi, t } };
}

}
