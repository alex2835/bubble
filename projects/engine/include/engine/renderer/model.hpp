#pragma once
#include <vector>
#include "engine/utils/imexp.hpp"
#include "engine/renderer/mesh.hpp"

namespace bubble
{
struct MeshNode
{
    string mName;
    vector<Mesh*> mMeshes;
    vector<Scope<MeshNode>> mChildern;

    MeshNode() = default;
    MeshNode( const string& name )
        : mName( name )
    {
    }
};

struct BUBBLE_ENGINE_EXPORT Model
{
    vector<Mesh> mMeshes;
    Ref<Shader> mShader;
    //AABB mBoundingBox;
    Scope<MeshNode> mRootNode;

    Model();
    Model( const Model& ) = delete;
    Model& operator= ( const Model& ) = delete;
    Model( Model&& ) = default;
    Model& operator= ( Model&& ) = default;

    void CreateBoundingBox();
};

}