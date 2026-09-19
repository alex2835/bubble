#include "test.hpp"
#include "engine/animation/inertialization.hpp"

// The transition curve and the joint-level inertializer, both pure maths.

namespace
{
bool Near( f32 a, f32 b, f32 epsilon = 1e-4f ) { return std::abs( a - b ) <= epsilon; }
bool Near( const vec3& a, const vec3& b, f32 epsilon = 1e-4f ) { return glm::length( a - b ) <= epsilon; }
}

TEST( Inertialization_CurveEnds )
{
    using Curve = Inertializer::Curve;

    // Starts at x0 with velocity v0, ends at rest on the target at t1.
    const Curve curve = Curve::Make( 1.0f, 2.0f, 0.5f );
    CHECK( Near( curve.Evaluate( 0.0f ), 1.0f ) );
    CHECK( Near( curve.Evaluate( 0.5f ), 0.0f ) );
    CHECK( Near( curve.Evaluate( 0.6f ), 0.0f ) );
    // v0 > 0 carries it past x0 first, briefly - the deceleration needed to
    // land in 0.5s is steep.
    CHECK( curve.Evaluate( 0.01f ) > 1.0f );
    // Velocity at the end is zero: the last steps barely move.
    CHECK( std::abs( curve.Evaluate( 0.499f ) - curve.Evaluate( 0.5f ) ) < 1e-5f );

    // Already heading for the target: cut short at -5 x0 / v0 = 0.25s.
    const Curve towards = Curve::Make( 1.0f, -20.0f, 0.5f );
    CHECK( Near( towards.mDuration, 0.25f ) );
    CHECK( Near( towards.Evaluate( 0.25f ), 0.0f ) );
    // And it never goes below zero on the way.
    for ( f32 t = 0.0f; t < 0.25f; t += 0.01f )
        CHECK( towards.Evaluate( t ) >= -1e-4f );

    // Nothing to do, or no time to do it in, is a flat zero: a snap.
    CHECK( Near( Curve::Make( 0.0f, 0.0f, 0.5f ).Evaluate( 0.1f ), 0.0f ) );
    CHECK( Near( Curve::Make( 1.0f, 0.0f, 0.0f ).Evaluate( 0.0f ), 0.0f ) );
}

TEST( Inertialization_JointsEaseIn )
{
    // A joint sitting still at A, switching to a clip that holds B.
    const JointPose a{ vec3( 0, 0, 0 ), glm::angleAxis( 0.0f, vec3( 0, 1, 0 ) ), vec3( 1 ) };
    const JointPose b{ vec3( 1, 2, 3 ), glm::angleAxis( 1.0f, vec3( 0, 1, 0 ) ), vec3( 2 ) };
    const vector<JointPose> previous{ a };
    const vector<JointPose> target{ b };
    const f32 dt = 1.0f / 60.0f;

    Inertializer inertializer;
    CHECK( not inertializer.Active() );
    inertializer.Begin( previous, previous, dt, target, 0.3f );
    CHECK( inertializer.Active() );

    // The first frame shows exactly where it was.
    vector<JointPose> pose = target;
    inertializer.Apply( pose, dt );
    CHECK( Near( pose[0].mTranslation, a.mTranslation ) );
    CHECK( Near( pose[0].mScale, a.mScale ) );
    CHECK( Near( glm::angle( pose[0].mRotation * glm::inverse( a.mRotation ) ), 0.0f, 1e-3f ) );

    // Then moves monotonically towards the target, and is there at the end.
    f32 lastDistance = glm::length( a.mTranslation - b.mTranslation );
    for ( int frame = 1; frame < 20; frame++ )
    {
        pose = target;
        inertializer.Apply( pose, dt );
        const f32 distance = glm::length( pose[0].mTranslation - b.mTranslation );
        CHECK( distance <= lastDistance + 1e-4f );
        lastDistance = distance;
    }
    CHECK( not inertializer.Active() );
    pose = target;
    inertializer.Apply( pose, dt );
    CHECK( Near( pose[0].mTranslation, b.mTranslation ) );
    CHECK( Near( glm::angle( pose[0].mRotation * glm::inverse( b.mRotation ) ), 0.0f, 1e-3f ) );

    // A transition with no time is a snap.
    inertializer.Begin( previous, previous, dt, target, 0.0f );
    CHECK( not inertializer.Active() );
}

TEST( Inertialization_CarriesVelocity )
{
    // A joint moving +x at 6 units/s, switching to a clip that holds the
    // origin: the pose keeps going in +x for a moment before turning back.
    const f32 dt = 1.0f / 60.0f;
    const vector<JointPose> before{ JointPose{ vec3( 0.9f, 0, 0 ), {}, vec3( 1 ) } };
    const vector<JointPose> previous{ JointPose{ vec3( 1.0f, 0, 0 ), {}, vec3( 1 ) } };
    const vector<JointPose> target{ JointPose{} };

    Inertializer inertializer;
    inertializer.Begin( previous, before, dt, target, 0.5f );
    vector<JointPose> pose = target;
    inertializer.Apply( pose, dt );
    pose = target;
    inertializer.Apply( pose, dt );
    CHECK( pose[0].mTranslation.x > 1.0f );
}
