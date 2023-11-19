#pragma once
#include <vector>
#include "glm/glm.hpp"
#include "utils/imexp.hpp"
#include "renderer/buffer.hpp"
#include "renderer/texture.hpp"
#include "renderer/vertex_array.hpp"
#include "renderer/material.hpp"

namespace bubble
{
struct VertexData
{
    std::vector<glm::vec3> mPositions;
    std::vector<glm::vec3> mNormals;
    std::vector<glm::vec2> mTexCoords;
    std::vector<glm::vec3> mTangents;
    std::vector<glm::vec3> mBitangents;
};


class BUBBLE_ENGINE_EXPORT Mesh
{
public:
    Mesh() = default;
    Mesh( const std::string& name,
          BasicMaterial&& Material,
          VertexData&& vertices,
          std::vector<uint32_t>&& indices );

    Mesh( const Mesh& ) = delete;
    Mesh& operator= ( const Mesh& ) = delete;

    Mesh( Mesh&& ) = default;
    Mesh& operator= ( Mesh&& ) = default;

private:
    std::string			  mName;
    VertexArray           mVertexArray;
    BasicMaterial         mMaterial;
    VertexData            mVertices;
    std::vector<uint32_t> mIndices;
};

}