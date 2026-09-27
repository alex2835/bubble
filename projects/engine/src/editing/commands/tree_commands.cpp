#include "engine/pch/pch.hpp"
#include "engine/editing/commands/tree_commands.hpp"
#include "engine/project/project.hpp"
#include "engine/scene/hierarchy.hpp"
#include <sol/sol.hpp>
#include "engine/scene/components/audio_source_component.hpp"
#include "engine/scene/components/camera_component.hpp"
#include "engine/scene/components/character_controller_component.hpp"
#include "engine/scene/components/folder_component.hpp"
#include "engine/scene/components/hierarchy_component.hpp"
#include "engine/scene/components/light_component.hpp"
#include "engine/scene/components/model_component.hpp"
#include "engine/scene/components/prefab_instance_component.hpp"
#include "engine/scene/components/rigid_body_component.hpp"
#include "engine/scene/components/script_component.hpp"
#include "engine/scene/components/shader_component.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/transform_component.hpp"

namespace bubble
{
namespace
{
// An entity and what is under it, out of the scene into `backup` under their
// own ids. Detached from its parent first; the caller notes where it was.
void Park( Scene& scene, Entity entity, Scene& backup )
{
    const vector<Entity> subtree = Subtree( scene, entity );
    for ( const Entity e : subtree )
    {
        // Parked again after a redo: the stale copy goes first, or its
        // components are pushed twice.
        if ( backup.HasEntity( e ) )
            backup.RemoveEntity( e );
        scene.CopyEntityIntoWithId( backup, e, (size_t)e );
    }
    scene.RemoveEntities( subtree );
}

// The inverse; the links inside came along, the caller attaches the top.
void Unpark( Scene& scene, Entity entity, Scene& backup )
{
    const vector<Entity> subtree = Subtree( backup, entity );
    for ( const Entity e : subtree )
        backup.CopyEntityIntoWithId( scene, e, (size_t)e );
    backup.RemoveEntities( subtree );
}
}

/// CreateEntityCommand

CreateEntityCommand::CreateEntityCommand( Project& project, Scene& scene, Entity parent, EntityKind kind, const Transform& spawnAt )
    : mProject( project ),
      mScene( scene ),
      mParent( parent == INVALID_ENTITY ? scene.Root() : parent ),
      mKind( kind ),
      mSpawnAt( spawnAt ),
      mName( std::format( "Create {}", magic_enum::enum_name( kind ) ) )
{
}

Entity CreateEntityCommand::MakeEntity( EntityKind kind, Project& project, Scene& scene, const Transform& spawnAt )
{
    const Entity entity = scene.CreateEntity();
    scene.AddComponent<HierarchyComponent>( entity );
    switch ( kind )
    {
        case EntityKind::Folder:
            scene.AddComponent<TagComponent>( entity, "folder" );
            scene.AddComponent<TransformComponent>( entity );
            scene.AddComponent<FolderComponent>( entity );
            break;
        case EntityKind::ModelObject:
            scene.AddComponent<TagComponent>( entity, "Model object" );
            scene.AddComponent<TransformComponent>( entity, spawnAt );
            scene.AddComponent<ModelComponent>( entity );
            scene.AddComponent<ShaderComponent>( entity );
            break;
        case EntityKind::PhysicsObject:
            scene.AddComponent<TagComponent>( entity, "Physics object" );
            scene.AddComponent<TransformComponent>( entity, spawnAt );
            scene.AddComponent<ModelComponent>( entity );
            scene.AddComponent<ShaderComponent>( entity );
            scene.AddComponent<RigidBodyComponent>( entity );
            break;
        case EntityKind::GameObject:
            scene.AddComponent<TagComponent>( entity, "Game object" );
            scene.AddComponent<TransformComponent>( entity, spawnAt );
            scene.AddComponent<ModelComponent>( entity );
            scene.AddComponent<ShaderComponent>( entity );
            scene.AddComponent<CharacterControllerComponent>( entity );
            scene.AddComponent<StateComponent>( entity, project.mScriptingEngine.CreateTable() );
            scene.AddComponent<ScriptComponent>( entity );
            break;
        case EntityKind::Script:
            scene.AddComponent<TagComponent>( entity, "Script" );
            scene.AddComponent<StateComponent>( entity, project.mScriptingEngine.CreateTable() );
            scene.AddComponent<ScriptComponent>( entity );
            break;
        case EntityKind::Light:
            scene.AddComponent<TagComponent>( entity, "Light" );
            scene.AddComponent<TransformComponent>( entity, spawnAt );
            scene.AddComponent<LightComponent>( entity )
                 .SyncToTransform( scene.GetComponent<TransformComponent>( entity ) );
            break;
        case EntityKind::Camera:
            scene.AddComponent<TagComponent>( entity, "Camera" );
            scene.AddComponent<TransformComponent>( entity, spawnAt );
            scene.AddComponent<CameraComponent>( entity );
            break;
        case EntityKind::Audio:
            scene.AddComponent<TagComponent>( entity, "Audio" );
            scene.AddComponent<TransformComponent>( entity, spawnAt );
            scene.AddComponent<AudioSourceComponent>( entity )
                 .SyncToTransform( scene.GetComponent<TransformComponent>( entity ) );
            break;
    }
    return entity;
}

void CreateEntityCommand::Execute()
{
    mEntity = MakeEntity( mKind, mProject, mScene, mSpawnAt );
    AttachChild( mScene, mEntity, mParent );
    mIndex = IndexInParent( mScene, mEntity );
    // Under an entity, the spot asked for is still a place in the world.
    if ( mKind != EntityKind::Folder and mParent != mScene.Root() )
        SetWorldTransform( mScene, mEntity, mSpawnAt );
}

void CreateEntityCommand::Undo()
{
    if ( not mScene.HasEntity( mEntity ) )
        return;
    mIndex = DetachFromParent( mScene, mEntity );
    Park( mScene, mEntity, mBackup );
}

void CreateEntityCommand::Redo()
{
    if ( not mBackup.HasEntity( mEntity ) )
        return;
    Unpark( mScene, mEntity, mBackup );
    AttachChild( mScene, mEntity, mParent, mIndex );
}

/// DeleteEntitiesCommand

DeleteEntitiesCommand::DeleteEntitiesCommand( Scene& scene, vector<Entity> entities )
    : mScene( scene )
{
    // Not the root, and not what goes anyway with an ancestor that is taken.
    for ( const Entity entity : entities )
    {
        if ( entity == scene.Root() or not scene.HasEntity( entity ) )
            continue;
        const bool underAnother = std::ranges::any_of( entities, [&]( Entity other )
        {
            return other != entity and IsAncestor( scene, other, entity );
        } );
        if ( not underAnother and std::ranges::find( mEntities, entity ) == mEntities.end() )
            mEntities.push_back( entity );
    }
}

void DeleteEntitiesCommand::Execute()
{
    mTaken.clear();
    for ( const Entity entity : mEntities )
    {
        if ( not mScene.HasEntity( entity ) )
            continue;
        const Entity parent = ParentOf( mScene, entity );
        const size_t index = DetachFromParent( mScene, entity );
        mTaken.push_back( { entity, parent, index } );
        Park( mScene, entity, mBackup );
    }
}

void DeleteEntitiesCommand::Undo()
{
    // Back in reverse, so the noted indices hold as the siblings fill in.
    for ( auto it = mTaken.rbegin(); it != mTaken.rend(); ++it )
    {
        Unpark( mScene, it->mEntity, mBackup );
        if ( mScene.HasEntity( it->mParent ) )
            AttachChild( mScene, it->mEntity, it->mParent, it->mIndex );
    }
}

/// CopyEntityCommand

CopyEntityCommand::CopyEntityCommand( Scene& scene, Entity source, Entity parent )
    : mScene( scene ),
      mSource( source ),
      mParent( parent == INVALID_ENTITY ? scene.Root() : parent )
{
}

void CopyEntityCommand::Execute()
{
    if ( not mScene.HasEntity( mSource ) or mSource == mScene.Root() )
        return;
    const mat4 world = ComputeWorldMatrix( mScene, mSource );
    map<Entity, Entity> copied;
    mCopy = CopySubtree( mScene, mSource, mScene, copied );
    AttachChild( mScene, mCopy, mParent );
    mIndex = IndexInParent( mScene, mCopy );
    SetWorldTransform( mScene, mCopy, Transform::FromMatrix( world ) );
}

void CopyEntityCommand::Undo()
{
    if ( not mScene.HasEntity( mCopy ) )
        return;
    mIndex = DetachFromParent( mScene, mCopy );
    Park( mScene, mCopy, mBackup );
}

void CopyEntityCommand::Redo()
{
    if ( not mBackup.HasEntity( mCopy ) )
        return;
    Unpark( mScene, mCopy, mBackup );
    AttachChild( mScene, mCopy, mParent, mIndex );
}

/// MoveEntityCommand

MoveEntityCommand::MoveEntityCommand( Scene& scene, Entity entity, Entity parent, size_t index )
    : mScene( scene ),
      mEntity( entity ),
      mParent( parent == INVALID_ENTITY ? scene.Root() : parent ),
      mIndex( index )
{
}

void MoveEntityCommand::Execute()
{
    if ( not mScene.HasEntity( mEntity ) or mEntity == mScene.Root() or mEntity == mParent or
         IsAncestor( mScene, mEntity, mParent ) )
        return;
    mOldParent = ParentOf( mScene, mEntity );
    mOldIndex = IndexInParent( mScene, mEntity );
    if ( mScene.HasComponent<TransformComponent>( mEntity ) )
        mOldLocal = mScene.GetComponent<TransformComponent>( mEntity );
    // Moving down among the same siblings, the gap it leaves shifts the rest.
    size_t index = mIndex;
    if ( mOldParent == mParent and index != cAtEnd and mOldIndex < index )
        index--;
    SetParent( mScene, mEntity, mParent, /*keepWorld*/ true, index );
}

void MoveEntityCommand::Undo()
{
    if ( mOldParent == INVALID_ENTITY or not mScene.HasEntity( mEntity ) )
        return;
    DetachFromParent( mScene, mEntity );
    AttachChild( mScene, mEntity, mOldParent, mOldIndex );
    if ( mScene.HasComponent<TransformComponent>( mEntity ) )
        static_cast<Transform&>( mScene.GetComponent<TransformComponent>( mEntity ) ) = mOldLocal;
}

/// InstantiatePrefabCommand

InstantiatePrefabCommand::InstantiatePrefabCommand( Project& project,
                                                    Scene& scene,
                                                    Entity parent,
                                                    path relPrefab,
                                                    PrefabPlacement placement,
                                                    size_t index,
                                                    std::optional<size_t> rootId )
    : mProject( project ),
      mScene( scene ),
      mParent( parent == INVALID_ENTITY ? scene.Root() : parent ),
      mPrefab( std::move( relPrefab ) ),
      mPlacement( std::move( placement ) ),
      mIndex( index ),
      mRootId( rootId )
{
}

void InstantiatePrefabCommand::Execute()
{
    mRoot = InstantiatePrefab( mProject, mScene, mParent, mIndex, mPrefab, mPlacement, mRootId );
    mIndex = IndexInParent( mScene, mRoot );
}

void InstantiatePrefabCommand::Undo()
{
    if ( not mScene.HasEntity( mRoot ) )
        return;
    mIndex = DetachFromParent( mScene, mRoot );
    Park( mScene, mRoot, mBackup );
}

void InstantiatePrefabCommand::Redo()
{
    if ( not mBackup.HasEntity( mRoot ) )
        return;
    Unpark( mScene, mRoot, mBackup );
    AttachChild( mScene, mRoot, mParent, mIndex );
}

Command MakeRefreshPrefabInstance( Project& project, Scene& scene, Entity instance )
{
    if ( not scene.HasEntity( instance ) or not scene.HasComponent<PrefabInstanceComponent>( instance ) )
        throw std::runtime_error( "Not a prefab instance" );

    PrefabPlacement placement;
    placement.mLocal = static_cast<const Transform&>( scene.GetComponent<TransformComponent>( instance ) );
    const path prefab = scene.GetComponent<PrefabInstanceComponent>( instance ).mPrefab;
    const Entity parent = ParentOf( scene, instance );

    auto step = CreateScope<CompositeCommand>( "Update prefab instance" );
    step->Add( CreateScope<DeleteEntitiesCommand>( scene, vector<Entity>{ instance } ) );
    step->Add( CreateScope<InstantiatePrefabCommand>( project, scene, parent, prefab, placement,
                                                      IndexInParent( scene, instance ), (size_t)instance ) );
    return step;
}

}
