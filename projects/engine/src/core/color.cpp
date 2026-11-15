#include "bubble/core/color.hpp"

namespace bubble
{
f32 SrgbToLinear( f32 srgb )
{
    return srgb <= 0.04045f ? srgb / 12.92f : pow( ( srgb + 0.055f ) / 1.055f, 2.4f );
}

vec4 ToLinear( const Color& color )
{
    return vec4( SrgbToLinear( color.mR ), SrgbToLinear( color.mG ), SrgbToLinear( color.mB ), color.mA );
}
}
