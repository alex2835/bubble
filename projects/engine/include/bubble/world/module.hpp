#pragma once
#include "bubble/types/function.hpp"
#include "bubble/types/pointer.hpp"
#include "bubble/types/string.hpp"

namespace bubble
{
class ComponentTypes;
class Reflection;
class World;

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
};
}
