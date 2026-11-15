// The editor's Luau: a script edits a level with no window, through the same
// operators a menu runs, reads what it made, and gathers steps into one; an
// operator's error is a script error at the line that ran it.
#include "bubble/editing/editor_lua.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Light
{
    f32 mBrightness = 1;
    vec3 mTint = vec3( 1 );
    Color mColor;
    EntityPath mTarget;
};

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

struct Fixture
{
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    Operators mOperators;
    hmap<string, string> mFiles;
    AssetCatalog mAssets;
    Workspace mWorkspace{ mComponentTypes, ProjectFiles::InMemory( mFiles ), mAssets };
    vector<string> mPrinted;
    Scope<EditorLua> mEditor;

    Fixture()
    {
        mReflection.Type<Light>( "light" )
            .Field<&Light::mBrightness>( "brightness" )
            .Field<&Light::mTint>( "tint" )
            .Field<&Light::mColor>( "color" )
            .Field<&Light::mTarget>( "target" );
        mComponentTypes.Add<Light>();
        AddBuiltinOperators( mOperators );
        mEditor = CreateScope<EditorLua>( mWorkspace, mOperators,
                                          [this]( string_view text ) { mPrinted.push_back( string( text ) ); } );
    }

    void Run( string_view source )
    {
        const auto ranMaybe = mEditor->TryRun( source, "macro.luau" );
        REQUIRE_MESSAGE( ranMaybe, ranMaybe.error() );
    }

    string Why( string_view source )
    {
        const auto ranMaybe = mEditor->TryRun( source, "macro.luau" );
        REQUIRE_FALSE( ranMaybe );
        return ranMaybe.error();
    }

    Document& Active() { return *mWorkspace.TryActive(); }

    Entity Find( string_view path )
    {
        const auto foundMaybe = TryResolve( Active().GetScene(), EntityPath{ Make( path ) } );
        REQUIRE_MESSAGE( foundMaybe, foundMaybe.error() );
        return *foundMaybe;
    }

    // A level with /props/lamp, made by a script.
    void Furnish()
    {
        Run( R"(--!strict
editor.ops.document.new()
editor.ops.entity.create( { name = "lamp" } )
editor.ops.component.add( { component = "light" } )
editor.ops.property.set( { component = "light", path = "brightness", value = 3 } )
editor.ops.property.set( { component = "light", path = "tint", value = vector.create( 1, 0.5, 0 ) } )
editor.ops.property.set( { component = "light", path = "color", value = { 1, 0, 0 } } )
editor.ops.entity.create( { name = "props", parent = "/" } )
editor.ops.property.set( { entities = { "/lamp" }, component = "light", path = "target", value = "/props" } )
editor.ops.entity.move( { entities = { "/lamp" }, parent = "/props" } )
)" );
    }
};
}

TEST_CASE( "A script edits a level with no window, one step an operator, and saves it" )
{
    Fixture f;
    f.Furnish();
    const Light& light = f.Active().GetScene().Get<Light>( f.Find( "/props/lamp" ) );
    CHECK( light.mBrightness == 3.0f );
    CHECK( light.mTint == vec3( 1, 0.5f, 0 ) );
    CHECK( light.mColor == Color{ 1, 0, 0, 1 } );
    CHECK( light.mTarget.mPath.View() == "/props" );
    CHECK( f.Active().GetHistory().UndoLabel() == "Move" );

    f.Run( R"(editor.ops.document.save( { path = "levels/room.scene" } ))" );
    CHECK( f.mFiles.contains( "levels/room.scene" ) );
    CHECK_FALSE( f.Active().IsDirty() );

    f.Run( "editor.ops.edit.undo()" );
    CHECK( f.Find( "/lamp" ) != Entity::Null );
    CHECK( f.Active().IsDirty() );
}

