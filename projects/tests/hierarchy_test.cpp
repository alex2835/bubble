#include "test.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/scene/components/hierarchy_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include <nlohmann/json.hpp>

namespace
{
bool Near( const vec3& a, const vec3& b, f32 epsilon = 1e-4f )
{
    return glm::length( a - b ) < epsilon;
}

vec3 WorldPosition( const Scene& scene, Entity entity )
{
    return scene.GetComponent<TransformComponent>( entity ).World().mPosition;
}

Entity Make( Scene& scene, const Transform& t )
{
    const Entity e = scene.CreateEntity();
    scene.AddComponent<TagComponent>( e, "e" );
    scene.AddComponent<TransformComponent>( e, t );
    return e;
}
}

TEST( Hierarchy_WorldFollowsParents )
{
    Scene scene;
    const Entity a = Make( scene, Transform( vec3( 10, 0, 0 ), vec3( 0, glm::half_pi<f32>(), 0 ), vec3( 2 ) ) );
    const Entity b = Make( scene, Transform( vec3( 1, 0, 0 ) ) );
    const Entity c = Make( scene, Transform( vec3( 0, 3, 0 ) ) );
    CHECK( SetParent( scene, b, a, /*keepWorld*/ false ) );
    CHECK( SetParent( scene, c, b, false ) );
    UpdateWorldTransforms( scene );

    // b: a's +X turned 90 about Y is -Z, doubled.
    CHECK( Near( WorldPosition( scene, b ), vec3( 10, 0, -2 ) ) );
    CHECK( Near( WorldPosition( scene, c ), vec3( 10, 6, -2 ) ) );
    CHECK( ParentOf( scene, c ) == b and ChildrenOf( scene, a ).size() == 1 );
    CHECK( IsAncestor( scene, a, c ) and not IsAncestor( scene, c, a ) );

    // No loops.
    CHECK( not SetParent( scene, a, c ) );
    CHECK( not SetParent( scene, a, a ) );

    // Re-parenting with keepWorld leaves it where it was.
    CHECK( SetParent( scene, c, INVALID_ENTITY, true ) );
    UpdateWorldTransforms( scene );
    CHECK( Near( WorldPosition( scene, c ), vec3( 10, 6, -2 ) ) );
    CHECK( not scene.HasComponent<HierarchyComponent>( c ) );
    // b lost its only child and has a parent still: it keeps the component.
    CHECK( scene.GetComponent<HierarchyComponent>( b ).mChildren.empty() );
}

TEST( Hierarchy_TreeIsTheTruthInTheEditor )
{
    // Entity under folder under entity: the folder is see-through.
    Fixture f;
    auto parent = f.Create( ProjectTreeNodeType::ModelObject );
    auto folder = f.Create( ProjectTreeNodeType::Folder );
    auto child = f.Create( ProjectTreeNodeType::Light );
    f.history.Execute( CreateScope<MoveNodeCommand>( folder, parent, f.scene ) );
    f.history.Execute( CreateScope<MoveNodeCommand>( child, folder, f.scene ) );
    CHECK( ParentOf( f.scene, child->AsEntity() ) == parent->AsEntity() );

    // Moved under, it stayed where it was in the world: both were made at
    // (1, 2, 3), so relative to the parent it is at the origin.
    UpdateWorldTransforms( f.scene );
    CHECK( Near( WorldPosition( f.scene, child->AsEntity() ), vec3( 1, 2, 3 ) ) );
    CHECK( Near( f.scene.GetComponent<TransformComponent>( child->AsEntity() ).mPosition, vec3( 0 ) ) );

    // Moving the parent moves the child.
    f.scene.GetComponent<TransformComponent>( parent->AsEntity() ).mPosition = vec3( 5, 0, 0 );
    UpdateWorldTransforms( f.scene );
    CHECK( Near( WorldPosition( f.scene, child->AsEntity() ), vec3( 5, 0, 0 ) ) );

    // Undo the second move: back at the top, where it was, local restored.
    f.history.Undo();
    CHECK( ParentOf( f.scene, child->AsEntity() ) == INVALID_ENTITY );
    CHECK( Near( f.scene.GetComponent<TransformComponent>( child->AsEntity() ).mPosition, vec3( 1, 2, 3 ) ) );
    f.history.Redo();
    CHECK( ParentOf( f.scene, child->AsEntity() ) == parent->AsEntity() );

    // A node cannot go under itself.
    const auto before = parent->mParent.lock();
    f.history.Execute( CreateScope<MoveNodeCommand>( parent, folder, f.scene ) );
    CHECK( parent->mParent.lock() == before );

    // Deleting the parent takes the child with it; undo brings both back linked.
    f.history.Execute( CreateScope<DeleteNodeCommand>( parent, f.scene ) );
    CHECK( not f.scene.HasEntity( child->AsEntity() ) );
    f.history.Undo();
    CHECK( ParentOf( f.scene, child->AsEntity() ) == parent->AsEntity() );
}

TEST( Hierarchy_SavedAndLoaded )
{
    Fixture f;
    auto parent = f.Create( ProjectTreeNodeType::ModelObject );
    auto child = f.Create( ProjectTreeNodeType::Light );
    f.history.Execute( CreateScope<MoveNodeCommand>( child, parent, f.scene ) );
    f.scene.GetComponent<TransformComponent>( parent->AsEntity() ).mPosition = vec3( 0, 10, 0 );

    const json saved = f.project.mLevel.ToJson( f.project );
    Level loaded;
    loaded.FromJson( saved, f.project );
    const Entity p = loaded.mScene.GetEntityById( (size_t)parent->AsEntity() );
    const Entity c = loaded.mScene.GetEntityById( (size_t)child->AsEntity() );
    CHECK( ParentOf( loaded.mScene, c ) == p );
    CHECK( ChildrenOf( loaded.mScene, p ).size() == 1 );
    // World transforms are ready right after a load.
    CHECK( Near( WorldPosition( loaded.mScene, c ), vec3( 0, 10, 0 ) ) );
}
