#pragma once
#include "bubble/core/jobs.hpp"
#include "bubble/reflection/reflection.hpp"
#include "bubble/scene/component_types.hpp"
#include "bubble/world/module.hpp"

namespace bubble
{
struct AppOptions
{
    // Jobs' threads, the main one among them; 0 is one a core.
    u32 mThreads = 0;
};

// What there is one of in a process: the pool of threads, the descriptions
// of types, the components files hold, and the modules every world is made
// with. Worlds - the game, a prefab's preview, a test - take it by
// reference, and it outlives them. The thread that makes it is the main
// one.
class App
{
public:
    explicit App( AppOptions options = {} );
    App( const App& ) = delete;
    App& operator=( const App& ) = delete;

    // Describes the module's components now; every world made after this
    // has the module, in the order modules were added. A name taken already
    // is a logic_error.
    void AddModule( ModuleType type );
    span<const ModuleType> Modules() const { return mModules; }

    Jobs& GetJobs() { return mJobs; }
    Reflection& GetReflection() { return mReflection; }
    ComponentTypes& GetComponentTypes() { return mComponentTypes; }
    const ComponentTypes& GetComponentTypes() const { return mComponentTypes; }

private:
    Jobs mJobs;
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    vector<ModuleType> mModules;
};
}
