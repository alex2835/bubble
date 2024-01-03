#pragma once
#include <vector>
#include "glm/glm.hpp"
#include "engine/utils/imexp.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/material.hpp"

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

    void BindVertetxArray() const;
    size_t IndiciesSize() const;

private:
    std::string			  mName;
    VertexArray           mVertexArray;
    VertexData            mVertices;
    std::vector<uint32_t> mIndices;
    BasicMaterial         mMaterial;
};

}