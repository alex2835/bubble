#include "test.hpp"
#include "engine/project/prefab.hpp"
#include "engine/scene/hierarchy.hpp"
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
    auto body = f.Create( ProjectTreeNodeType::ModelObject );
    auto part = f.Create( ProjectTreeNodeType::Light );
    f.history.Execute( CreateScope<MoveNodeCommand>( part, body, f.scene ) );
    f.scene.GetComponent<TagComponent>( body->AsEntity() ).mName = "body";
    f.scene.GetComponent<TransformComponent>( part->AsEntity() ).mPosition = vec3( 0, 2, 0 );
    // The body's state names the part, which has to be carried over.
    f.scene.AddComponent<StateComponent>( body->AsEntity(), f.project.mScriptingEngine.CreateTable() );
    f.scene.GetComponent<StateComponent>( body->AsEntity() ).mState->as<Table>()["part"] = part->AsEntity();

    auto ctx = f.Ctx();
    CHECK( InvokeOperator( "prefab.save", ctx, { { "file", "prefabs/thing" }, { "node", body->ID() } } ) );
    CHECK( filesystem::is_regular_file( f.mDir / "prefabs/thing.prefab" ) );

    CHECK( InvokeOperator( "prefab.instantiate", ctx, { { "file", "prefabs/thing.prefab" }, { "spawn_at", vec3( 10, 0, 0 ) } } ) );
    auto instance = f.mSelection.GetTreeNode();
    CHECK( instance and instance->IsEntity() );
    if ( not instance )
        return;
    const Entity root = instance->AsEntity();
    // One entity at the top: it is the root, with the link to the file.
    CHECK( f.scene.GetComponent<TagComponent>( root ).mName == "body" );
    CHECK( f.scene.GetComponent<PrefabInstanceComponent>( root ).mPrefab == "prefabs/thing.prefab" );
    CHECK( instance->mChildren.size() == 1 );
    const Entity copiedPart = instance->mChildren[0]->AsEntity();
    CHECK( ParentOf( f.scene, copiedPart ) == root );
    UpdateWorldTransforms( f.scene );
    CHECK( Near( f.scene.GetComponent<TransformComponent>( copiedPart ).World().mPosition, vec3( 10, 2, 0 ) ) );
    // The reference in the state followed the copy.
    const sol::object named = f.scene.GetComponent<StateComponent>( root ).mState->as<Table>()["part"];
    CHECK( named.is<Entity>() and named.as<Entity>() == copiedPart );

    // Undo takes the instance out, redo brings it back under the same ids.
    f.history.Undo();
    CHECK( not f.scene.HasEntity( root ) );
    f.history.Redo();
    CHECK( f.scene.HasEntity( root ) and ParentOf( f.scene, copiedPart ) == root );

    // The instance is moved; then the prefab changes; an update keeps the
    // placement and the root's id, and takes the new content.
    f.scene.GetComponent<TransformComponent>( root ).mPosition = vec3( 20, 0, 0 );
    f.scene.GetComponent<TransformComponent>( part->AsEntity() ).mPosition = vec3( 0, 5, 0 );
    f.scene.GetComponent<TransformComponent>( body->AsEntity() ).mScale = vec3( 2 );
    CHECK( InvokeOperator( "prefab.save", ctx, { { "file", "prefabs/thing.prefab" }, { "node", body->ID() } } ) );
    CHECK( InvokeOperator( "prefab.update_instances", ctx, { { "file", "prefabs/thing.prefab" } } ) );
    CHECK( f.scene.HasEntity( root ) );
    // Where it was is the instance's; the scale is the prefab's.
    CHECK( Near( f.scene.GetComponent<TransformComponent>( root ).mPosition, vec3( 20, 0, 0 ) ) );
    CHECK( Near( f.scene.GetComponent<TransformComponent>( root ).mScale, vec3( 2 ) ) );
    const auto refreshed = FindNodeByEntity( root, f.root );
    CHECK( refreshed and refreshed->mChildren.size() == 1 );
    if ( refreshed and not refreshed->mChildren.empty() )
    {
        UpdateWorldTransforms( f.scene );
        CHECK( Near( f.scene.GetComponent<TransformComponent>( refreshed->mChildren[0]->AsEntity() ).World().mPosition, vec3( 20, 10, 0 ) ) );
    }
    // One step to undo: the old content back.
    f.history.Undo();
    CHECK( f.scene.HasEntity( copiedPart ) and ParentOf( f.scene, copiedPart ) == root );

    // A prefab file is a level: it opens as one, and cannot hold itself.
    Level opened;
    opened.Load( f.mDir / "prefabs/thing.prefab", f.project );
    CHECK( opened.mTreeRoot->mChildren.size() == 1 );
    OperatorContext inPrefab{ f.project, opened, f.history, f.mSelection, f.mClipboard };
    bool refused = false;
    try { InvokeOperator( "prefab.instantiate", inPrefab, { { "file", "prefabs/thing.prefab" } } ); }
    catch ( const std::exception& ) { refused = true; }
    CHECK( refused );
}

TEST( Prefab_SpawnAtRunTime )
{
    PrefabFixture f;
    auto a = f.Create( ProjectTreeNodeType::ModelObject );
    auto b = f.Create( ProjectTreeNodeType::ModelObject );
    auto folder = f.Create( ProjectTreeNodeType::Folder );
    f.history.Execute( CreateScope<MoveNodeCommand>( a, folder, f.scene ) );
    f.history.Execute( CreateScope<MoveNodeCommand>( b, folder, f.scene ) );
    f.scene.GetComponent<TransformComponent>( b->AsEntity() ).mPosition = vec3( 4, 2, 3 );
    SavePrefab( folder, f.scene, f.mDir / "two.prefab", f.project );

    // Two at the top: a root is made to hold them, placed where asked.
    Scene running;
    const auto spawned = SpawnPrefab( f.project, running, "two.prefab", vec3( 0, 100, 0 ) );
    CHECK( spawned.size() == 3 );
    const Entity root = spawned.front();
    CHECK( running.HasComponent<PrefabInstanceComponent>( root ) );
    CHECK( ChildrenOf( running, root ).size() == 2 );
    // b sat at (3, 0, 0) from a; a is the origin of the prefab.
    bool found = false;
    for ( const Entity child : ChildrenOf( running, root ) )
        if ( Near( running.GetComponent<TransformComponent>( child ).World().mPosition, vec3( 3, 100, 0 ) ) )
            found = true;
    CHECK( found );
}
