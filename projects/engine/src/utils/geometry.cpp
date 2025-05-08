
#include "engine/utils/geometry.hpp"

namespace bubble
{

ShapeData GenerateSphereLinesShape( f32 radius /*= 1.0f */ )
{
    u32 stacks = 8;
    u32 sectors = 16;
    vector<vec3> vertices;
    vertices.reserve( stacks * sectors );

    for ( u32 i = 0; i <= stacks; ++i )
    {
        f32 stackAngle = glm::pi<f32>() / 2 - i * glm::pi<f32>() / stacks; // from pi/2 to -pi/2
        f32 xy = radius * cosf( stackAngle );
        f32 z = radius * sinf( stackAngle );

        for ( u32 j = 0; j <= sectors; ++j )
        {
            f32 sectorAngle = j * 2 * glm::pi<f32>() / sectors;
            f32 x = xy * cosf( sectorAngle );
            f32 y = xy * sinf( sectorAngle );
            vertices.emplace_back( x, y, z );
        }
    }

    vector<u32> indices;
    indices.reserve( stacks * sectors * 4 );
    // Horizontal lines (rings)
    for ( u32 i = 0; i <= stacks; ++i )
    {
        for ( u32 j = 0; j < sectors; ++j )
        {
            u32 cur = i * ( sectors + 1 ) + j;
            indices.push_back( cur );
            indices.push_back( cur + 1 );
        }
    }

    // Vertical lines (meridians)
    for ( u32 j = 0; j <= sectors; ++j )
    {
        for ( u32 i = 0; i < stacks; ++i )
        {
            u32 cur = i * ( sectors + 1 ) + j;
            indices.push_back( cur );
            indices.push_back( cur + ( sectors + 1 ) );
        }
    }
    return ShapeData{ std::move( vertices ), std::move( indices ) };
}


ShapeData GenerateCubeLinesShape( vec3 he /*= vec3( 1.0f ) */ )
{
    vector<vec3> vertices = {
        // bottom face (y = -he.y)
        {-he.x, -he.y, -he.z}, // 0
        { he.x, -he.y, -he.z}, // 1
        { he.x, -he.y,  he.z}, // 2
        {-he.x, -he.y,  he.z}, // 3

        // top face (y = +he.y)
        {-he.x,  he.y, -he.z}, // 4
        { he.x,  he.y, -he.z}, // 5
        { he.x,  he.y,  he.z}, // 6
        {-he.x,  he.y,  he.z}, // 7
    };

    vector<u32> indices = {
        // bottom face
        0, 1,  1, 2,  2, 3,  3, 0,
        // top face
        4, 5,  5, 6,  6, 7,  7, 4,
        // vertical edges
        0, 4,  1, 5,  2, 6,  3, 7
    };
    return ShapeData{ std::move( vertices ), std::move( indices ) };
}

AABB CalculateTransformedBBox( const AABB& box, const mat4& transform )
{
    AABB newBox;
    vec3 min = box.getMin();
    vec3 max = box.getMax();
    newBox.extend( vec3( transform * vec4( min.x, min.y, min.z, 1 ) ) );
    newBox.extend( vec3( transform * vec4( max.x, min.y, min.z, 1 ) ) );
    newBox.extend( vec3( transform * vec4( max.x, max.y, min.z, 1 ) ) );
    newBox.extend( vec3( transform * vec4( min.x, max.y, min.z, 1 ) ) );
    newBox.extend( vec3( transform * vec4( min.x, min.y, max.z, 1 ) ) );
    newBox.extend( vec3( transform * vec4( max.x, min.y, max.z, 1 ) ) );
    newBox.extend( vec3( transform * vec4( max.x, max.y, max.z, 1 ) ) );
    newBox.extend( vec3( transform * vec4( min.x, max.y, max.z, 1 ) ) );
    return newBox;
}

}