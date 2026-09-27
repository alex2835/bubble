#include "engine/pch/pch.hpp"
#include "engine/project/prefab.hpp"
#include "engine/project/project.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/scene/components/prefab_instance_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
namespace
{
void LoadPrefabFile( Level& prefab, const path& absFile, Project& project )
{
    if ( not filesystem::is_regular_file( absFile ) )
        throw std::runtime_error( std::format( "No prefab file {}", absFile.string() ) );
    prefab.Load( absFile, project );
}

// The prefab's root copied into `scene`, hanging from nothing, linked to its
// file. Everything made is in `copied`.
Entity CopyPrefab( Project& project, Scene& scene, const path& relPrefab, map<Entity, Entity>& copied, std::optional<size_t> rootId )
{
    Level prefab;
    LoadPrefabFile( prefab, project.RootDir() / relPrefab, project );
    const Entity root = CopySubtree( prefab.mScene, prefab.Root(), scene, copied, rootId );
    scene.AddComponent<PrefabInstanceComponent>( root, relPrefab.generic_string() );
    if ( not scene.HasComponent<TransformComponent>( root ) )
        scene.AddComponent<TransformComponent>( root );
    return root;
}
}


Entity InstantiatePrefab( Project& project,
                          Scene& scene,
                          Entity parent,
                          size_t index,
                          const path& relPrefab,
                          const PrefabPlacement& placement,
                          std::optional<size_t> rootId )
{
    map<Entity, Entity> copied;
    const Entity root = CopyPrefab( project, scene, relPrefab, copied, rootId );

    auto& transform = scene.GetComponent<TransformComponent>( root );
    if ( placement.mLocal )
    {
        transform.mPosition = placement.mLocal->mPosition;
        transform.mRotation = placement.mLocal->mRotation;
    }
    AttachChild( scene, root, parent == INVALID_ENTITY ? scene.Root() : parent, index );
    if ( placement.mName and scene.HasComponent<TagComponent>( root ) )
        scene.GetComponent<TagComponent>( root ).mName = *placement.mName;
    MakeNameUnique( scene, root );

    // Spawned at a point in the world, which under a parent is somewhere
    // else locally.
    if ( placement.mWorldPosition and not placement.mLocal )
    {
        Transform world = Transform::FromMatrix( ComputeWorldMatrix( scene, root ) );
        world.mPosition = *placement.mWorldPosition;
        SetWorldTransform( scene, root, world );
    }
    return root;
}

void SavePrefab( Scene& scene, Entity entity, const path& absFile, Project& project )
{
    Level prefab;
    prefab.mName = absFile.stem().string();

    // The entity becomes the prefab's root in place of the default one.
    const Entity placeholder = prefab.Root();
    map<Entity, Entity> copied;
    const Entity root = CopySubtree( scene, entity, prefab.mScene, copied );
    prefab.mScene.RemoveEntity( placeholder );
    prefab.mScene.SetRoot( root );

    // At the origin, turned and scaled as it is in the world.
    if ( prefab.mScene.HasComponent<TransformComponent>( root ) )
    {
        Transform world = Transform::FromMatrix( ComputeWorldMatrix( scene, entity ) );
        world.mPosition = vec3( 0 );
        static_cast<Transform&>( prefab.mScene.GetComponent<TransformComponent>( root ) ) = world;
    }
    // Inner instances keep their links; the new prefab's own root is not an
    // instance of anything.
    if ( prefab.mScene.HasComponent<PrefabInstanceComponent>( root ) )
        prefab.mScene.RemoveComponent<PrefabInstanceComponent>( root );

    filesystem::create_directories( absFile.parent_path() );
    prefab.Save( absFile, project );
}

vector<Entity> FindPrefabInstances( const Scene& scene, const string& relPrefab )
{
    vector<Entity> found;
    std::function<void( Entity )> walk = [&]( Entity entity )
    {
        if ( scene.HasComponent<PrefabInstanceComponent>( entity ) )
        {
            const string& prefab = scene.GetComponent<PrefabInstanceComponent>( entity ).mPrefab;
            if ( relPrefab.empty() or prefab == relPrefab )
            {
                found.push_back( entity );
                if ( not relPrefab.empty() )
                    return;
            }
        }
        for ( const Entity child : ChildrenOf( scene, entity ) )
            walk( child );
    };
    if ( scene.HasEntity( scene.Root() ) )
        walk( scene.Root() );
    return found;
}

vector<Entity> SpawnPrefab( Project& project, Scene& scene, const path& relPrefab, const vec3& position )
{
    map<Entity, Entity> copied;
    const Entity root = CopyPrefab( project, scene, relPrefab, copied, std::nullopt );
    if ( scene.HasEntity( scene.Root() ) )
        AttachChild( scene, root, scene.Root() );
    MakeNameUnique( scene, root );
    scene.GetComponent<TransformComponent>( root ).mPosition = position;
    UpdateWorldTransforms( scene );

    vector<Entity> spawned = { root };
    for ( const auto& [_, entity] : copied )
        if ( entity != root )
            spawned.push_back( entity );
    return spawned;
}

}
