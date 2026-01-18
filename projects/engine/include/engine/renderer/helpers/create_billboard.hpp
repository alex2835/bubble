#pragma once
#include "engine/renderer/model.hpp"

namespace bubble
{
inline Ref<Mesh> CreateBillboardQuadMesh()
{
    // Create a centered quad (-0.5 to 0.5) for billboard rendering
    VertexBufferData vertices;

    // Positions (centered quad)
    vertices.mPositions = {
        vec3( -0.5f, -0.5f, 0.0f ),  // Bottom-left
        vec3( 0.5f, -0.5f, 0.0f ),  // Bottom-right
        vec3( 0.5f,  0.5f, 0.0f ),  // Top-right
        vec3( -0.5f,  0.5f, 0.0f )   // Top-left
    };

    // Texture coordinates
    vertices.mTexCoords = {
        vec2( 0.0f, 0.0f ),  // Bottom-left
        vec2( 1.0f, 0.0f ),  // Bottom-right
        vec2( 1.0f, 1.0f ),  // Top-right
        vec2( 0.0f, 1.0f )   // Top-left
    };

    // Normals (facing forward)
    vertices.mNormals = {
        vec3( 0.0f, 0.0f, 1.0f ),
        vec3( 0.0f, 0.0f, 1.0f ),
        vec3( 0.0f, 0.0f, 1.0f ),
        vec3( 0.0f, 0.0f, 1.0f )
    };

    // Indices (two triangles)
    vector<u32> indices = {
        0, 1, 2,  // First triangle
        0, 2, 3   // Second triangle
    };

    // Create basic material (will be overridden by billboard shader)
    BasicMaterial material;

    return CreateRef<Mesh>( "BillboardQuad", 
                            std::move( material ),
                            std::move( vertices ),
                            std::move( indices ) );
}

} // bubble