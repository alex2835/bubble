#include "test.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/scene/components/folder_component.hpp"
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
    const Entity e = CreateChildEntity( scene );
    scene.AddComponent<TagComponent>( e, "e" );
    scene.AddComponent<TransformComponent>( e, t );
    return e;
}
}

TEST( Hierarchy_WorldFollowsParents )
{
    Level level;
    Scene& scene = level.mScene;
    const Entity a = Make( scene, Transform( vec3( 10, 0, 0 ), vec3( 0, glm::half_pi<f32>(), 0 ), vec3( 2 ) ) );
    const Entity b = Make( scene, Transform( vec3( 1, 0, 0 ) ) );
    const Entity c = Make( scene, Transform( vec3( 0, 3, 0 ) ) );
    CHECK( ParentOf( scene, a ) == scene.Root() );
    CHECK( SetParent( scene, b, a, /*keepWorld*/ false ) );
    CHECK( SetParent( scene, c, b, false ) );
    UpdateWorldTransforms( scene );

    // b: a's +X turned 90 about Y is -Z, doubled.
    CHECK( Near( WorldPosition( scene, b ), vec3( 10, 0, -2 ) ) );
    CHECK( Near( WorldPosition( scene, c ), vec3( 10, 6, -2 ) ) );
    CHECK( ParentOf( scene, c ) == b and ChildrenOf( scene, a ).size() == 1 );
    CHECK( IsAncestor( scene, a, c ) and not IsAncestor( scene, c, a ) );
    CHECK( ( Subtree( scene, a ) == vector{ a, b, c } ) );

    // No loops, and the root stays the root.
    CHECK( not SetParent( scene, a, c ) );
    CHECK( not SetParent( scene, a, a ) );
    CHECK( not SetParent( scene, scene.Root(), a ) );

    // Back under the root, keeping its place in the world.
    CHECK( SetParent( scene, c, INVALID_ENTITY, true ) );
    UpdateWorldTransforms( scene );
    CHECK( ParentOf( scene, c ) == scene.Root() );
    CHECK( Near( WorldPosition( scene, c ), vec3( 10, 6, -2 ) ) );
    CHECK( ChildrenOf( scene, b ).empty() );
}

TEST( Hierarchy_FolderIsAGroup )
{
    Fixture f;
    const Entity folder = f.Create( EntityKind::Folder );
    const Entity child = f.Create( EntityKind::Light, folder );
    UpdateWorldTransforms( f.scene );
    // Made at (1, 2, 3) in the world, under a folder that sits at the origin.
    CHECK( Near( WorldPosition( f.scene, child ), vec3( 1, 2, 3 ) ) );

    // Moving the folder moves what is in it.
    f.scene.GetComponent<TransformComponent>( folder ).mPosition = vec3( 5, 0, 0 );
    UpdateWorldTransforms( f.scene );
    CHECK( Near( WorldPosition( f.scene, child ), vec3( 6, 2, 3 ) ) );

    // Moved to the top, it stays where it is; undo puts it back.
    f.history.Execute( CreateScope<MoveEntityCommand>( f.scene, child, f.Root() ) );
    UpdateWorldTransforms( f.scene );
    CHECK( ParentOf( f.scene, child ) == f.Root() );
    CHECK( Near( WorldPosition( f.scene, child ), vec3( 6, 2, 3 ) ) );
    f.history.Undo();
    CHECK( ParentOf( f.scene, child ) == folder );
    CHECK( Near( f.scene.GetComponent<TransformComponent>( child ).mPosition, vec3( 1, 2, 3 ) ) );

    // An entity cannot go under itself.
    f.history.Execute( CreateScope<MoveEntityCommand>( f.scene, folder, child ) );
    CHECK( ParentOf( f.scene, folder ) == f.Root() );
}

TEST( Hierarchy_SavedAndLoaded )
{
    Fixture f;
    const Entity parent = f.Create( EntityKind::ModelObject );
    const Entity first = f.Create( EntityKind::Light, parent );
    const Entity second = f.Create( EntityKind::Camera, parent );
    f.scene.GetComponent<TransformComponent>( parent ).mPosition = vec3( 0, 10, 0 );

    const json saved = f.project.mLevel.ToJson( f.project );
    CHECK( not saved.contains( "ProjectTree" ) );
    Level loaded;
    loaded.FromJson( saved, f.project );
    const auto id = []( Scene& s, Entity e ) { return s.GetEntityById( (size_t)e ); };
    CHECK( loaded.Root() == id( loaded.mScene, f.Root() ) );
    CHECK( ParentOf( loaded.mScene, id( loaded.mScene, parent ) ) == loaded.Root() );
    // The children in their order.
    const auto children = ChildrenOf( loaded.mScene, id( loaded.mScene, parent ) );
    CHECK( children.size() == 2 and children[0] == id( loaded.mScene, first ) and children[1] == id( loaded.mScene, second ) );
    // World transforms are ready right after a load. `first` was made at
    // (1, 2, 3) in the world, where its parent then was, so it sits on it.
    CHECK( Near( WorldPosition( loaded.mScene, id( loaded.mScene, first ) ), vec3( 0, 10, 0 ) ) );
}

TEST( Hierarchy_OldTreeMigrates )
{
    // A level as files were before the tree moved into the scene: entities
    // with no Hierarchy, and a tree beside them with a folder in it.
    Fixture f;
    const json old = json::parse( R"({
      "Scene": {
        "Entity counter": 3,
        "Entity components": { "1": [0, 1], "2": [0, 1] },
        "Component pools": {
          "Tag": { "1": { "Tag": "a", "Class": "Object" }, "2": { "Tag": "b", "Class": "Object" } },
          "Transform": { "1": { "Position": [1, 0, 0], "Rotation": [0, 0, 0], "Scale": [1, 1, 1] },
                         "2": { "Position": [0, 2, 0], "Rotation": [0, 0, 0], "Scale": [1, 1, 1] } }
        }
      },
      "ProjectTree": { "Counter": 4, "Tree": { "ID": 0, "Type": "Level", "State": "main", "Children": [
        { "ID": 1, "Type": "Folder", "State": "props", "Children": [
          { "ID": 2, "Type": "ModelObject", "State": 1, "Children": [
            { "ID": 3, "Type": "Light", "State": 2, "Children": [] } ] } ] } ] } }
    })" );
    Level level;
    level.FromJson( old, f.project );
    Scene& scene = level.mScene;
    const Entity a = scene.GetEntityById( 1 ), b = scene.GetEntityById( 2 );

    CHECK( scene.GetComponent<TagComponent>( level.Root() ).mName == "main" );
    const auto top = ChildrenOf( scene, level.Root() );
    CHECK( top.size() == 1 );
    const Entity props = top[0];
    CHECK( scene.HasComponent<FolderComponent>( props ) and scene.GetComponent<TagComponent>( props ).mName == "props" );
    CHECK( ParentOf( scene, a ) == props and ParentOf( scene, b ) == a );
    // The folder is at the origin: nothing moved.
    CHECK( Near( WorldPosition( scene, b ), vec3( 1, 2, 0 ) ) );
    // Saved again, the tree is gone from the file.
    CHECK( not level.ToJson( f.project ).contains( "ProjectTree" ) );
}
