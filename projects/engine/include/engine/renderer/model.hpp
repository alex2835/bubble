#pragma once
#include "engine/types/number.hpp"
#include "engine/types/string.hpp"
#include "engine/types/array.hpp"
#include "engine/types/glm.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/material.hpp"
#include "engine/renderer/webgpu.hpp"
#include "engine/animation/skeleton.hpp"

namespace bubble
{
// Per vertex skinning data. Kept beside VertexBufferData rather than in it,
// because nothing on the GPU reads it: skinning runs on the CPU, and the posed
// vertices reach the GPU through UpdateDynamicVertexBufferData like any other
// dynamic mesh. Joint indices are in the model's Skeleton order.
struct MeshSkin
{
    vector<glm::u16vec4> mJointIndices;
    vector<vec4> mJointWeights;

    bool Empty() const { return mJointIndices.empty(); }
};


class Mesh
{
public:
    Mesh() = default;
    Mesh( string name,
          BasicMaterial material,
          VertexBufferData vertices,
          vector<u32> indices,
          MeshSkin skin = {} );

    Mesh( const Mesh& ) = delete;
    Mesh& operator= ( const Mesh& ) = delete;

    Mesh( Mesh&& ) = default;
    Mesh& operator= ( Mesh&& ) = default;

    void BindBuffers( wgpu::RenderPassEncoder pass ) const;
    u64 IndiciesSize() const;

    void UpdateDynamicVertexBufferData( VertexBufferData vertices, vector<u32> indices );

    void ApplyMaterial( wgpu::RenderPassEncoder pass ) const;

public:
    string mName;
    MeshBuffers mBuffers;
    VertexBufferData mVertices;
    vector<u32> mIndices;
    BasicMaterial mMaterial;
    MeshSkin mSkin;
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
    // Set only for a skinned model. The clips were imported with the skeleton
    // and are in its joint order, so they are the model's and not a project
    // resource of their own.
    Ref<Skeleton> mSkeleton;
    vector<Ref<AnimationClip>> mClips;

    Model();
    Model( const Model& ) = delete;
    Model& operator= ( const Model& ) = delete;
    Model( Model&& ) = default;
    Model& operator= ( Model&& ) = default;

    static AABB CreateBoundingBox( const Model& model );

    bool Skinned() const { return mSkeleton != nullptr; }
    const Ref<AnimationClip>& FindClip( string_view name ) const;
};

}