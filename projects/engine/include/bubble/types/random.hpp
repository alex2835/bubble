#pragma once
#include <random>

namespace bubble
{
// The OS's randomness, for seeds; not for the game, which has to replay.
using std::random_device;
}
