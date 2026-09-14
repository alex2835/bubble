// A value inside a component's Lua table, addressed by key path.
#include "test.hpp"
#include "engine/editing/lua_value_command.hpp"
#include <sol/sol.hpp>

TEST( LuaValue )
{
    Fixture f;
    auto node = f.Create( ProjectTreeNodeType::Script );
    const Entity entity = node->AsEntity();
    const LuaTableRoot root = StateComponent::StateTableRoot( f.scene, entity );
    auto state = [&]() { return *root.Get(); };

    // Add a scalar at the top level
    f.history.Execute( CreateScope<SetLuaValueCommand>( root, LuaPath{ "hp"s }, Any( sol::lua_nil ), Any( 10 ) ) );
    CHECK( state()["hp"].get<int>() == 10 );
    CHECK( f.history.NextUndoName() == "State.hp" );
    f.history.Undo();
    CHECK( not state()["hp"].valid() );
    f.history.Redo();
    CHECK( state()["hp"].get<int>() == 10 );

    // A nested table, then a value inside it by path
    f.history.Execute( CreateScope<SetLuaValueCommand>( root, LuaPath{ "pos"s }, Any( sol::lua_nil ),
                                                        Any( f.project.mScriptingEngine.CreateTable() ) ) );
    f.history.Execute( CreateScope<SetLuaValueCommand>( root, LuaPath{ "pos"s, "x"s }, Any( sol::lua_nil ), Any( 1.5f ) ) );
    CHECK( state()["pos"]["x"].get<float>() == 1.5f );
    CHECK( f.history.NextUndoName() == "State.pos.x" );
    f.history.Execute( CreateScope<SetLuaValueCommand>( root, LuaPath{ "pos"s, "x"s }, Any( 1.5f ), Any( 2.5f ) ) );
    CHECK( state()["pos"]["x"].get<float>() == 2.5f );
    f.history.Undo();
    CHECK( state()["pos"]["x"].get<float>() == 1.5f );

    // Removing the subtable keeps it for undo, as a copy
    Table posBefore = state()["pos"];
    f.history.Execute( CreateScope<SetLuaValueCommand>( root, LuaPath{ "pos"s }, Any( posBefore ), Any( sol::lua_nil ) ) );
    CHECK( not state()["pos"].valid() );
    f.history.Undo();
    CHECK( state()["pos"]["x"].get<float>() == 1.5f );
    CHECK( state()["pos"].get<Table>() != posBefore ); // a copy, not the old reference

    // Array keys
    f.history.Execute( CreateScope<SetLuaValueCommand>( root, LuaPath{ "list"s }, Any( sol::lua_nil ),
                                                        Any( f.project.mScriptingEngine.CreateTable() ) ) );
    f.history.Execute( CreateScope<SetLuaValueCommand>( root, LuaPath{ "list"s, 1 }, Any( sol::lua_nil ), Any( "a"s ) ) );
    CHECK( state()["list"][1].get<string>() == "a" );
    CHECK( f.history.NextUndoName() == "State.list[1]" );

    // On a deleted entity the step is a no-op, and lands again once restored
    f.history.Execute( CreateScope<SetLuaValueCommand>( root, LuaPath{ "hp"s }, Any( 10 ), Any( 20 ) ) );
    f.history.Execute( CreateScope<DeleteNodeCommand>( node, f.scene ) );
    CHECK( not root.Get() );
    f.history.Undo(); // restore entity
    CHECK( state()["hp"].get<int>() == 20 );
    f.history.Undo(); // hp back to 10
    CHECK( state()["hp"].get<int>() == 10 );
}
