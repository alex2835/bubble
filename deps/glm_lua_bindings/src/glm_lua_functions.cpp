#include <sol/sol.hpp>
#include <format>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/projection.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include "glm_lua_bindings.hpp"

// Every free function lives in this one file on purpose. sol registers a global
// by name, so splitting an overload set across two files would leave the second
// registration silently replacing the first - `rotate` for vectors and `rotate`
// for matrices have to be built as one overload set to both survive.

namespace bubble
{
namespace
{
constexpr float NEARLY_EQUAL_EPSILON = 0.001f;

template <class Vec>
bool NearlyEqual( const Vec& a, const Vec& b, float epsilon )
{
    return glm::all( glm::epsilonEqual( a, b, epsilon ) );
}

} // namespace


void CreateMathFreeFunctionsBindings( sol::state& lua )
{
    // ------------------------------------------------------------- angles --
    // GLSL spelling and GLSL behaviour: these work component-wise on vectors.

    lua.set_function( "radians",
        sol::overload(
            []( float degrees ) { return glm::radians( degrees ); },
            []( const glm::vec2& v ) { return glm::radians( v ); },
            []( const glm::vec3& v ) { return glm::radians( v ); },
            []( const glm::vec4& v ) { return glm::radians( v ); }
        )
    );

    lua.set_function( "degrees",
        sol::overload(
            []( float radians ) { return glm::degrees( radians ); },
            []( const glm::vec2& v ) { return glm::degrees( v ); },
            []( const glm::vec3& v ) { return glm::degrees( v ); },
            []( const glm::vec4& v ) { return glm::degrees( v ); }
        )
    );


    // ------------------------------------------------------------- common --

    lua.set_function( "abs",
        sol::overload(
            []( float x ) { return glm::abs( x ); },
            []( const glm::vec2& v ) { return glm::abs( v ); },
            []( const glm::vec3& v ) { return glm::abs( v ); },
            []( const glm::vec4& v ) { return glm::abs( v ); }
        )
    );

    lua.set_function( "sign",
        sol::overload(
            []( float x ) { return glm::sign( x ); },
            []( const glm::vec2& v ) { return glm::sign( v ); },
            []( const glm::vec3& v ) { return glm::sign( v ); },
            []( const glm::vec4& v ) { return glm::sign( v ); }
        )
    );

    lua.set_function( "floor",
        sol::overload(
            []( float x ) { return glm::floor( x ); },
            []( const glm::vec2& v ) { return glm::floor( v ); },
            []( const glm::vec3& v ) { return glm::floor( v ); },
            []( const glm::vec4& v ) { return glm::floor( v ); }
        )
    );

    lua.set_function( "ceil",
        sol::overload(
            []( float x ) { return glm::ceil( x ); },
            []( const glm::vec2& v ) { return glm::ceil( v ); },
            []( const glm::vec3& v ) { return glm::ceil( v ); },
            []( const glm::vec4& v ) { return glm::ceil( v ); }
        )
    );

    // The fractional part, always positive: fract(-0.25) is 0.75, as in GLSL.
    lua.set_function( "fract",
        sol::overload(
            []( float x ) { return glm::fract( x ); },
            []( const glm::vec2& v ) { return glm::fract( v ); },
            []( const glm::vec3& v ) { return glm::fract( v ); },
            []( const glm::vec4& v ) { return glm::fract( v ); }
        )
    );

    // GLSL mod, not C fmod: the result takes the sign of y, so mod(-1, 8) is 7.
    // Useful for wrapping an angle or an index.
    lua.set_function( "mod",
        sol::overload(
            []( float x, float y ) { return glm::mod( x, y ); },
            []( const glm::vec2& v, float y ) { return glm::mod( v, y ); },
            []( const glm::vec3& v, float y ) { return glm::mod( v, y ); },
            []( const glm::vec4& v, float y ) { return glm::mod( v, y ); }
        )
    );

    lua.set_function( "min",
        sol::overload(
            []( float a, float b ) { return glm::min( a, b ); },
            []( const glm::vec2& a, const glm::vec2& b ) { return glm::min( a, b ); },
            []( const glm::vec3& a, const glm::vec3& b ) { return glm::min( a, b ); },
            []( const glm::vec4& a, const glm::vec4& b ) { return glm::min( a, b ); },
            []( const glm::vec2& a, float b ) { return glm::min( a, glm::vec2( b ) ); },
            []( const glm::vec3& a, float b ) { return glm::min( a, glm::vec3( b ) ); },
            []( const glm::vec4& a, float b ) { return glm::min( a, glm::vec4( b ) ); }
        )
    );

    lua.set_function( "max",
        sol::overload(
            []( float a, float b ) { return glm::max( a, b ); },
            []( const glm::vec2& a, const glm::vec2& b ) { return glm::max( a, b ); },
            []( const glm::vec3& a, const glm::vec3& b ) { return glm::max( a, b ); },
            []( const glm::vec4& a, const glm::vec4& b ) { return glm::max( a, b ); },
            []( const glm::vec2& a, float b ) { return glm::max( a, glm::vec2( b ) ); },
            []( const glm::vec3& a, float b ) { return glm::max( a, glm::vec3( b ) ); },
            []( const glm::vec4& a, float b ) { return glm::max( a, glm::vec4( b ) ); }
        )
    );

    lua.set_function( "clamp",
        sol::overload(
            []( float value, float low, float high ) { return glm::clamp( value, low, high ); },
            []( double value, double low, double high ) { return std::clamp( value, low, high ); },
            []( int value, int low, int high ) { return std::clamp( value, low, high ); },
            []( const glm::vec2& v, float low, float high ) { return glm::clamp( v, low, high ); },
            []( const glm::vec3& v, float low, float high ) { return glm::clamp( v, low, high ); },
            []( const glm::vec4& v, float low, float high ) { return glm::clamp( v, low, high ); },
            []( const glm::vec2& v, const glm::vec2& low, const glm::vec2& high ) { return glm::clamp( v, low, high ); },
            []( const glm::vec3& v, const glm::vec3& low, const glm::vec3& high ) { return glm::clamp( v, low, high ); },
            []( const glm::vec4& v, const glm::vec4& low, const glm::vec4& high ) { return glm::clamp( v, low, high ); }
        )
    );

    // Linear interpolation. `mix` is the GLSL name; `lerp` is kept as the name
    // that was already here.
    auto mixOverloads =
        sol::overload(
            []( float a, float b, float t ) { return glm::mix( a, b, t ); },
            []( const glm::vec2& a, const glm::vec2& b, float t ) { return glm::mix( a, b, t ); },
            []( const glm::vec3& a, const glm::vec3& b, float t ) { return glm::mix( a, b, t ); },
            []( const glm::vec4& a, const glm::vec4& b, float t ) { return glm::mix( a, b, t ); },
            []( const glm::vec2& a, const glm::vec2& b, const glm::vec2& t ) { return glm::mix( a, b, t ); },
            []( const glm::vec3& a, const glm::vec3& b, const glm::vec3& t ) { return glm::mix( a, b, t ); },
            []( const glm::vec4& a, const glm::vec4& b, const glm::vec4& t ) { return glm::mix( a, b, t ); }
        );
    lua.set_function( "mix", mixOverloads );
    lua.set_function( "lerp", mixOverloads );

    // 0 below the edge, 1 at or above it.
    lua.set_function( "step",
        sol::overload(
            []( float edge, float x ) { return glm::step( edge, x ); },
            []( float edge, const glm::vec2& v ) { return glm::step( glm::vec2( edge ), v ); },
            []( float edge, const glm::vec3& v ) { return glm::step( glm::vec3( edge ), v ); },
            []( float edge, const glm::vec4& v ) { return glm::step( glm::vec4( edge ), v ); }
        )
    );

    // Smooth 0..1 ramp between the two edges. The usual choice for easing a
    // value in and out without writing the curve by hand.
    lua.set_function( "smoothstep",
        sol::overload(
            []( float low, float high, float x ) { return glm::smoothstep( low, high, x ); },
            []( float low, float high, const glm::vec2& v ) { return glm::smoothstep( glm::vec2( low ), glm::vec2( high ), v ); },
            []( float low, float high, const glm::vec3& v ) { return glm::smoothstep( glm::vec3( low ), glm::vec3( high ), v ); },
            []( float low, float high, const glm::vec4& v ) { return glm::smoothstep( glm::vec4( low ), glm::vec4( high ), v ); }
        )
    );


    // ---------------------------------------------------------- geometric --

    lua.set_function( "length",
        sol::overload(
            []( const glm::vec2& v ) { return glm::length( v ); },
            []( const glm::vec3& v ) { return glm::length( v ); },
            []( const glm::vec4& v ) { return glm::length( v ); }
        )
    );

    // Squared length and distance. No square root, so this is the one to
    // compare against a squared radius in a per-frame proximity test.
    lua.set_function( "length2",
        sol::overload(
            []( const glm::vec2& v ) { return glm::length2( v ); },
            []( const glm::vec3& v ) { return glm::length2( v ); },
            []( const glm::vec4& v ) { return glm::length2( v ); }
        )
    );

    lua.set_function( "distance",
        sol::overload(
            []( const glm::vec2& a, const glm::vec2& b ) { return glm::distance( a, b ); },
            []( const glm::vec3& a, const glm::vec3& b ) { return glm::distance( a, b ); },
            []( const glm::vec4& a, const glm::vec4& b ) { return glm::distance( a, b ); }
        )
    );

    lua.set_function( "distance2",
        sol::overload(
            []( const glm::vec2& a, const glm::vec2& b ) { return glm::distance2( a, b ); },
            []( const glm::vec3& a, const glm::vec3& b ) { return glm::distance2( a, b ); },
            []( const glm::vec4& a, const glm::vec4& b ) { return glm::distance2( a, b ); }
        )
    );

    lua.set_function( "dot",
        sol::overload(
            []( const glm::vec2& a, const glm::vec2& b ) { return glm::dot( a, b ); },
            []( const glm::vec3& a, const glm::vec3& b ) { return glm::dot( a, b ); },
            []( const glm::vec4& a, const glm::vec4& b ) { return glm::dot( a, b ); }
        )
    );

    lua.set_function( "cross", []( const glm::vec3& a, const glm::vec3& b ) { return glm::cross( a, b ); } );

    // Normalising a zero vector yields NaN, in glm as in GLSL. Guard with
    // is_nearly_zero when the input can be zero.
    lua.set_function( "normalize",
        sol::overload(
            []( const glm::vec2& v ) { return glm::normalize( v ); },
            []( const glm::vec3& v ) { return glm::normalize( v ); },
            []( const glm::vec4& v ) { return glm::normalize( v ); }
        )
    );

    // The angle between two vectors, in radians. Both are normalised first.
    lua.set_function( "angle",
        sol::overload(
            []( const glm::vec2& a, const glm::vec2& b ) { return glm::angle( glm::normalize( a ), glm::normalize( b ) ); },
            []( const glm::vec3& a, const glm::vec3& b ) { return glm::angle( glm::normalize( a ), glm::normalize( b ) ); }
        )
    );

    // Bounce an incoming direction off a surface. `normal` must be normalised.
    lua.set_function( "reflect",
        sol::overload(
            []( const glm::vec2& incident, const glm::vec2& normal ) { return glm::reflect( incident, normal ); },
            []( const glm::vec3& incident, const glm::vec3& normal ) { return glm::reflect( incident, normal ); }
        )
    );

    // Bend a direction through a surface. `eta` is the ratio of refractive
    // indices; the result is zero on total internal reflection.
    lua.set_function( "refract",
        sol::overload(
            []( const glm::vec2& incident, const glm::vec2& normal, float eta ) { return glm::refract( incident, normal, eta ); },
            []( const glm::vec3& incident, const glm::vec3& normal, float eta ) { return glm::refract( incident, normal, eta ); }
        )
    );

    // The part of `v` that lies along `onto`. Subtract it to get the part that
    // does not - flattening a camera vector onto a plane, say.
    lua.set_function( "project",
        sol::overload(
            []( const glm::vec2& v, const glm::vec2& onto ) { return glm::proj( v, onto ); },
            []( const glm::vec3& v, const glm::vec3& onto ) { return glm::proj( v, onto ); }
        )
    );


    // --------------------------------------------------------- comparison --

    // Floating point equality that tolerates accumulated error. The epsilon
    // defaults to 0.001, which suits world-space positions in metres.
    lua.set_function( "is_nearly_equal",
        sol::overload(
            []( float a, float b ) { return glm::epsilonEqual( a, b, NEARLY_EQUAL_EPSILON ); },
            []( float a, float b, float epsilon ) { return glm::epsilonEqual( a, b, epsilon ); },
            []( const glm::vec2& a, const glm::vec2& b ) { return NearlyEqual( a, b, NEARLY_EQUAL_EPSILON ); },
            []( const glm::vec3& a, const glm::vec3& b ) { return NearlyEqual( a, b, NEARLY_EQUAL_EPSILON ); },
            []( const glm::vec4& a, const glm::vec4& b ) { return NearlyEqual( a, b, NEARLY_EQUAL_EPSILON ); },
            []( const glm::vec2& a, const glm::vec2& b, float epsilon ) { return NearlyEqual( a, b, epsilon ); },
            []( const glm::vec3& a, const glm::vec3& b, float epsilon ) { return NearlyEqual( a, b, epsilon ); },
            []( const glm::vec4& a, const glm::vec4& b, float epsilon ) { return NearlyEqual( a, b, epsilon ); }
        )
    );

    lua.set_function( "is_nearly_zero",
        sol::overload(
            []( float x ) { return glm::epsilonEqual( x, 0.f, NEARLY_EQUAL_EPSILON ); },
            []( const glm::vec2& v ) { return NearlyEqual( v, glm::vec2( 0 ), NEARLY_EQUAL_EPSILON ); },
            []( const glm::vec3& v ) { return NearlyEqual( v, glm::vec3( 0 ), NEARLY_EQUAL_EPSILON ); },
            []( const glm::vec4& v ) { return NearlyEqual( v, glm::vec4( 0 ), NEARLY_EQUAL_EPSILON ); }
        )
    );


    // ----------------------------------------------------------- matrices --

    lua.set_function( "identity_mat2", glm::identity<glm::mat2> );
    lua.set_function( "identity_mat3", glm::identity<glm::mat3> );
    lua.set_function( "identity_mat4", glm::identity<glm::mat4> );

    lua.set_function( "inverse",
        sol::overload(
            []( const glm::mat2& m ) { return glm::inverse( m ); },
            []( const glm::mat3& m ) { return glm::inverse( m ); },
            []( const glm::mat4& m ) { return glm::inverse( m ); }
        )
    );

    lua.set_function( "transpose",
        sol::overload(
            []( const glm::mat2& m ) { return glm::transpose( m ); },
            []( const glm::mat3& m ) { return glm::transpose( m ); },
            []( const glm::mat4& m ) { return glm::transpose( m ); }
        )
    );

    lua.set_function( "determinant",
        sol::overload(
            []( const glm::mat2& m ) { return glm::determinant( m ); },
            []( const glm::mat3& m ) { return glm::determinant( m ); },
            []( const glm::mat4& m ) { return glm::determinant( m ); }
        )
    );


    // --------------------------------------------------------- transforms --
    // These post-multiply, the way glm does: translate(m, v) is m * T(v), so
    // building a transform reads scale-last:
    //     local m = translate( identity_mat4(), position )
    //     m = rotate( m, angle, axis )
    //     m = scale( m, size )

    lua.set_function( "translate", []( const glm::mat4& m, const glm::vec3& offset )
    {
        return glm::translate( m, offset );
    } );

    lua.set_function( "scale", []( const glm::mat4& m, const glm::vec3& factor )
    {
        return glm::scale( m, factor );
    } );

    // One name, two meanings, resolved by the first argument: turn a matrix, or
    // turn a vector directly.
    lua.set_function( "rotate",
        sol::overload(
            []( const glm::mat4& m, float angle, const glm::vec3& axis ) { return glm::rotate( m, angle, axis ); },
            []( const glm::vec3& v, float angle, const glm::vec3& axis ) { return glm::rotate( v, angle, axis ); },
            []( const glm::vec2& v, float angle ) { return glm::rotate( v, angle ); }
        )
    );

    // A view matrix looking from `eye` at `center`.
    lua.set_function( "look_at", []( const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up )
    {
        return glm::lookAt( eye, center, up );
    } );

    // fovY is in radians - pass radians( 60 ) rather than 60.
    lua.set_function( "perspective", []( float fovY, float aspect, float nearPlane, float farPlane )
    {
        return glm::perspective( fovY, aspect, nearPlane, farPlane );
    } );

    lua.set_function( "ortho",
        sol::overload(
            []( float left, float right, float bottom, float top )
            {
                return glm::ortho( left, right, bottom, top );
            },
            []( float left, float right, float bottom, float top, float nearPlane, float farPlane )
            {
                return glm::ortho( left, right, bottom, top, nearPlane, farPlane );
            }
        )
    );
}

} // namespace bubble
