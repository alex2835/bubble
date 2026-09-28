#include "test.hpp"
#include "engine/project/prefab.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/scene/node_path.hpp"
#include "engine/editing/selection.hpp"
#include "engine/editing/clipboard.hpp"
#include "engine/editing/operators/operator.hpp"
#include "engine/scene/components/prefab_instance_component.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/serialization/types_serialization.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <fstream>

// A prefab is saved from a node, instantiated, changed and its instances
// updated - all against a project in a scratch directory.

namespace
{
bool Near( const vec3& a, const vec3& b ) { return glm::length( a - b ) < 1e-4f; }

struct PrefabFixture : Fixture
{
    path mDir;
    Selection mSelection;
    Clipboard mClipboard;

    PrefabFixture()
    {
        mDir = filesystem::temp_directory_path() / std::format( "bubble_prefab_test_{}", (u64)std::chrono::steady_clock::now().time_since_epoch().count() );
        filesystem::create_directories( mDir );
        project.mRootFile = mDir / "test.bubble";
    }
    ~PrefabFixture() { std::error_code ec; filesystem::remove_all( mDir, ec ); }

    OperatorContext Ctx() { return OperatorContext{ project, project.mLevel, history, mSelection, mClipboard }; }
};
}

TEST( Prefab_SaveInstantiateUpdate )
{
    OperatorRegistry::RegisterBuiltins();
    PrefabFixture f;
    // A body with a part under it; the body placed away from the origin.
    const Entity body = f.Create( EntityKind::ModelObject );
    const Entity part = f.Create( EntityKind::Light, body );
    f.scene.GetComponent<TagComponent>( body ).mName = "body";
    f.scene.GetComponent<TransformComponent>( part ).mPosition = vec3( 0, 2, 0 );
    // The body's state names the part, which has to lead to the copy's.
    f.scene.AddComponent<StateComponent>( body, f.project.mScriptingEngine.CreateTable() );
    f.scene.GetComponent<StateComponent>( body ).mState->as<Table>()["part"] = NodePath( NameOf( f.scene, part ) );

    auto ctx = f.Ctx();
    CHECK( InvokeOperator( "prefab.save", ctx, { { "file", "prefabs/thing" }, { "entity", (u64)body } } ) );
    CHECK( filesystem::is_regular_file( f.mDir / "prefabs/thing.prefab" ) );

    CHECK( InvokeOperator( "prefab.instantiate", ctx, { { "file", "prefabs/thing.prefab" }, { "spawn_at", vec3( 10, 0, 0 ) } } ) );
    CHECK( f.mSelection.IsSingleSelection() );
    if ( not f.mSelection.IsSingleSelection() )
        return;
    const Entity root = f.mSelection.GetSingleEntity();
    // The prefab's root copied, linked to the file, and named after it.
    CHECK( f.scene.GetComponent<TagComponent>( root ).mName == "thing" );
    CHECK( f.scene.GetComponent<PrefabInstanceComponent>( root ).mPrefab == "prefabs/thing.prefab" );
    CHECK( ParentOf( f.scene, root ) == f.Root() );
    const auto children = f.Children( root );
    CHECK( children.size() == 1 );
    if ( children.empty() )
        return;
    const Entity copiedPart = children[0];
    UpdateWorldTransforms( f.scene );
    CHECK( Near( f.scene.GetComponent<TransformComponent>( copiedPart ).World().mPosition, vec3( 10, 2, 0 ) ) );
    // The reference in the state is a path, so it leads to the copy.
    const sol::object named = f.scene.GetComponent<StateComponent>( root ).mState->as<Table>()["part"];
    CHECK( named.is<NodePath>() and FindByPath( f.scene, root, named.as<NodePath>().mPath ) == copiedPart );

    // Undo takes the instance out, redo brings it back under the same ids.
    f.history.Undo();
    CHECK( not f.scene.HasEntity( root ) );
    f.history.Redo();
    CHECK( f.scene.HasEntity( root ) and ParentOf( f.scene, copiedPart ) == root );

    // The instance is moved; then the prefab changes; an update keeps the
    // placement and the root's id, and takes the new content.
    f.scene.GetComponent<TransformComponent>( root ).mPosition = vec3( 20, 0, 0 );
    f.scene.GetComponent<TransformComponent>( part ).mPosition = vec3( 0, 5, 0 );
    f.scene.GetComponent<TransformComponent>( body ).mScale = vec3( 2 );
    CHECK( InvokeOperator( "prefab.save", ctx, { { "file", "prefabs/thing.prefab" }, { "entity", (u64)body } } ) );
    CHECK( InvokeOperator( "prefab.update_instances", ctx, { { "file", "prefabs/thing.prefab" } } ) );
    CHECK( f.scene.HasEntity( root ) and ParentOf( f.scene, root ) == f.Root() );
    // Where it was is the instance's; the scale is the prefab's.
    CHECK( Near( f.scene.GetComponent<TransformComponent>( root ).mPosition, vec3( 20, 0, 0 ) ) );
    CHECK( Near( f.scene.GetComponent<TransformComponent>( root ).mScale, vec3( 2 ) ) );
    const auto refreshed = f.Children( root );
    CHECK( refreshed.size() == 1 );
    if ( not refreshed.empty() )
    {
        UpdateWorldTransforms( f.scene );
        CHECK( Near( f.scene.GetComponent<TransformComponent>( refreshed[0] ).World().mPosition, vec3( 20, 10, 0 ) ) );
    }
    // One step to undo: the old content back.
    f.history.Undo();
    CHECK( f.scene.HasEntity( copiedPart ) and ParentOf( f.scene, copiedPart ) == root );

    // A prefab file is a level whose root is the prefab; and it cannot hold
    // itself.
    Level opened;
    opened.Load( f.mDir / "prefabs/thing.prefab", f.project );
    CHECK( opened.mScene.GetComponent<TagComponent>( opened.Root() ).mName == "thing" );
    CHECK( ChildrenOf( opened.mScene, opened.Root() ).size() == 1 );
    OperatorContext inPrefab{ f.project, opened, f.history, f.mSelection, f.mClipboard };
    bool refused = false;
    try { InvokeOperator( "prefab.instantiate", inPrefab, { { "file", "prefabs/thing.prefab" } } ); }
    catch ( const std::exception& ) { refused = true; }
    CHECK( refused );
}

