// The editor's Lua: the `editor` table over the operator registry.
#include "test.hpp"
#include "engine/editing/editor_lua.hpp"
#include "engine/editing/selection.hpp"
#include "engine/editing/clipboard.hpp"
#include "engine/serialization/types_serialization.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

TEST( EditorLua )
{
    OperatorRegistry::RegisterBuiltins();
    Fixture f;
    Selection selection;
    Clipboard clipboard;
    OperatorQueue queue;
    EditorLua lua( OperatorContext{ f.project, f.history, selection, clipboard }, queue );

    // Conversions both ways
    {
        sol::state& L = lua.State();
        const json j = LuaToJson( L.script( "return { type = 'Light', spawn_at = vec3( 1, 2, 3 ), list = { 1, 2.5, 'x' }, on = true }" ) );
        CHECK( j["type"] == "Light" );
        CHECK( j["spawn_at"] == json( { 1.0, 2.0, 3.0 } ) );
        CHECK( j["list"] == json( { 1, 2.5, "x" } ) );
        CHECK( j["list"][0].is_number_integer() );
        CHECK( j["on"] == true );
        const sol::object back = JsonToLua( L, json{ { "a", 1 }, { "b", { 1, 2 } }, { "c", "s" } } );
        const sol::table t = back.as<sol::table>();
        CHECK( t["a"].get<int>() == 1 and t["b"][2].get<int>() == 2 and t["c"].get<string>() == "s" );
    }

    // The sugar form, with a vec3 argument; the result is selected
    CHECK( lua.Run( "assert( editor.ops.scene.create_node{ type = 'Light', spawn_at = vec3( 4, 5, 6 ) } )" ).empty() );
    CHECK( f.root->mChildren.size() == 1 );
    const Entity light = f.root->mChildren[0]->AsEntity();
    CHECK( f.scene.GetComponent<TransformComponent>( light ).mPosition == vec3( 4, 5, 6 ) );
    CHECK( lua.Run( "assert( editor.selection()[1] == " + std::to_string( (u64)light ) + " )" ).empty() );

    // Components on the selection, and undo through the table
    CHECK( lua.Run( "editor.ops.entity.add_component{ component = 'State' }" ).empty() );
    CHECK( f.scene.HasComponent<StateComponent>( light ) );
    CHECK( lua.Run( "assert( editor.undo_name() == 'Add State' ); editor.undo()" ).empty() );
    CHECK( not f.scene.HasComponent<StateComponent>( light ) );
    // Undoing the create takes the selected entity away with it
    CHECK( lua.Run( "editor.undo(); assert( #editor.selection() == 0 )" ).empty() );
    CHECK( selection.IsEmpty() and not selection.GetTreeNode() );
    CHECK( lua.Run( "editor.redo(); assert( #editor.selection() == 0 )" ).empty() );
    CHECK( f.scene.HasEntity( light ) );
    CHECK( lua.Run( "editor.select( " + std::to_string( (u64)light ) + " )" ).empty() );

    // Reading the document
    CHECK( f.root->ID() == 0 and f.root->mChildren[0]->ID() == 1 ); // ids are handed out from the level's counter
    CHECK( lua.Run( "local t = editor.tree(); assert( t.type == 'Root' and #t.children == 1 and t.children[1].type == 'Light' )" ).empty() );
    CHECK( lua.Run( "assert( editor.entities_by_tag( 'Light' )[1] == " + std::to_string( (u64)light ) + " )" ).empty() );
    CHECK( lua.Run( "assert( #editor.operators() > 5 )" ).empty() );
    CHECK( lua.Run( "assert( editor.poll( 'scene.delete' ) )" ).empty() );

    // An operator's error is a Lua error, and it lands in the log
    lua.ClearLog();
    const string err = lua.Run( "editor.ops.entity.remove_component{ component = 'Tag' }" );
    CHECK( not err.empty() and err.find( "Tag component cannot be removed" ) != string::npos );
    CHECK( lua.Log().size() == 1 and lua.Log()[0].starts_with( "error:" ) );

    // print goes to the log; enqueue waits for a flush
    lua.ClearLog();
    CHECK( lua.Run( "print( 'hello', 42 ); editor.enqueue( 'scene.delete' )" ).empty() );
    CHECK( lua.Log().size() == 1 and lua.Log()[0] == "hello	42" );
    CHECK( f.root->mChildren.size() == 1 );
    OperatorContext ctx{ f.project, f.history, selection, clipboard };
    queue.Flush( ctx );
    CHECK( f.root->mChildren.empty() );
    CHECK( queue.Empty() );
}
