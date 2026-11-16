#include "bubble/render/debug_draw.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
namespace
{
constexpr u32 cCircleSegments = 24;
constexpr f32 cTurn = 6.2831853f;
}

void DebugDraw::Line( vec3 from, vec3 to, Color color )
{
    mLines.push_back( DebugLine{ .mFrom = from, .mTo = to, .mColor = color } );
}

void DebugDraw::Box( vec3 min, vec3 max, Color color )
{
    Box( mat4( 1.0f ), min, max, color );
}

void DebugDraw::Box( const mat4& world, vec3 min, vec3 max, Color color )
{
    const auto corner = [&]( u32 at ) {
        const vec3 local( at & 1 ? max.x : min.x, at & 2 ? max.y : min.y, at & 4 ? max.z : min.z );
        return vec3( world * vec4( local, 1.0f ) );
    };
    // Each edge joins two corners one bit apart.
    for ( u32 at = 0; at < 8; ++at )
        for ( const u32 bit : { 1u, 2u, 4u } )
            if ( ( at & bit ) == 0 )
                Line( corner( at ), corner( at | bit ), color );
}

void DebugDraw::Sphere( vec3 centre, f32 radius, Color color )
{
    const auto point = [&]( u32 axis, u32 step ) {
        const f32 angle = cTurn * static_cast<f32>( step ) / static_cast<f32>( cCircleSegments );
        const f32 a = cos( angle ) * radius;
        const f32 b = sin( angle ) * radius;
        switch ( axis )
        {
            case 0: return centre + vec3( 0.0f, a, b );
            case 1: return centre + vec3( a, 0.0f, b );
            default: return centre + vec3( a, b, 0.0f );
        }
    };
    for ( u32 axis = 0; axis < 3; ++axis )
        for ( u32 step = 0; step < cCircleSegments; ++step )
            Line( point( axis, step ), point( axis, step + 1 ), color );
}

void DebugDraw::Axes( const mat4& world, f32 length )
{
    const vec3 origin( world[3] );
    Line( origin, vec3( world * vec4( length, 0.0f, 0.0f, 1.0f ) ), Color{ .mR = 1, .mG = 0, .mB = 0, .mA = 1 } );
    Line( origin, vec3( world * vec4( 0.0f, length, 0.0f, 1.0f ) ), Color{ .mR = 0, .mG = 1, .mB = 0, .mA = 1 } );
    Line( origin, vec3( world * vec4( 0.0f, 0.0f, length, 1.0f ) ), Color{ .mR = 0, .mG = 0, .mB = 1, .mA = 1 } );
}

vector<DebugLine> DebugDraw::Take()
{
    return exchange( mLines, {} );
}
}
