// The editor's Lua: the `editor` table over the operator registry.
#include "test.hpp"
#include "engine/editing/scripting/editor_lua.hpp"
#include "engine/editing/selection.hpp"
#include "engine/editing/clipboard.hpp"
#include "engine/serialization/types_serialization.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/scene/components/light_component.hpp"

TEST( EditorLua )
{
    OperatorRegistry::RegisterBuiltins();
    Fixture f;
    Selection selection;
    Clipboard clipboard;
    OperatorQueue queue;
    EditorLua lua( OperatorContext{ f.project, f.project.mLevel, f.history, selection, clipboard }, queue );

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
    CHECK( lua.Run( "assert( editor.ops.scene.create_node{ type = 'light', spawn_at = vec3( 4, 5, 6 ) } )" ).empty() );
    CHECK( f.Top().size() == 1 );
    const Entity light = f.Top()[0];
    CHECK( f.scene.GetComponent<TransformComponent>( light ).mPosition == vec3( 4, 5, 6 ) );
    CHECK( lua.Run( "assert( editor.selection()[1] == " + std::to_string( (u64)light ) + " )" ).empty() );

    // Components on the selection, and undo through the table
    CHECK( lua.Run( "editor.ops.entity.add_component{ component = 'state' }" ).empty() );
    CHECK( f.scene.HasComponent<StateComponent>( light ) );
    CHECK( lua.Run( "assert( editor.undo_name() == 'Add State' ); editor.undo()" ).empty() );
    CHECK( not f.scene.HasComponent<StateComponent>( light ) );
    // Undoing the create takes the selected entity away with it
    CHECK( lua.Run( "editor.undo(); assert( #editor.selection() == 0 )" ).empty() );
    CHECK( selection.IsEmpty() );
    CHECK( lua.Run( "editor.redo(); assert( #editor.selection() == 0 )" ).empty() );
    CHECK( f.scene.HasEntity( light ) );
    CHECK( lua.Run( "editor.select( " + std::to_string( (u64)light ) + " )" ).empty() );

    // Reading the document
    CHECK( lua.Run( std::format( "local t = editor.tree(); assert( t.entity == {} and t.folder and #t.children == 1 and t.children[1].entity == {} and t.children[1].name == 'Light' )",
                                 (u64)f.Root(), (u64)light ) ).empty() );
    CHECK( lua.Run( "assert( editor.entities_by_tag( 'Light' )[1] == " + std::to_string( (u64)light ) + " )" ).empty() );
    CHECK( lua.Run( "assert( #editor.operators() > 5 )" ).empty() );
    CHECK( lua.Run( "assert( editor.poll( 'scene.delete' ) )" ).empty() );

    // An operator's error is a Lua error, and it lands in the log
    lua.ClearLog();
    const string err = lua.Run( "editor.ops.entity.remove_component{ component = 'tag' }" );
    CHECK( not err.empty() and err.find( "Tag component cannot be removed" ) != string::npos );
    CHECK( lua.Log().size() == 1 and lua.Log()[0].starts_with( "error:" ) );

    // print goes to the log; enqueue waits for a flush
    lua.ClearLog();
    CHECK( lua.Run( "print( 'hello', 42 ); editor.enqueue( 'scene.delete' )" ).empty() );
    CHECK( lua.Log().size() == 1 and lua.Log()[0] == "hello	42" );
    CHECK( f.Top().size() == 1 );
    OperatorContext ctx{ f.project, f.project.mLevel, f.history, selection, clipboard };
    queue.Flush( ctx );
    CHECK( f.Top().empty() );
    CHECK( queue.Empty() );
}

TEST( EditorLua_Describe )
{
    OperatorRegistry::RegisterBuiltins();
    Fixture f;
    Selection selection;
    Clipboard clipboard;
    OperatorQueue queue;
    EditorLua lua( OperatorContext{ f.project, f.project.mLevel, f.history, selection, clipboard }, queue );
    sol::state& L = lua.State();

    // Arrays in order, maps by key, numbers as Lua shows them, a cycle named.
    CHECK( DescribeLuaValue( L.script( "return { 3, 1.5, 'x' }" ), 3, false ) == "{ 3, 1.5, \"x\" }" );
    CHECK( DescribeLuaValue( L.script( "return { b = 2, a = { 1, 2 } }" ), 3, false ) == "{ a = { 1, 2 }, b = 2 }" );
    CHECK( DescribeLuaValue( L.script( "local t = { n = 1 }; t.self = t; return t" ), 3, false ) == "{ n = 1, self = <cycle> }" );
    CHECK( DescribeLuaValue( L.script( "return { { { 1 } } }" ), 1, false ) == "{ { ...1 } }" );

    // An expression at the console shows its value; a statement just runs.
    lua.ClearLog();
    CHECK( lua.RunInteractive( "1 + 2" ).empty() );
    CHECK( lua.RunInteractive( "x = { 1 }" ).empty() );
    CHECK( lua.RunInteractive( "x" ).empty() );
    CHECK( lua.Log().size() == 2 and lua.Log()[0] == "3" and lua.Log()[1] == "{ 1 }" );
    // print opens tables too.
    lua.ClearLog();
    CHECK( lua.Run( "print( 'n', { 1, 2 } )" ).empty() );
    CHECK( lua.Log().size() == 1 and lua.Log()[0] == "n\t{ 1, 2 }" );
}

