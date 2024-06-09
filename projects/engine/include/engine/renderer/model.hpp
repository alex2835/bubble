#pragma once
#include "engine/utils/imexp.hpp"
#include "engine/utils/types.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/material.hpp"
#include <vector>

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

struct BUBBLE_ENGINE_EXPORT Model
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