TEST( Prefab_SpawnAtRunTime )
{
    PrefabFixture f;
    const Entity folder = f.Create( EntityKind::Folder );
    const Entity a = f.Create( EntityKind::ModelObject, folder );
    const Entity b = f.Create( EntityKind::ModelObject, folder );
    f.scene.GetComponent<TransformComponent>( b ).mPosition = vec3( 4, 2, 3 );
    SavePrefab( f.scene, folder, f.mDir / "two.prefab", f.project );

    // Spawned under the running level's root, placed where asked.
    Level running;
    const auto spawned = SpawnPrefab( f.project, running.mScene, "two.prefab", vec3( 0, 100, 0 ) );
    CHECK( spawned.size() == 3 );
    const Entity root = spawned.front();
    CHECK( ParentOf( running.mScene, root ) == running.Root() );
    CHECK( running.mScene.HasComponent<PrefabInstanceComponent>( root ) );
    const auto children = ChildrenOf( running.mScene, root );
    CHECK( children.size() == 2 );
    if ( children.size() == 2 )
    {
        CHECK( Near( running.mScene.GetComponent<TransformComponent>( children[0] ).World().mPosition, vec3( 1, 102, 3 ) ) );
        CHECK( Near( running.mScene.GetComponent<TransformComponent>( children[1] ).World().mPosition, vec3( 4, 102, 3 ) ) );
    }
    (void)a;
}
