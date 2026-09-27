// Names unique among siblings, paths of names, and NodePath references in
// State tables.
#include "test.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/editing/operators/operator.hpp"
#include "engine/editing/selection.hpp"
#include "engine/editing/clipboard.hpp"
#include "engine/project/prefab.hpp"
#include "engine/scene/node_path.hpp"
#include "engine/serialization/any_serialization.hpp"
#include "engine/scene/components/folder_component.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace
{
string Name( const Fixture& f, Entity e ) { return NameOf( f.scene, e ); }
}

TEST( Names_UniqueAmongSiblings )
{
    Fixture f;
    const Entity a = f.Create( EntityKind::Light );
    const Entity b = f.Create( EntityKind::Light );
    CHECK( Name( f, a ) == "Light" and Name( f, b ) == "Light2" );

    // A copy next to them counts on.
    f.history.Execute( CreateScope<CopyEntityCommand>( f.scene, a, f.Root() ) );
    CHECK( Name( f, f.Top().back() ) == "Light3" );

    // In another parent the name is free; moved back out, it is taken again,
    // and undo gives the old one back.
    const Entity folder = f.Create( EntityKind::Folder );
    const Entity inside = f.Create( EntityKind::Light, folder );
    CHECK( Name( f, inside ) == "Light" );
    f.history.Execute( CreateScope<MoveEntityCommand>( f.scene, inside, f.Root() ) );
    CHECK( Name( f, inside ) == "Light4" );
    f.history.Undo();
    CHECK( Name( f, inside ) == "Light" and ParentOf( f.scene, inside ) == folder );

    // A rename to a sibling's name gets a number past it; to its own,
    // nothing - and "Light" for Light2 is Light2 again.
    CHECK( MakeRenameCommand( f.scene, b, "Light" ) == nullptr );
    auto rename = MakeRenameCommand( f.scene, b, "Light3" );
    CHECK( rename != nullptr );
    f.history.Execute( std::move( rename ) );
    CHECK( Name( f, b ) == "Light4" );
    CHECK( MakeRenameCommand( f.scene, b, "Light4" ) == nullptr );
    // Slashes cannot be in a name; nothing is not a name.
    CHECK( UniqueChildName( f.scene, f.Root(), "a/b" ) == "ab" );
    CHECK( UniqueChildName( f.scene, f.Root(), "" ) == "Entity" );
    CHECK( UniqueChildName( f.scene, f.Root(), ".." ) == "Entity" );
}

TEST( Names_DuplicatesInAFileAreNumbered )
{
    Fixture f;
    const Entity a = f.Create( EntityKind::Light );
    const Entity b = f.Create( EntityKind::Light );
    const Entity c = f.Create( EntityKind::Light );
    // As a file from before could have them.
    f.scene.GetComponent<TagComponent>( a ).mName = "floor";
    f.scene.GetComponent<TagComponent>( b ).mName = "floor";
    f.scene.GetComponent<TagComponent>( c ).mName = "floor2";

    Level loaded;
    loaded.FromJson( f.project.mLevel.ToJson( f.project ), f.project );
    const auto top = ChildrenOf( loaded.mScene, loaded.Root() );
    CHECK( top.size() == 3 );
    if ( top.size() == 3 )
    {
        // The first keeps its name; the second skips the name a later one has.
        CHECK( NameOf( loaded.mScene, top[0] ) == "floor" );
        CHECK( NameOf( loaded.mScene, top[1] ) == "floor3" );
        CHECK( NameOf( loaded.mScene, top[2] ) == "floor2" );
    }
}

TEST( Paths_FindAndBack )
{
    Fixture f;
    const Entity props = f.Create( EntityKind::Folder );
    f.scene.GetComponent<TagComponent>( props ).mName = "props";
    const Entity chair = f.Create( EntityKind::ModelObject, props );
    f.scene.GetComponent<TagComponent>( chair ).mName = "chair";
    const Entity leg = f.Create( EntityKind::Light, chair );
    const Entity door = f.Create( EntityKind::Camera, props );
    f.scene.GetComponent<TagComponent>( door ).mName = "door";

    CHECK( FindByPath( f.scene, f.Root(), "props/chair" ) == chair );
    CHECK( FindByPath( f.scene, leg, "/props/door" ) == door );
    CHECK( FindByPath( f.scene, chair, "../door" ) == door );
    CHECK( FindByPath( f.scene, chair, "Light" ) == leg );
    CHECK( FindByPath( f.scene, chair, "." ) == chair );
    CHECK( FindByPath( f.scene, chair, "/" ) == f.Root() );
    CHECK( FindByPath( f.scene, chair, "nothing" ) == INVALID_ENTITY );
    CHECK( FindByPath( f.scene, f.Root(), ".." ) == INVALID_ENTITY );

    CHECK( PathOf( f.scene, leg ) == "/props/chair/Light" );
    CHECK( PathOf( f.scene, f.Root() ) == "/" );
    CHECK( RelativePath( f.scene, chair, door ) == "../door" );
    CHECK( RelativePath( f.scene, props, leg ) == "chair/Light" );
    CHECK( RelativePath( f.scene, leg, leg ) == "." );
    // Every relative path leads back to where it was made for.
    for ( const Entity from : { f.Root(), props, chair, leg, door } )
        for ( const Entity to : { f.Root(), props, chair, leg, door } )
            CHECK( FindByPath( f.scene, from, RelativePath( f.scene, from, to ) ) == to );

    // Operators take a path wherever they take an entity id.
    OperatorRegistry::RegisterBuiltins();
    Selection selection;
    Clipboard clipboard;
    OperatorContext ctx{ f.project, f.project.mLevel, f.history, selection, clipboard };
    CHECK( InvokeOperator( "scene.rename", ctx, { { "entity", "props/door" }, { "name", "chair" } } ) );
    CHECK( NameOf( f.scene, door ) == "chair2" );
}

