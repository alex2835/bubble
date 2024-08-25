#pragma once
#include "engine/types/number.hpp"
#include "engine/types/string.hpp"
#include "engine/types/array.hpp"
#include "engine/types/glm.hpp"
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

class Mesh
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

    void BindVertexArray() const;
    u64 IndiciesSize() const;

    void ApplyMaterial( const Ref<Shader>& shader ) const;

private:
    string mName;
    VertexArray mVertexArray;
    VertexData mVertices;
    vector<u32> mIndices;
    BasicMaterial mMaterial;
    AABB mBoundingBox;
};


struct MeshTreeViewNode
{
    string mName;
    vector<Mesh*> mMeshes;
    vector<Scope<MeshTreeViewNode>> mChildren;

    MeshTreeViewNode() = default;
    MeshTreeViewNode( const string& name )
        : mName( name )
    {}
};

struct Model
{
    string mName;
    path mPath;
    vector<Mesh> mMeshes;
    Scope<MeshTreeViewNode> mRootMeshTreeView;
    AABB mBoundingBox;

    Model();
    Model( const Model& ) = delete;
    Model& operator= ( const Model& ) = delete;
    Model( Model&& ) = default;
    Model& operator= ( Model&& ) = default;
};

}