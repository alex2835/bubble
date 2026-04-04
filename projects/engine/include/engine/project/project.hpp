#pragma once
#include "engine/utils/filesystem.hpp"
#include "engine/scene/scene.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/types/any.hpp"
#include "project_tree.hpp"

namespace bubble
{
class Project
{
private:
    void LoadDefaultResources();

protected:
    json SaveScene();
    void LoadScene( const json& j );
    
    json SaveProjectTreeNode( const Ref<ProjectTreeNode>& node );
    json SaveProjectTree();
    Ref<ProjectTreeNode> LoadProjectTreeNode( const json& j, const Ref<ProjectTreeNode>& parent );
    void LoadProjectTree( const json& j );

public:
    explicit Project();
    ~Project();
    void Create( const path& rootDir, const string& projectName );
    void Open( const path& rootFile );
    void Save();
    bool IsValid();

    string mName;
    path mRootFile;
    Loader mLoader;
    ScriptingEngine mScriptingEngine;
    Scope<Any> mGlobalState;
    Scene mScene;
    Ref<ProjectTreeNode> mProjectTreeRoot; // Tree view for a scene
};

}