TEST_CASE( "A script reads fields, whole components, children and the selection" )
{
    Fixture f;
    f.Furnish();
    f.Run( R"(--!strict
print( editor.get( "/props/lamp", "light.brightness" ) )
local light = editor.get( "/props/lamp", "light" )
print( light.brightness, light.target, light.color[1], light.tint.y )
print( #editor.children( "/props" ), editor.children( "/props" )[1] )
print( editor.components( "/props/lamp" )[1], #editor.components( "/props" ) )
print( editor.exists( "/props/lamp" ), editor.exists( "/lamp" ) )
editor.ops.selection.set( { entities = { "/props", "/props/lamp" } } )
print( editor.selection()[2] )
)" );
    CHECK( f.mPrinted ==
           vector<string>{ "3", "3\t/props\t1\t0.5", "1\t/props/lamp", "light\t0", "true\tfalse", "/props/lamp" } );
}

TEST_CASE( "An operator's error is a script error at the line that ran it, and says what there is" )
{
    Fixture f;
    f.Furnish();
    CHECK( f.Why( "\neditor.ops.entity.rename( { name = 3 } )" ) ==
           "macro.luau:2: entity.rename.name: text expected, got 3" );
    CHECK( f.Why( R"(editor.run( "entity.rename", { entity = "/props/lamp" } ))" ) ==
           "macro.luau:1: entity.rename: name is missing (arguments: entity?, name)" );
    CHECK( f.Why( "editor.ops.entity.vanish()" )
               .starts_with( "macro.luau:1: entity.vanish is not an operator (entity has: " ) );
    CHECK( f.Why( "editor.ops.lights.on()" ) ==
           "macro.luau:1: no operators are called lights.* (there are: component, document, edit, entity, prefab, "
           "property, selection)" );
    CHECK( f.Why( R"(editor.get( "/props/lamp", "light.glow" ))" ).starts_with( "macro.luau:1: /props/lamp: " ) );
    CHECK( f.Why( R"(editor.get( "/props/lamp", "shadow" ))" ) ==
           "macro.luau:1: /props/lamp: shadow is not a component (components: light)" );
    CHECK( f.Why( "editor.ops.entity.create( " ).starts_with( "macro.luau:1" ) );
    // The editor's table is the editor's.
    CHECK( f.Why( "editor.run = nil" ).starts_with( "macro.luau:1: " ) );
}

TEST_CASE( "A group of operators is one step, and an error inside takes the whole of it back" )
{
    Fixture f;
    f.Furnish();
    f.Run( R"(--!strict
editor.group( "Furnish", function()
    editor.ops.entity.create( { name = "chair", parent = "/props" } )
    editor.ops.entity.create( { name = "table", parent = "/props" } )
end )
)" );
    CHECK( f.Active().GetHistory().UndoLabel() == "Furnish" );
    CHECK( ChildrenOf( f.Active().GetScene(), f.Find( "/props" ) ).size() == 3 );
    f.Run( "editor.ops.edit.undo()" );
    CHECK( ChildrenOf( f.Active().GetScene(), f.Find( "/props" ) ).size() == 1 );

    const u64 before = f.Active().GetHistory().Position();
    CHECK( f.Why( R"(--!strict
editor.group( "Broken", function()
    editor.ops.entity.create( { name = "stool", parent = "/props" } )
    editor.ops.entity.rename( { entity = "/props/nowhere", name = "x" } )
end )
)" )
               .find( "macro.luau:4: entity.rename: entity: /props/nowhere: " ) != string::npos );
    CHECK( f.Active().GetHistory().Position() == before );
    CHECK_FALSE( f.Active().GetHistory().InGroup() );
    CHECK( ChildrenOf( f.Active().GetScene(), f.Find( "/props" ) ).size() == 1 );
}

TEST_CASE( "A script asks whether an operator can run, and each run has globals of its own" )
{
    Fixture f;
    f.Furnish();
    f.Run( R"(--!strict
editor.ops.selection.clear()
print( editor.poll( "entity.destroy" ) )
editor.ops.selection.set( { entities = { "/props" } } )
print( editor.poll( "entity.destroy" ) )
counter = 1
)" );
    f.Run( "print( counter )" );
    CHECK( f.mPrinted == vector<string>{ "false\tentity.destroy: nothing selected", "true\tnil", "nil" } );

    hmap<string, string> none;
    Workspace empty( f.mComponentTypes, ProjectFiles::InMemory( none ), f.mAssets );
    EditorLua editor( empty, f.mOperators );
    CHECK( editor.TryRun( R"(editor.get( "/", "light" ))", "macro.luau" ).error() ==
           "macro.luau:1: no document is open" );
}

TEST_CASE( "A script edits a level and a prefab, and the prefab's change reaches its instances, theirs kept" )
{
    Fixture f;
    f.Run( R"(--!strict
editor.ops.document.new()
editor.ops.entity.create( { name = "house" } )
editor.ops.entity.create( { name = "door", parent = "/house" } )
editor.ops.component.add( { entities = { "/house/door" }, component = "light" } )
editor.ops.prefab.create( { entity = "/house", path = "props/house.prefab" } )
editor.ops.prefab.instantiate( { prefab = "props/house.prefab", parent = "/", name = "barn" } )
editor.ops.property.set( { entities = { "/barn/door" }, component = "light", path = "brightness", value = 5 } )
editor.ops.document.save( { path = "levels/street.scene" } )

editor.ops.document.open( { path = "props/house.prefab" } )
editor.ops.property.set( { entities = { "/door" }, component = "light", path = "tint", value = vector.create( 0, 1, 0 ) } )
editor.ops.entity.create( { name = "chimney", parent = "/" } )
editor.ops.document.save()
editor.ops.document.close()

print( editor.get( "/barn/door", "light.brightness" ), editor.get( "/house/door", "light.brightness" ) )
print( editor.get( "/barn/door", "light.tint" ).y, editor.exists( "/barn/chimney" ), editor.exists( "/house/chimney" ) )
)" );
    CHECK( f.mPrinted == vector<string>{ "5\t1", "1\ttrue\ttrue" } );
}
