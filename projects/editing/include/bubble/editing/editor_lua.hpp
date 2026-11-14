#pragma once
#include "bubble/editing/operators.hpp"
#include "bubble/scripts/lua.hpp"

namespace bubble
{
// The editor's Luau: a VM of its own - never a world's - whose `editor`
// table runs the operators and reads the active document. What a menu does,
// a script does, with no window:
//
//     editor.ops.entity.create( { name = "lamp" } )
//     editor.ops.property.set( { component = "light", path = "brightness", value = 3 } )
//     print( editor.get( "/lamp", "light.brightness" ) )      --> 3
//     editor.group( "Furnish", function() ... end )           -- one step of the history
//
// Also editor.run( id, args ), editor.poll( id, args ) -> ok, why,
// editor.children( path ), editor.components( path ), editor.exists( path )
// and editor.selection(). Arguments are read by what the operator declares;
// an operator's error is a script error at the line that ran it.
class EditorLua
{
public:
    EditorLua( Workspace& workspace, const Operators& operators, LuaState::PrintHandler print = PrintToLog );
    EditorLua( const EditorLua& ) = delete;
    EditorLua& operator=( const EditorLua& ) = delete;

    // Compiles and runs `source`, named `name` in errors:
    // "macro.luau:3: entity.rename: name is missing (arguments: entity?, name)".
    // Each run has globals of its own over the editor's.
    expected<void, string> TryRun( string_view source, string_view name );

    LuaState& State() { return mLua; }

private:
    void Bind();
    void BindOperators( LuaTable& editor );
    void BindReads( LuaTable& editor );

    Document& Active() const;
    Entity Resolve( const Document& document, const string& path ) const;
    Value::Record ArgsFor( string_view id, const LuaValue& args );

    // First: every LuaValue below dies before it.
    LuaState mLua;
    Workspace& mWorkspace;
    const Operators& mOperators;
    LuaTable mRunEnvironment;
};
}
