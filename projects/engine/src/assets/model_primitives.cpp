#include "bubble/assets/model_primitives.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/matrix.hpp"

namespace bubble
{
namespace
{
// A face of a box `half` from the centre each way, facing `normal`: its
// corners along `across` and `up`, which turn counter-clockwise about the
// normal (across × up = normal), so the face does too seen from outside.
void AddFace( ModelMesh& mesh, vec3 normal, vec3 across, vec3 up, vec3 half )
{
    const u32 first = static_cast<u32>( mesh.mVertices.size() );
    const vec3 centre = normal * half;
    for ( const vec2 corner : { vec2( -1.0f, -1.0f ), vec2( 1.0f, -1.0f ), vec2( 1.0f, 1.0f ), vec2( -1.0f, 1.0f ) } )
        mesh.mVertices.push_back( ModelVertex{
            .mPosition = centre + ( across * corner.x + up * corner.y ) * half,
            .mNormal = normal,
            // The image's top at the face's top.
            .mUv = vec2( ( corner.x + 1.0f ) * 0.5f, ( 1.0f - corner.y ) * 0.5f ),
        } );
    vector<u32>& indices = mesh.mLods.front().mIndices;
    for ( const u32 index : { 0u, 1u, 2u, 0u, 2u, 3u } )
        indices.push_back( first + index );
}

ModelAsset WithOneMesh( ModelMesh mesh )
{
    ModelAsset model;
    model.mMin = vec3( numeric_limits<f32>::max() );
    model.mMax = vec3( -numeric_limits<f32>::max() );
    for ( const ModelVertex& vertex : mesh.mVertices )
    {
        model.mMin = glm::min( model.mMin, vertex.mPosition );
        model.mMax = glm::max( model.mMax, vertex.mPosition );
    }
    model.mMeshes.push_back( std::move( mesh ) );
    return model;
}

ModelMesh EmptyMesh()
{
    ModelMesh mesh;
    mesh.mMaterial = "default";
    mesh.mLods.push_back( ModelLod{ .mError = 0.0f, .mIndices = {} } );
    return mesh;
}
}

ModelAsset MakeBox( vec3 size )
{
    const vec3 half = size * 0.5f;
    const vec3 x( 1.0f, 0.0f, 0.0f );
    const vec3 y( 0.0f, 1.0f, 0.0f );
    const vec3 z( 0.0f, 0.0f, 1.0f );
    ModelMesh mesh = EmptyMesh();
    AddFace( mesh, x, -z, y, half );
    AddFace( mesh, -x, z, y, half );
    AddFace( mesh, z, x, y, half );
    AddFace( mesh, -z, -x, y, half );
    AddFace( mesh, y, x, -z, half );
    AddFace( mesh, -y, x, z, half );
    return WithOneMesh( std::move( mesh ) );
}

ModelAsset MakeSphere( f32 radius, u32 rings )
{
    rings = max( rings, 2u );
    const u32 around = rings * 2;
    ModelMesh mesh = EmptyMesh();
    // A grid of (around + 1) x (rings + 1) points: the seam and the poles
    // repeated, each with its own uv.
    for ( u32 ring = 0; ring <= rings; ++ring )
    {
        const f32 latitude = glm::pi<f32>() * static_cast<f32>( ring ) / static_cast<f32>( rings );
        for ( u32 step = 0; step <= around; ++step )
        {
            const f32 longitude = 2.0f * glm::pi<f32>() * static_cast<f32>( step ) / static_cast<f32>( around );
            const vec3 normal( sin( latitude ) * sin( longitude ), cos( latitude ),
                               sin( latitude ) * cos( longitude ) );
            mesh.mVertices.push_back( ModelVertex{
                .mPosition = normal * radius,
                .mNormal = normal,
                .mUv = vec2( static_cast<f32>( step ) / static_cast<f32>( around ),
                             static_cast<f32>( ring ) / static_cast<f32>( rings ) ),
            } );
        }
    }
    vector<u32>& indices = mesh.mLods.front().mIndices;
    for ( u32 ring = 0; ring < rings; ++ring )
        for ( u32 step = 0; step < around; ++step )
        {
            const u32 here = ring * ( around + 1 ) + step;
            const u32 below = here + around + 1;
            // Counter-clockwise seen from outside.
            for ( const u32 index : { here, below, here + 1, here + 1, below, below + 1 } )
                indices.push_back( index );
        }
    return WithOneMesh( std::move( mesh ) );
}

ModelAsset MakePlane( f32 size )
{
    ModelMesh mesh = EmptyMesh();
    const vec3 half( size * 0.5f, 0.0f, size * 0.5f );
    AddFace( mesh, vec3( 0.0f, 1.0f, 0.0f ), vec3( 1.0f, 0.0f, 0.0f ), vec3( 0.0f, 0.0f, -1.0f ), half );
    return WithOneMesh( std::move( mesh ) );
}
}