// property.set and editor.get: any field of a reflected component, by path.
TEST( EditorLua_Properties )
{
    OperatorRegistry::RegisterBuiltins();
    Fixture f;
    Selection selection;
    Clipboard clipboard;
    OperatorQueue queue;
    EditorLua lua( OperatorContext{ f.project, f.project.mLevel, f.history, selection, clipboard }, queue );

    CHECK( lua.Run( "editor.ops.scene.create_node{ type = 'light' }" ).empty() );
    const Entity lamp = f.Top()[0];
    const auto light = [&]() -> const LightComponent& { return f.scene.GetComponent<LightComponent>( lamp ); };

    // On the selection; an enum by name.
    CHECK( lua.Run( "editor.ops.property.set{ path = 'light.type', value = 'spot' }" ).empty() );
    CHECK( light().mType == LightType::Spot );
    // On an entity by path; derived state follows.
    const f32 linear = light().mLinear;
    CHECK( lua.Run( "editor.ops.property.set{ entity = 'Light', path = 'light.distance', value = 13 }" ).empty() );
    CHECK( light().mDistance == 13.0f and light().mLinear != linear );
    CHECK( lua.Run( "assert( editor.undo_name() == 'light.distance' )" ).empty() );
    CHECK( lua.Run( "editor.ops.property.set{ path = 'transform.position', value = vec3( 7, 8, 9 ) }" ).empty() );
    CHECK( f.scene.GetComponent<TransformComponent>( lamp ).mPosition == vec3( 7, 8, 9 ) );

    // Read back as property.set takes it.
    CHECK( lua.Run( "assert( editor.get( 'Light', 'light.type' ) == 'spot' )" ).empty() );
    CHECK( lua.Run( "local p = editor.get( 'Light', 'transform.position' ); assert( #p == 3 and p[3] == 9 )" ).empty() );

    // The value a field holds already makes no step; undo goes one back.
    CHECK( lua.Run( "editor.ops.property.set{ path = 'light.distance', value = 13 }" ).empty() );
    CHECK( f.history.NextUndoName() == "transform.position" );
    CHECK( lua.Run( "editor.undo()" ).empty() );
    CHECK( f.scene.GetComponent<TransformComponent>( lamp ).mPosition == vec3( 0 ) );

    // What is wrong, said.
    const auto fails = [&]( const string& code, string_view says )
    {
        const string error = lua.Run( code );
        return error.find( says ) != string::npos;
    };
    CHECK( fails( "editor.ops.property.set{ path = 'light.brigthness', value = 1 }", "has no field 'brigthness'" ) );
    CHECK( fails( "editor.ops.property.set{ path = 'camera.fov', value = 1 }", "has no camera component" ) );
    CHECK( fails( "editor.ops.property.set{ path = 'light.linear', value = 1 }", "read only" ) );
    CHECK( fails( "editor.ops.property.set{ path = 'light.type', value = 'cube' }", "has no value 'cube'" ) );
    CHECK( fails( "editor.ops.property.set{ path = 'brightness', value = 1 }", "is not Component.field" ) );
    CHECK( fails( "editor.ops.property.set{ path = 'Nope.x', value = 1 }", "Nope" ) );
    CHECK( lua.Run( "editor.ops.entity.add_component{ component = 'state' }" ).empty() );
    CHECK( fails( "editor.ops.property.set{ path = 'state.x', value = 1 }", "does not describe its fields yet" ) );
    CHECK( fails( "editor.get( 'Lamp', 'light.type' )", "nothing at 'Lamp'" ) );
}

// A name is unique among siblings; the field does not go around that.
TEST( EditorLua_NamesThroughRename )
{
    OperatorRegistry::RegisterBuiltins();
    Fixture f;
    Selection selection;
    Clipboard clipboard;
    OperatorQueue queue;
    EditorLua lua( OperatorContext{ f.project, f.project.mLevel, f.history, selection, clipboard }, queue );
    CHECK( lua.Run( "editor.ops.scene.create_node{ type = 'light' }" ).empty() );
    CHECK( lua.Run( "editor.ops.scene.create_node{ type = 'light' }" ).empty() );
    const string error = lua.Run( "editor.ops.property.set{ entity = 'Light2', path = 'tag.name', value = 'Light' }" );
    CHECK( error.find( "name is read only - renamed with scene.rename" ) != string::npos );
    // The operator for it keeps the names apart.
    CHECK( lua.Run( "editor.ops.scene.rename{ entity = 'Light2', name = 'Light' }" ).empty() );
    CHECK( lua.Run( "assert( editor.try_find( 'Light2' ) ~= nil )" ).empty() );
}
