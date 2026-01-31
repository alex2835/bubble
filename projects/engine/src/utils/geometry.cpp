
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

ShapeData GenerateCapsuleLinesShape( f32 radius /*= 0.5f*/, f32 height /*= 1.0f*/ )
{
    u32 segments = 16;
    u32 rings = 8;
    f32 halfHeight = height / 2.0f;

    vector<vec3> vertices;
    vector<u32> indices;

    // Top hemisphere (centered at +halfHeight on Y axis)
    for ( u32 i = 0; i <= rings / 2; ++i )
    {
        f32 stackAngle = glm::pi<f32>() / 2 - i * glm::pi<f32>() / rings;
        f32 xy = radius * cosf( stackAngle );
        f32 y = radius * sinf( stackAngle ) + halfHeight;

        for ( u32 j = 0; j <= segments; ++j )
        {
            f32 sectorAngle = j * 2 * glm::pi<f32>() / segments;
            f32 x = xy * cosf( sectorAngle );
            f32 z = xy * sinf( sectorAngle );
            vertices.emplace_back( x, y, z );
        }
    }

    u32 topHemiVerts = static_cast<u32>( vertices.size() );

    // Bottom hemisphere (centered at -halfHeight on Y axis)
    for ( u32 i = rings / 2; i <= rings; ++i )
    {
        f32 stackAngle = glm::pi<f32>() / 2 - i * glm::pi<f32>() / rings;
        f32 xy = radius * cosf( stackAngle );
        f32 y = radius * sinf( stackAngle ) - halfHeight;

        for ( u32 j = 0; j <= segments; ++j )
        {
            f32 sectorAngle = j * 2 * glm::pi<f32>() / segments;
            f32 x = xy * cosf( sectorAngle );
            f32 z = xy * sinf( sectorAngle );
            vertices.emplace_back( x, y, z );
        }
    }

    // Top hemisphere horizontal lines
    for ( u32 i = 0; i <= rings / 2; ++i )
    {
        for ( u32 j = 0; j < segments; ++j )
        {
            u32 cur = i * ( segments + 1 ) + j;
            indices.push_back( cur );
            indices.push_back( cur + 1 );
        }
    }

    // Top hemisphere vertical lines
    for ( u32 j = 0; j <= segments; ++j )
    {
        for ( u32 i = 0; i < rings / 2; ++i )
        {
            u32 cur = i * ( segments + 1 ) + j;
            indices.push_back( cur );
            indices.push_back( cur + ( segments + 1 ) );
        }
    }

    // Bottom hemisphere horizontal lines
    u32 bottomStart = topHemiVerts;
    u32 bottomRings = rings / 2 + 1;
    for ( u32 i = 0; i < bottomRings; ++i )
    {
        for ( u32 j = 0; j < segments; ++j )
        {
            u32 cur = bottomStart + i * ( segments + 1 ) + j;
            indices.push_back( cur );
            indices.push_back( cur + 1 );
        }
    }

    // Bottom hemisphere vertical lines
    for ( u32 j = 0; j <= segments; ++j )
    {
        for ( u32 i = 0; i < bottomRings - 1; ++i )
        {
            u32 cur = bottomStart + i * ( segments + 1 ) + j;
            indices.push_back( cur );
            indices.push_back( cur + ( segments + 1 ) );
        }
    }

    // Connect top and bottom hemispheres with vertical lines
    u32 topEquatorStart = ( rings / 2 ) * ( segments + 1 );
    for ( u32 j = 0; j <= segments; ++j )
    {
        indices.push_back( topEquatorStart + j );
        indices.push_back( bottomStart + j );
    }

    return ShapeData{ std::move( vertices ), std::move( indices ) };
}

ShapeData GenerateFrustumLinesShape( f32 fov, f32 aspectRatio, f32 nearDist, f32 farDist )
{
    f32 tanHalfFov = glm::tan( fov * 0.5f );
    f32 nearHeight = nearDist * tanHalfFov;
    f32 nearWidth = nearHeight * aspectRatio;
    f32 farHeight = farDist * tanHalfFov;
    f32 farWidth = farHeight * aspectRatio;

    // Near plane corners (looking down -Z axis)
    // nTL, nTR, nBR, nBL
    vector<vec3> vertices = {
        vec3( -nearWidth,  nearHeight, -nearDist ), // 0 - near top left
        vec3(  nearWidth,  nearHeight, -nearDist ), // 1 - near top right
        vec3(  nearWidth, -nearHeight, -nearDist ), // 2 - near bottom right
        vec3( -nearWidth, -nearHeight, -nearDist ), // 3 - near bottom left

        vec3( -farWidth,  farHeight, -farDist ),    // 4 - far top left
        vec3(  farWidth,  farHeight, -farDist ),    // 5 - far top right
        vec3(  farWidth, -farHeight, -farDist ),    // 6 - far bottom right
        vec3( -farWidth, -farHeight, -farDist ),    // 7 - far bottom left
    };

    vector<u32> indices = {
        // Near plane
        0, 1,  1, 2,  2, 3,  3, 0,
        // Far plane
        4, 5,  5, 6,  6, 7,  7, 4,
        // Connecting edges
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

BBoxShapeData CalculateBBoxShapeData( AABB box )
{
    vec3 min = box.getMin();
    vec3 max = box.getMax();
    array<vec3, 8> vertices{ 
        vec3( min.x, min.y, min.z ),
        vec3( max.x, min.y, min.z ),
        vec3( max.x, max.y, min.z ),
        vec3( min.x, max.y, min.z ),
        vec3( min.x, min.y, max.z ),
        vec3( max.x, min.y, max.z ),
        vec3( max.x, max.y, max.z ),
        vec3( min.x, max.y, max.z )
    };

    array<u32, 24> indices = {
        0, 1,  1, 2,  2, 3,  3, 0,// bottom lines
        4, 5,  5, 6,  6, 7,  7, 4,// top lines
        0, 4,  1, 5,  2, 6,  3, 7 // vertical edges lines
    };
    return BBoxShapeData{ vertices, indices };
}

}