TEST( Paths_FromScripts )
{
    Fixture f;
    PhysicsEngine physics;
    f.project.mScriptingEngine.BindLoader( f.project.mLoader );
    f.project.mScriptingEngine.BindScene( f.scene, physics );
    const Entity props = f.Create( EntityKind::Folder );
    f.scene.GetComponent<TagComponent>( props ).mName = "props";
    const Entity chair = f.Create( EntityKind::ModelObject, props );
    f.scene.GetComponent<TagComponent>( chair ).mName = "chair";

    sol::state& lua = *f.project.mScriptingEngine.mLua;
    lua.script( R"(
        chair = level:find( "props/chair" )
        props = chair:find( ".." )
        missing = level:find( "props/table" )
        path = chair:get_path()
        -- Two made with the same name are told apart.
        e1 = create_entity(); e1:add_tag( "crate" )
        e2 = create_entity(); e2:add_tag( "crate" )
        second = e2.name
        e2.name = "chair"; e2:set_parent( props )
        moved = e2.name
        -- A missing component is reported by the entity's path.
        ok, err = pcall( function() return chair:get_camera() end )
    )" );
    CHECK( lua["err"].get<string>().contains( "'/props/chair' has no Camera component" ) );
    CHECK( lua["chair"].get<Entity>() == chair );
    CHECK( lua["props"].get<Entity>() == props );
    CHECK( not lua["missing"].valid() );
    CHECK( lua["path"].get<string>() == "/props/chair" );
    CHECK( lua["second"].get<string>() == "crate2" );
    CHECK( lua["moved"].get<string>() == "chair2" );
}

TEST( NodePath_SavedAndResolved )
{
    Fixture f;
    PhysicsEngine physics;
    f.project.mScriptingEngine.BindLoader( f.project.mLoader );
    f.project.mScriptingEngine.BindScene( f.scene, physics );
    const Entity body = f.Create( EntityKind::Script );
    const Entity part = f.Create( EntityKind::Light, body );
    const Entity lamp = f.Create( EntityKind::Light );
    f.scene.GetComponent<TagComponent>( part ).mName = "part";

    Table state = f.scene.GetComponent<StateComponent>( body ).mState->as<Table>();
    state["part"] = NodePath( "part" );
    state["lamp"] = NodePath( "../Light" );
    state["gone"] = NodePath( "nothing" );
    Table nested = f.project.mScriptingEngine.CreateTable();
    nested["again"] = NodePath( "part" );
    state["nested"] = nested;

    // Kept as a path in a file.
    const json saved = SaveAnyValue( state );
    CHECK( saved["part"]["__type"] == "NodePath" and saved["part"]["path"] == "part" );
    const Any loaded = LoadAnyValue( f.project.mScriptingEngine, saved );
    CHECK( loaded.as<Table>()["lamp"].get<NodePath>().mPath == "../Light" );

    // Turned into entities, as when the game starts.
    ResolveAllNodePaths( f.scene );
    CHECK( state["part"].get<Entity>() == part );
    CHECK( state["lamp"].get<Entity>() == lamp );
    CHECK( not state["gone"].valid() );
    CHECK( state["nested"]["again"].get<Entity>() == part );
}

TEST( NodePath_InsidePrefabInstances )
{
    Fixture f;
    const path dir = filesystem::temp_directory_path() / "bubble_nodepath_test";
    filesystem::remove_all( dir );
    filesystem::create_directories( dir );
    f.project.mRootFile = dir / "p.bubble";

    const Entity body = f.Create( EntityKind::Script );
    const Entity part = f.Create( EntityKind::Light, body );
    f.scene.GetComponent<TagComponent>( part ).mName = "part";
    f.scene.GetComponent<StateComponent>( body ).mState->as<Table>()["part"] = NodePath( "part" );
    SavePrefab( f.scene, body, dir / "thing.prefab", f.project );

    // Two instances, each finds its own part.
    Level level;
    const auto one = SpawnPrefab( f.project, level.mScene, "thing.prefab", vec3( 0 ) );
    const auto two = SpawnPrefab( f.project, level.mScene, "thing.prefab", vec3( 5, 0, 0 ) );
    CHECK( NameOf( level.mScene, one.front() ) != NameOf( level.mScene, two.front() ) );
    ResolveAllNodePaths( level.mScene );
    for ( const auto& spawned : { one, two } )
    {
        const Entity root = spawned.front();
        const auto children = ChildrenOf( level.mScene, root );
        const Any& st = *level.mScene.GetComponent<StateComponent>( root ).mState;
        CHECK( children.size() == 1 and st.as<Table>()["part"].get<Entity>() == children[0] );
    }
    filesystem::remove_all( dir );
}
