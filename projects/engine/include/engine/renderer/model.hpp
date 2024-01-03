#pragma once
#include <vector>
#include "engine/utils/imexp.hpp"
#include "engine/renderer/mesh.hpp"

namespace bubble
{
struct MeshNode
{
    std::string mName;
    std::vector<Mesh*> mMeshes;
    std::vector<Scope<MeshNode>> mChildern;

    MeshNode() = default;
    MeshNode( const std::string& name )
        : mName( name )
    {
    }
};

struct BUBBLE_ENGINE_EXPORT Model
{
    std::vector<Mesh> mMeshes;
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