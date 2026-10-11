#pragma once
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
}
