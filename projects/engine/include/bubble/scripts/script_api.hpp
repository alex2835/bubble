#pragma once
#include "bubble/core/name.hpp"
#include "bubble/scene/entity.hpp"
#include "bubble/scripts/lua.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/math.hpp"
#include "bubble/types/variant.hpp"

namespace bubble
{
class App;
class World;

// A component as a world's scripts hold it: its entity. Each component has
// a Luau type of its own over it, so the type says which.
struct ScriptComponent
{
    Entity mEntity;
};

// What a module's call into a script passes after self.
using ScriptArg = variant<bool, f64, vec3, Entity, string>;

// A callback of an entity's script that a module calls - physics'
// on_collision - sent through the world's events, so it comes where the
// frame's order says: after the step's physics. The scripts module calls it
// on the instance of `mTarget`, if that has one and its script has the
// callback; else it goes unheard.
struct ScriptCall
{
    Entity mTarget;
    Name mCallback;
    vector<ScriptArg> mArgs;
};

// What a module adds to its worlds' scripts beyond what reflection says of
// its components: members that reach its runtime - a body's velocity, a
// character's ground -, globals of its own - physics.raycast -, and the
// callbacks its ScriptCalls call - on_collision. Each comes with its Luau
// declaration, for bubble.d.luau. A module fills one in ModuleType's
// mScripts; the scripts module applies it to each world's state, and
// WriteScriptDeclarations reads the declarations.
class ScriptApi
{
public:
    using Extend = function<void( LuaTypeBuilder<ScriptComponent>& type, World& world )>;
    using Add = function<void( LuaState& lua, World& world )>;

    struct Extension
    {
        Name mComponent;
        Extend mExtend;
        // Lines of the component's `declare extern type`: "    velocity: vector\n".
        string mDeclaration;
    };

    struct Globals
    {
        Add mAdd;
        // Whole declarations: "declare physics: { ... }\n".
        string mDeclaration;
    };

    struct Callback
    {
        string mName;
        // How a script writes it, for people: "on_collision( self, other: entity, point: vector, normal: vector )".
        string mSignature;
    };

    // Members for the Luau type of `component`, a component App describes.
    void ExtendComponent( Name component, Extend extend, string declaration );
    void AddGlobals( Add add, string declaration );
    void AddCallback( string name, string signature );

    span<const Extension> Extensions() const { return mExtensions; }
    span<const Globals> AllGlobals() const { return mGlobals; }
    span<const Callback> Callbacks() const { return mCallbacks; }

private:
    vector<Extension> mExtensions;
    vector<Globals> mGlobals;
    vector<Callback> mCallbacks;
};

// Every App module's, in App's order.
ScriptApi CollectScriptApi( const App& app );
}
