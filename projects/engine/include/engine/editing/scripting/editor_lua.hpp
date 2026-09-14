#pragma once
#include <sol/forward.hpp>
#include "engine/editing/operators/operator.hpp"
#include "engine/editing/operators/operator_queue.hpp"
#include "engine/types/pointer.hpp"
#include "engine/utils/filesystem.hpp"

// The editor's scripting: a Lua state of its own - never the game's - whose
// `editor` table is the operator registry and the selection. What a menu
// item does, a script can do:
//
//     editor.ops.scene.create_node{ type = "Light", spawn_at = vec3( 0, 5, 0 ) }
//     editor.ops.entity.add_component{ component = "State" }
//     editor.undo()
//
// Arguments cross as a table turned into the operator's JSON: numbers,
// strings, booleans, nested tables (an array when its keys are 1..n) and the
// vec2/vec3/vec4 usertypes. Anything an operator throws is a Lua error with
// the operator's message.
namespace bubble
{
class OperatorQueue;

class EditorLua
{
public:
    EditorLua( OperatorContext ctx, OperatorQueue& queue );
    ~EditorLua();

    // Runs a chunk. Output of print() and an error, if any, go to the log
    // returned by Log(); the error is also returned. Empty means it ran.
    string Run( string_view code, string_view chunkName = "console" );
    string RunFile( const path& file );

    // What scripts printed, oldest first. A console echoes what it ran
    // into the same log through Print.
    const vector<string>& Log() const { return mLog; }
    void ClearLog() { mLog.clear(); }
    void Print( string line );

    sol::state& State() { return *mLua; }

private:
    void Bind();

    OperatorContext mCtx;
    OperatorQueue& mQueue;
    Scope<sol::state> mLua;
    vector<string> mLog;
};

// The conversions the `editor` table is built on, on their own for tests
// and for any other binding that takes a JSON argument.
json LuaToJson( const sol::object& value );
sol::object JsonToLua( sol::state_view lua, const json& value );

}
