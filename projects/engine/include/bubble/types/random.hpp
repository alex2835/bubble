#pragma once
#include <random>

namespace bubble
{
// The system's entropy, for what must differ every time: a new asset id.
// Play itself is seeded (bubble/core/random.hpp).
using std::random_device;
}
