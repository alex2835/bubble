#pragma once
#include <vector>
#include "engine/utils/imexp.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/material.hpp"

namespace bubble
{
struct VertexData
{
    vector<vec3> mPositions;
    vector<vec3> mNormals;
    vector<vec2> mTexCoords;
    vector<vec3> mTangents;
    vector<vec3> mBitangents;
};


class BUBBLE_ENGINE_EXPORT Mesh
{
public:
    Mesh() = default;
    Mesh( const string& name,
          BasicMaterial&& Material,
          VertexData&& vertices,
          vector<u32>&& indices );

    Mesh( const Mesh& ) = delete;
    Mesh& operator= ( const Mesh& ) = delete;

    Mesh( Mesh&& ) = default;
    Mesh& operator= ( Mesh&& ) = default;

    void BindVertetxArray() const;
    size_t IndiciesSize() const;

private:
    string			  mName;
    VertexArray           mVertexArray;
    VertexData            mVertices;
    vector<u32> mIndices;
    BasicMaterial         mMaterial;
};

}