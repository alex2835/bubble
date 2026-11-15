// Transforms: an entity's place relative to its parent, and the world
// transforms the transform module makes from them each frame.
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/stream.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr f32 cQuarterTurn = 1.5707964f;

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

// Where a point lands, or which way a direction points, under a matrix.
vec3 Point( const mat4& matrix, vec3 point )
{
    return vec3( matrix * vec4( point, 1.0f ) );
}

vec3 Direction( const mat4& matrix, vec3 direction )
{
    return vec3( matrix * vec4( direction, 0.0f ) );
}

bool Near( vec3 a, vec3 b )
{
    return glm::length( a - b ) < 1e-5f;
}

struct Fixture
{
    App mApp;

    Fixture() { mApp.AddModule( TransformModule() ); }
};
}

TEST_CASE( "A rotation turns about Y, then X, then Z, and -Z is forward" )
{
    const vec3 forward( 0.0f, 0.0f, -1.0f );
    // Yawing a quarter turn to the left faces -X.
    CHECK( Near( Direction( LocalMatrix( Transform{ .mRotation = { 0.0f, cQuarterTurn, 0.0f } } ), forward ),
                 vec3( -1.0f, 0.0f, 0.0f ) ) );
    // Pitched up a quarter turn, it looks up whatever its yaw: the pitch
    // turns first, inside the yaw.
    CHECK( Near( Direction( LocalMatrix( Transform{ .mRotation = { cQuarterTurn, cQuarterTurn, 0.0f } } ), forward ),
                 vec3( 0.0f, 1.0f, 0.0f ) ) );
    // Scaled, then turned, then moved.
    const Transform placed{ .mPosition = { 1.0f, 2.0f, 3.0f },
                            .mRotation = { 0.0f, cQuarterTurn, 0.0f },
                            .mScale = { 2.0f, 2.0f, 2.0f } };
    CHECK( Near( Point( LocalMatrix( placed ), vec3( 0.0f, 0.0f, -1.0f ) ), vec3( -1.0f, 2.0f, 3.0f ) ) );
}

TEST_CASE( "A world transform is the transforms from the root down, made each frame" )
{
    Fixture f;
    Scene scene;
    const Entity house = CreateChild( scene, scene.Root(), Make( "house" ) );
    scene.Add<Transform>( house, Transform{ .mPosition = { 10.0f, 0.0f, 0.0f },
                                            .mRotation = { 0.0f, cQuarterTurn, 0.0f },
                                            .mScale = { 1.0f, 1.0f, 1.0f } } );
    // No transform of its own: where its parent is.
    const Entity room = CreateChild( scene, house, Make( "room" ) );
    const Entity lamp = CreateChild( scene, room, Make( "lamp" ) );
    scene.Add<Transform>( lamp, Transform{ .mPosition = { 0.0f, 2.0f, -1.0f } } );
    // Outside the tree: its own transform alone.
    const Entity loose = scene.Create();
    scene.Add<Transform>( loose, Transform{ .mPosition = { 0.0f, 0.0f, 5.0f } } );
    World world( f.mApp, std::move( scene ) );
    world.Frame( 1.0 / 60.0 );

    const Scene& seen = world.GetScene();
    CHECK( Near( Point( seen.Get<WorldTransform>( room ).mMatrix, vec3( 0.0f ) ), vec3( 10.0f, 0.0f, 0.0f ) ) );
    // A step forward inside the house, which faces -X.
    CHECK( Near( Point( seen.Get<WorldTransform>( lamp ).mMatrix, vec3( 0.0f ) ), vec3( 9.0f, 2.0f, 0.0f ) ) );
    CHECK( Near( Point( seen.Get<WorldTransform>( loose ).mMatrix, vec3( 0.0f ) ), vec3( 0.0f, 0.0f, 5.0f ) ) );

    // Moved, it is somewhere else from the next frame on.
    world.GetScene().Get<Transform>( house ).mPosition = vec3( 0.0f );
    world.Frame( 1.0 / 60.0 );
    CHECK( Near( Point( world.GetScene().Get<WorldTransform>( lamp ).mMatrix, vec3( 0.0f ) ),
                 vec3( -1.0f, 2.0f, 0.0f ) ) );

    // Outside the tree, it has a world transform while it has a transform.
    world.GetScene().Remove<Transform>( loose );
    world.Frame( 1.0 / 60.0 );
    CHECK_FALSE( world.GetScene().Has<WorldTransform>( loose ) );
}
