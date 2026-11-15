#pragma once
#include "bubble/types/math.hpp"
#include "bubble/types/number.hpp"

namespace bubble
{
// A color as the artist picks it: red, green, blue and opacity, 0 to 1, in
// sRGB. The renderer turns it linear.
struct Color
{
    f32 mR = 1;
    f32 mG = 1;
    f32 mB = 1;
    f32 mA = 1;

    friend bool operator==( const Color&, const Color& ) = default;
};

// One sRGB channel as the artist picks it, in linear light, which the GPU
// and the lighting work in.
f32 SrgbToLinear( f32 srgb );
// The color in linear light; opacity as it is.
vec4 ToLinear( const Color& color );
}
