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
ShapeData GenerateCapsuleLinesShape( float radius = 0.5f, float height = 1.0f );
ShapeData GenerateFrustumLinesShape( float fov, float aspectRatio, float nearDist, float farDist );


// BBox
struct BBoxShapeData
{
    array<vec3, 8> vertices;
    array<u32, 24> indices;
};
AABB CalculateTransformedBBox( const AABB& box, const mat4& transform );
BBoxShapeData CalculateBBoxShapeData( AABB box );

// Normals
// The matrix a normal has to be transformed by to survive a non-uniform scale:
// transpose(inverse(model)). One value for the whole draw, so the vertex stage
// has no business recomputing it per vertex.
mat3 CalculateNormalMat( const mat4& transform );

// Generate camera mesh

}