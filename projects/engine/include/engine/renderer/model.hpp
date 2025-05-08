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
class Mesh
{
public:
    Mesh() = default;
    Mesh( string name,
          BasicMaterial material,
          VertexBufferData vertices,
          vector<u32> indices,
          BufferType vbType = BufferType::Static );

    Mesh( const Mesh& ) = delete;
    Mesh& operator= ( const Mesh& ) = delete;

    Mesh( Mesh&& ) = default;
    Mesh& operator= ( Mesh&& ) = default;

    void BindVertexArray() const;
    u64 IndiciesSize() const;

    void UpdateDynamicVertexBufferData( VertexBufferData vertices );
    void UpdateDynamicVertexBufferData( VertexBufferData vertices, vector<u32> indices );

    void ApplyMaterial( const Ref<Shader>& shader ) const;

public:
    string mName;
    VertexArray mVertexArray;
    VertexBufferData mVertices;
    vector<u32> mIndices;
    BasicMaterial mMaterial;
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
    AABB mBBox;

    Model();
    Model( const Model& ) = delete;
    Model& operator= ( const Model& ) = delete;
    Model( Model&& ) = default;
    Model& operator= ( Model&& ) = default;

    static AABB CreateBoundingBox( const Model& model );
};

}