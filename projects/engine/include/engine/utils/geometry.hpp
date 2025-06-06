#pragma once
#include "engine/types/glm.hpp"
#include "engine/types/array.hpp"
#include "engine/types/number.hpp"


namespace bubble
{
// Shape generation
struct ShapeData
{
    vector<vec3> vertices;
    vector<u32> indices;
};
ShapeData GenerateSphereLinesShape( float radius = 1.0f );
ShapeData GenerateCubeLinesShape( vec3 he = vec3( 1.0f ) );


// BBox
struct BBoxShapeData
{
    array<vec3, 8> vertices;
    array<u32, 24> indices;
};
AABB CalculateTransformedBBox( const AABB& box, const mat4& transform );
BBoxShapeData CalculateBBoxShapeData( AABB box );

}