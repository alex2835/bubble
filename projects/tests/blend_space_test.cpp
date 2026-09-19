#include "test.hpp"
#include "engine/animation/blend_space.hpp"

namespace
{
bool Near( f32 a, f32 b ) { return std::abs( a - b ) <= 1e-5f; }
}

TEST( BlendSpace_Weights )
{
    BlendSpace space;
    // Added out of order; kept sorted.
    space.Add( "run", 4.0f );
    space.Add( "idle", 0.0f );
    space.Add( "walk", 1.5f );
    CHECK( space.mPoints[0].mClip == "idle" );
    CHECK( space.mPoints[1].mClip == "walk" );
    CHECK( space.mPoints[2].mClip == "run" );

    // Outside the range: the nearest end, alone.
    auto w = space.Weights( -1.0f );
    CHECK( w.size() == 1 and w[0].mPoint == 0 and Near( w[0].mWeight, 1.0f ) );
    w = space.Weights( 9.0f );
    CHECK( w.size() == 1 and w[0].mPoint == 2 and Near( w[0].mWeight, 1.0f ) );

    // On a point: that point, alone.
    w = space.Weights( 1.5f );
    CHECK( w.size() == 1 and w[0].mPoint == 1 and Near( w[0].mWeight, 1.0f ) );

    // Between walk (1.5) and run (4): a quarter of the way.
    w = space.Weights( 1.5f + 2.5f * 0.25f );
    CHECK( w.size() == 2 );
    CHECK( w[0].mPoint == 1 and Near( w[0].mWeight, 0.75f ) );
    CHECK( w[1].mPoint == 2 and Near( w[1].mWeight, 0.25f ) );

    // Between idle and walk.
    w = space.Weights( 0.75f );
    CHECK( w.size() == 2 and w[0].mPoint == 0 and Near( w[0].mWeight, 0.5f ) and w[1].mPoint == 1 );

    CHECK( BlendSpace{}.Weights( 0.0f ).empty() );
    BlendSpace one;
    one.Add( "idle", 3.0f );
    w = one.Weights( 100.0f );
    CHECK( w.size() == 1 and w[0].mPoint == 0 );
}
