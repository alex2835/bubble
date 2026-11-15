#pragma once
#include "bubble/types/number.hpp"

namespace bubble
{
// Numbers that look random and come out the same on every machine for the
// same seed: what a world's logic draws on, so that a replay plays the same.
// PCG32 (O'Neill, pcg-random.org) rather than <random>, whose distributions
// differ from one standard library to another. Copying one copies where it
// is: a world's snapshot keeps it whole.
class Random
{
public:
    // A stream is another sequence for the same seed.
    explicit Random( u64 seed = 0, u64 stream = cDefaultStream );

    // All 32 bits.
    u32 Next();
    // In [0, bound), every number as likely; a bound of 0 is a logic_error.
    u32 Below( u32 bound );
    // In [0, 1), in steps of 2^-24 - every one a float exactly.
    f32 Float();
    // In [low, high) - high itself only by rounding.
    f32 Range( f32 low, f32 high );

    friend bool operator==( const Random&, const Random& ) = default;

private:
    static constexpr u64 cDefaultStream = 0xda3e39cb94b95bdbull;

    u64 mState = 0;
    u64 mIncrement = 0;
};
}
