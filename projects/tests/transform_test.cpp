#include "test.hpp"
#include "engine/renderer/transform.hpp"

// The rotation became a quaternion under an Euler interface. What has to hold
// is that everything written the old way still means what it meant.

namespace
{
bool Near( const mat4& a, const mat4& b, f32 epsilon = 1e-4f )
{
    for ( int c = 0; c < 4; c++ )
        for ( int r = 0; r < 4; r++ )
            if ( std::abs( a[c][r] - b[c][r] ) > epsilon )
                return false;
    return true;
}

bool Same( const quat& a, const quat& b )
{
    return std::abs( glm::dot( a, b ) ) > 1.0f - 1e-5f;
}

// How the matrix was built when the rotation was three angles.
mat4 OldMatrix( const vec3& p, const vec3& r, const vec3& s )
{
    auto m = glm::translate( mat4( 1.0f ), p );
    m = glm::rotate( m, r.z, vec3( 0, 0, 1 ) );
    m = glm::rotate( m, r.y, vec3( 0, 1, 0 ) );
    m = glm::rotate( m, r.x, vec3( 1, 0, 0 ) );
    return glm::scale( m, s );
}

const vec3 cAngles[] = { vec3( 0 ), vec3( 0.3f, 0, 0 ), vec3( 0, 2.5f, 0 ), vec3( 0, 0, -1.2f ),
                         vec3( 0.4f, -2.8f, 1.1f ), vec3( -1.5f, 1.4f, 3.0f ), vec3( 1.0f, 1.5707f, 0.2f ) };
}

TEST( Transform_EulerMeansWhatItMeant )
{
    // Old level values build the same matrix as before.
    for ( const vec3& r : cAngles )
    {
        const Transform t( vec3( 1, 2, 3 ), r, vec3( 2, 0.5f, 1 ) );
        CHECK( Near( t.TransformMat(), OldMatrix( vec3( 1, 2, 3 ), r, vec3( 2, 0.5f, 1 ) ) ) );
        // Angles read back are a spelling of the same rotation.
        CHECK( Same( Transform::FromEuler( t.Euler() ), t.mRotation ) );
    }
}

TEST( Transform_LookAngles )
{
    // A camera saved as (pitch, yaw, 0) reads back its pitch and yaw, for
    // yaws past 90 degrees too, where Euler().y folds.
    for ( f32 yaw : { 0.0f, 0.7f, 1.9f, 3.0f, -2.2f } )
        for ( f32 pitch : { 0.0f, 0.5f, -1.2f } )
        {
            const Transform t( vec3( 0 ), vec3( pitch, yaw, 0 ) );
            const vec2 look = t.LookAngles();
            CHECK( std::abs( look.x - pitch ) < 1e-4f );
            CHECK( std::abs( look.y - yaw ) < 1e-4f );
            Transform u;
            u.SetLookAngles( pitch, yaw );
            CHECK( Same( u.mRotation, t.mRotation ) );
        }
}

TEST( Transform_FromMatrix )
{
    for ( const vec3& r : cAngles )
    {
        const Transform t( vec3( -4, 5, 0.5f ), r, vec3( 3, 1, 0.25f ) );
        const Transform back = Transform::FromMatrix( t.TransformMat() );
        CHECK( glm::length( back.mPosition - t.mPosition ) < 1e-4f );
        CHECK( glm::length( back.mScale - t.mScale ) < 1e-4f );
        CHECK( Same( back.mRotation, t.mRotation ) );
    }
    // A mirror comes back as a negative scale that builds the same matrix.
    const Transform mirrored( vec3( 0 ), vec3( 0.3f, 0.2f, 0.1f ), vec3( 1, -2, 1 ) );
    CHECK( Near( Transform::FromMatrix( mirrored.TransformMat() ).TransformMat(), mirrored.TransformMat() ) );
}
