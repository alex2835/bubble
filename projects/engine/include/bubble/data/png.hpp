#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/math.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"

namespace bubble
{
// RGBA8 pixels, rows from the top, as a PNG: zlib's stored blocks, no
// compression - a screenshot, a test's reference picture.
string EncodePng( span<const u8> rgba, ivec2 size );
}
