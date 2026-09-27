#pragma once
#include "engine/utils/filesystem.hpp"
#include "engine/scene/scene.hpp"
#include "engine/types/json.hpp"

namespace bubble
{
class Project;

constexpr string_view LEVEL_FILE_EXT = ".level"sv;

// One playable scene of a project. A project owns many level files and has
// exactly one open at a time; resources, the Lua VM and global_state live on
// the project and survive a level switch, everything here does not.
//
// The scene is one tree of entities under a root that stands for the level
// itself - named after it, a folder at the top of the Entities window. A
// prefab file is a level file too, its root the prefab's.
//
// Held by value, never through a pointer: the Lua scene bindings capture a
// Scene& once, so a switch has to load into the same object rather than
// replace it.
class Level
{
private:
    json SaveScene( const Project& project ) const;
    void LoadScene( const json& j, Project& project );
    // Files from before the scene was a tree kept the tree beside it, with
    // folders that were not entities. Rebuilt from that, folders made.
    void MigrateTree( const json& tree );
    // Anything hanging from nothing but the root goes under the root;
    // entities with no components at all, which old files could hold, go.
    void AdoptStrays();

public:
    Level();
    Level( const Level& ) = delete;
    Level& operator=( const Level& ) = delete;

    // Back to the state of a freshly constructed level: an empty scene with
    // a root. Drops the scene - and with it every Lua table the state
    // components hold - before anything else, so nothing in the VM keeps
    // pointing at dead entities.
    void Clear();

    // Serialization is split from the file so the same code handles a level
    // file and a legacy project file with the level embedded in it.
    json ToJson( const Project& project ) const;
    void FromJson( const json& j, Project& project );

    void Load( const path& absFile, Project& project );
    void Save( const path& absFile, const Project& project ) const;
    bool IsValid() const { return not mFile.empty(); }

    Entity Root() const { return mScene.Root(); }
    // What the tree shows at its top: the level's name.
    void SetRootName( const string& name );
    // A new root, a folder named `name`; the old one, if any, is left as it is.
    Entity MakeRoot( const string& name );

    string mName;  // file stem
    path mFile;    // absolute
    Scene mScene;
};

}
