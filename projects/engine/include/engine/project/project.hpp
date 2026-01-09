#pragma once
#include "engine/utils/filesystem.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/loader/loader.hpp"
#include "project_tree.hpp"

namespace bubble
{
class Project
{
    void LoadDefaultResources();
    json SaveScene();
    void LoadScene( const json& j );
    
    json SaveProjectTreeNode( const Ref<ProjectTreeNode>& node );
    json SaveProjectTree();
    Ref<ProjectTreeNode> LoadProjectTreeNode( const json& j, ProjectTreeNode* parent );
    void LoadProjectTree( const json& j );

public:
    explicit Project( WindowInput& input );
    ~Project();
    void Create( const path& rootDir, const string& projectName );
    void Open( const path& rootFile );
    void Save();
    bool IsValid();

    string mName;
    path mRootFile;
    ScriptingEngine mScriptingEngine;
    PhysicsEngine mPhysicsEngine;
    Loader mLoader;
    Scene mScene;
    // Just tree view for a scene
    Ref<ProjectTreeNode> mProjectTreeRoot;
};

}
