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
    json SaveScene() const;
    void LoadScene( const json& j );
    
    json SaveProjectTreeNode( const Ref<ProjectTreeNode>& node ) const;
    json SaveProjectTree() const;
    Ref<ProjectTreeNode> LoadProjectTreeNode( const json& j, const Ref<ProjectTreeNode>& parent );
    void LoadProjectTree( const json& j );

public:
    explicit Project();
    ~Project();
    Project( Project&& ) = default;
    Project& operator=( Project&& ) = default;

    void Create( const path& rootDir, const string& projectName );
    void Open( const path& rootFile );
    void Save() const;
    bool IsValid() const;

    string mName;
    path mRootFile;
    Loader mLoader;
    ScriptingEngine mScriptingEngine;
    Scope<Any> mGlobalState;
    Scene mScene;
    Ref<ProjectTreeNode> mProjectTreeRoot; // Tree view for a scene
    u64 mNodeIDCounter = 0;
};

}
