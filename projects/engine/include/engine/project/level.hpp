#pragma once
#include "engine/utils/filesystem.hpp"
#include "engine/scene/scene.hpp"
#include "engine/types/json.hpp"
#include "project_tree.hpp"

namespace bubble
{
class Project;

constexpr string_view LEVEL_FILE_EXT = ".level"sv;

// One playable scene of a project: the entities and the editor hierarchy that
// organizes them. A project owns many level files and has exactly one open at
// a time; resources, the Lua VM and global_state live on the project and
// survive a level switch, everything here does not.
//
// Held by value, never through a pointer: the Lua scene bindings capture a
// Scene& once, so a switch has to load into the same object rather than
// replace it.
class Level
{
private:
    json SaveScene( const Project& project ) const;
    void LoadScene( const json& j, Project& project );

    json SaveTreeNode( const Ref<ProjectTreeNode>& node ) const;
    json SaveTree() const;
    Ref<ProjectTreeNode> LoadTreeNode( const json& j, const Ref<ProjectTreeNode>& parent );
    void LoadTree( const json& j );

public:
    Level();
    // Every tree node keeps a pointer back to mNodeIDCounter, so a moved-from
    // level would leave the whole tree counting into a dead object.
    Level( const Level& ) = delete;
    Level& operator=( const Level& ) = delete;

    // Back to the state of a freshly constructed level. Drops the scene - and
    // with it every Lua table the state components hold - before anything
    // else, so nothing in the VM keeps pointing at dead entities.
    void Clear();

    // Serialization is split from the file so the same code handles a level
    // file and a legacy project file with the level embedded in it.
    json ToJson( const Project& project ) const;
    void FromJson( const json& j, Project& project );

    void Load( const path& absFile, Project& project );
    void Save( const path& absFile, const Project& project ) const;
    bool IsValid() const { return not mFile.empty(); }

    string mName;  // file stem
    path mFile;    // absolute
    Scene mScene;
    // The counter before the root: the root's constructor draws its id from
    // it, and members initialize in declaration order.
    u64 mNodeIDCounter = 0;
    Ref<ProjectTreeNode> mTreeRoot;
};

}
