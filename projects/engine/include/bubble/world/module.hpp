#pragma once
#include "bubble/types/function.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/pointer.hpp"
#include "bubble/types/string.hpp"

namespace bubble
{
class ComponentTypes;
class Reflection;
class ScriptApi;
class World;

// A module's part of a WorldSnapshot, its own to read.
class ModuleSnapshot
{
public:
    virtual ~ModuleSnapshot() = default;
};

// A part of the engine that turns data into behaviour: physics, audio,
// scripts. One is made for each world, and it reaches the rest of the world
// only through what World gives it - data events, steps in the frame,
// services and events. It never knows another module.
//
// Its components are data, described for files and the editor. What it
// builds from them - a physics body, a voice - lives in runtime components
// beside them: plain structs no one describes, never saved or copied, built
// again from the data whenever the data comes back (undo, a snapshot).
class Module
{
public:
    virtual ~Module() = default;

    // What the module keeps that no component holds - Luau's self tables,
    // a physics engine's state - as a WorldSnapshot carries it; nothing when
    // the components say it all.
    virtual Ref<const ModuleSnapshot> TrySnapshot() { return nullptr; }
    // World::Restore is about to put the scene back as a snapshot has it;
    // `snapshotMaybe` is the module's part of it, made by this module or by
    // the same module of another world of the App. The scene's components
    // then go and come back as data events.
    virtual void Restoring( OptRef<const ModuleSnapshot> snapshotMaybe ) { (void)snapshotMaybe; }
    // And then they are back, the runtime built again: what a module keeps
    // beyond them can be laid on it.
    virtual void Restored() {}
};

// What App knows of a module: how to describe its components, once a
// process, and how to make one for a world.
struct ModuleType
{
    // snake_case, for messages: "physics".
    string mName;
    // Describes the module's components and adds them to the types files
    // hold. Called once, when App takes the module.
    function<void( Reflection&, ComponentTypes& )> mDescribe;
    // Makes the module for a world. It subscribes to data events, adds its
    // steps and its services there, and leaves the scene alone: the world
    // tells it about every component there is once all modules are made.
    function<Scope<Module>( World& )> mCreate;
    // What the module gives a world's scripts beyond its components'
    // reflected fields: see ScriptApi. Asked for each world's scripts, and
    // for bubble.d.luau.
    function<void( ScriptApi& )> mScripts = {};
    // Made in the editor's worlds of documents too (WorldOptions::mEditing):
    // a module that shows the scene as it is - transforms, rendering, the
    // UI's layout, a pose. The rest - scripts, physics, sound - wait for Play.
    bool mInEditor = false;
};
}
