#include "bubble/core/random.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
Random::Random( u64 seed, u64 stream ) : mIncrement( ( stream << 1u ) | 1u )
{
    // As pcg32_srandom_r: the seed goes in between two steps, so seeds next
    // to each other start far apart.
    Next();
    mState += seed;
    Next();
}

u32 Random::Next()
{
    const u64 old = mState;
    mState = old * 6364136223846793005ull + mIncrement;
    const u32 shifted = static_cast<u32>( ( ( old >> 18u ) ^ old ) >> 27u );
    const u32 rotation = static_cast<u32>( old >> 59u );
    return ( shifted >> rotation ) | ( shifted << ( ( 0u - rotation ) & 31u ) );
}

u32 Random::Below( u32 bound )
{
    if ( bound == 0 )
        throw logic_error( "Random::Below( 0 ): there is no number below 0" );
    // The numbers under 2^32 mod bound would come up once more than the
    // rest: draw again when one does.
    const u32 threshold = ( 0u - bound ) % bound;
    for ( ;; )
    {
        const u32 drawn = Next();
        if ( drawn >= threshold )
            return drawn % bound;
    }
}

f32 Random::Float()
{
    return static_cast<f32>( Next() >> 8u ) * 0x1.0p-24f;
}

f32 Random::Range( f32 low, f32 high )
{
    return low + ( high - low ) * Float();
}
}
