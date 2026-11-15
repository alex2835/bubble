#pragma once
#include "bubble/reflection/reflection.hpp"
#include "bubble/scene/component_types.hpp"
#include "bubble/world/module.hpp"

namespace bubble
{
// What there is one of in a process: the descriptions of types, the
// components files hold, and the modules every world is made with. Worlds -
// the game, a prefab's preview, a test - take it by reference, and it
// outlives them.
class App
{
public:
    App();
    App( const App& ) = delete;
    App& operator=( const App& ) = delete;

    // Describes the module's components now; every world made after this
    // has the module, in the order modules were added. A name taken already
    // is a logic_error.
    void AddModule( ModuleType type );
    span<const ModuleType> Modules() const { return mModules; }

    Reflection& GetReflection() { return mReflection; }
    ComponentTypes& GetComponentTypes() { return mComponentTypes; }
    const ComponentTypes& GetComponentTypes() const { return mComponentTypes; }

private:
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    vector<ModuleType> mModules;
};
}
