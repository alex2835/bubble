#include "engine/pch/pch.hpp"
#include "engine/editing/scene_commands.hpp"
#include "engine/project/project.hpp"
#include "engine/scene/component_manager.hpp"
#include <sol/sol.hpp>

namespace bubble
{
namespace
{
size_t IndexIn( const Ref<ProjectTreeNode>& parent, const Ref<ProjectTreeNode>& node )
{
    const auto& children = parent->mChildren;
    const auto it = std::ranges::find( children, node );
    return it == children.end() ? children.size() : std::distance( children.begin(), it );
}

void EraseFrom( const Ref<ProjectTreeNode>& parent, const Ref<ProjectTreeNode>& node )
{
    auto& children = parent->mChildren;
    const auto it = std::ranges::find( children, node );
    if ( it != children.end() )
        children.erase( it );
}

void InsertAt( const Ref<ProjectTreeNode>& parent, const Ref<ProjectTreeNode>& node, size_t index )
{
    auto& children = parent->mChildren;
    if ( index < children.size() )
        children.insert( children.begin() + index, node );
    else
        children.push_back( node );
}

// Every entity of the subtree into `backup` under its own id, then out of
// `scene`. Deleting in one batch is what keeps a big selection O(n).
void ParkSubtreeEntities( const Ref<ProjectTreeNode>& node, Scene& scene, Scene& backup, map<Entity, Entity>& mapping )
{
    set<Entity> entities;
    FillEntitiesInSubTree( entities, node );
    for ( const auto entity : entities )
    {
        // Redo after undo parks the same id again; the stale copy has to go
        // first or its components get pushed twice.
        if ( backup.HasEntity( entity ) )
            backup.RemoveEntity( entity );
        mapping[entity] = scene.CopyEntityIntoWithId( backup, entity, (size_t)entity );
    }
    const vector<Entity> removeList( entities.begin(), entities.end() );
    scene.RemoveEntities( removeList );
}

// The inverse: back under the same id, node by node so the node's Entity
// state is refreshed as it goes.
void UnparkSubtreeEntities( Ref<ProjectTreeNode>& node, Scene& scene, Scene& backup, map<Entity, Entity>& mapping )
{
    if ( node->IsEntity() )
    {
        const Entity original = node->AsEntity();
        if ( const auto it = mapping.find( original ); it != mapping.end() )
            node->mState = backup.CopyEntityIntoWithId( scene, it->second, (size_t)original );
    }
    for ( auto& child : node->mChildren )
    {
        child->mParent = node;
        UnparkSubtreeEntities( child, scene, backup, mapping );
    }
}
}

/// DeleteNodeCommand

DeleteNodeCommand::DeleteNodeCommand( Ref<ProjectTreeNode> node, Scene& scene )
    : mNode( node ),
      mParent( node->mParent.lock() ),
      mScene( scene )
{
    if ( mParent )
        mIndexInParent = IndexIn( mParent, mNode );
}

void DeleteNodeCommand::Execute()
{
    if ( not mParent )
        return;
    ParkSubtreeEntities( mNode, mScene, mBackupScene, mEntityMapping );
    EraseFrom( mParent, mNode );
}

void DeleteNodeCommand::Undo()
{
    if ( not mParent )
        return;
    RestoreNodeEntities( mNode );
    InsertAt( mParent, mNode, mIndexInParent );
}

void DeleteNodeCommand::RestoreNodeEntities( Ref<ProjectTreeNode>& node )
{
    UnparkSubtreeEntities( node, mScene, mBackupScene, mEntityMapping );
}

/// DeleteMultipleNodesCommand

DeleteMultipleNodesCommand::DeleteMultipleNodesCommand( const vector<Ref<ProjectTreeNode>>& nodes, Scene& scene )
    : mScene( scene )
{
    for ( const auto& node : nodes )
    {
        auto parent = node->mParent.lock();
        if ( not parent )
            continue;
        mNodeInfos.push_back( { node, parent, IndexIn( parent, node ) } );
    }
}

void DeleteMultipleNodesCommand::Execute()
{
    for ( const auto& info : mNodeInfos )
        ParkSubtreeEntities( info.mNode, mScene, mBackupScene, mEntityMapping );
    for ( const auto& info : mNodeInfos )
        EraseFrom( info.mParent, info.mNode );
}

void DeleteMultipleNodesCommand::Undo()
{
    // Reverse order so the stored indices are right as the siblings fill in.
    for ( auto it = mNodeInfos.rbegin(); it != mNodeInfos.rend(); ++it )
    {
        RestoreNodeEntities( it->mNode );
        InsertAt( it->mParent, it->mNode, it->mIndexInParent );
    }
}

void DeleteMultipleNodesCommand::RestoreNodeEntities( Ref<ProjectTreeNode>& node )
{
    UnparkSubtreeEntities( node, mScene, mBackupScene, mEntityMapping );
}

/// CopyNodeCommand

CopyNodeCommand::CopyNodeCommand( Ref<ProjectTreeNode> sourceNode, Ref<ProjectTreeNode> targetParent, Scene& scene )
    : mSourceNode( sourceNode ),
      mTargetParent( targetParent ),
      mScene( scene )
{
}

void CopyNodeCommand::Execute()
{
    mCopiedNode = ProjectTreeNode::CopyNode( mSourceNode, mScene );
    mCopiedNode->mParent = mTargetParent;
    mTargetParent->mChildren.push_back( mCopiedNode );
}

void CopyNodeCommand::Undo()
{
    if ( not mCopiedNode or not mTargetParent )
        return;
    ParkSubtreeEntities( mCopiedNode, mScene, mBackupScene, mEntityMapping );
    EraseFrom( mTargetParent, mCopiedNode );
}

void CopyNodeCommand::Redo()
{
    if ( not mCopiedNode or not mTargetParent )
        return;
    UnparkSubtreeEntities( mCopiedNode, mScene, mBackupScene, mEntityMapping );
    mCopiedNode->mParent = mTargetParent;
    mTargetParent->mChildren.push_back( mCopiedNode );
}

/// CreateNodeCommand

CreateNodeCommand::CreateNodeCommand( Ref<ProjectTreeNode> parent,
                                      ProjectTreeNodeType type,
                                      Project& project,
                                      const Transform& spawnAt )
    : mParent( parent ),
      mType( type ),
      mProject( project ),
      mSpawnAt( spawnAt ),
      mName( std::format( "Create {}", magic_enum::enum_name( type ) ) )
{
}

Entity CreateNodeCommand::CreateEntityFor( ProjectTreeNodeType type, Project& project, const Transform& spawnAt )
{
    Scene& scene = project.mLevel.mScene;
    switch ( type )
    {
        case ProjectTreeNodeType::ModelObject:
        {
            const auto entity = scene.CreateEntity();
            scene.AddComponent<TagComponent>( entity, "Model object" );
            scene.AddComponent<TransformComponent>( entity, spawnAt );
            scene.AddComponent<ModelComponent>( entity );
            scene.AddComponent<ShaderComponent>( entity );
            return entity;
        }
        case ProjectTreeNodeType::PhysicsObject:
        {
            const auto entity = scene.CreateEntity();
            scene.AddComponent<TagComponent>( entity, "Physics object" );
            scene.AddComponent<TransformComponent>( entity, spawnAt );
            scene.AddComponent<ModelComponent>( entity );
            scene.AddComponent<ShaderComponent>( entity );
            scene.AddComponent<RigidBodyComponent>( entity );
            return entity;
        }
        case ProjectTreeNodeType::GameObject:
        {
            const auto entity = scene.CreateEntity();
            scene.AddComponent<TagComponent>( entity, "Game object" );
            scene.AddComponent<TransformComponent>( entity, spawnAt );
            scene.AddComponent<ModelComponent>( entity );
            scene.AddComponent<ShaderComponent>( entity );
            scene.AddComponent<CharacterControllerComponent>( entity );
            scene.AddComponent<StateComponent>( entity, project.mScriptingEngine.CreateTable() );
            scene.AddComponent<ScriptComponent>( entity );
            return entity;
        }
        case ProjectTreeNodeType::Script:
        {
            const auto entity = scene.CreateEntity();
            scene.AddComponent<TagComponent>( entity, "Script" );
            scene.AddComponent<StateComponent>( entity, project.mScriptingEngine.CreateTable() );
            scene.AddComponent<ScriptComponent>( entity );
            return entity;
        }
        case ProjectTreeNodeType::Light:
        {
            const auto entity = scene.CreateEntity();
            scene.AddComponent<TagComponent>( entity, "Light" );
            scene.AddComponent<TransformComponent>( entity, spawnAt );
            scene.AddComponent<LightComponent>( entity )
                 .SyncToTransform( scene.GetComponent<TransformComponent>( entity ) );
            return entity;
        }
        case ProjectTreeNodeType::Camera:
        {
            const auto entity = scene.CreateEntity();
            scene.AddComponent<TagComponent>( entity, "Camera" );
            scene.AddComponent<TransformComponent>( entity, spawnAt );
            scene.AddComponent<CameraComponent>( entity );
            return entity;
        }
        case ProjectTreeNodeType::Root:
        case ProjectTreeNodeType::Folder:
            return INVALID_ENTITY;
    }
    return INVALID_ENTITY;
}

void CreateNodeCommand::Execute()
{
    if ( not mParent )
        return;

    mCreatedNode = CreateRef<ProjectTreeNode>( mProject.mLevel.mNodeIDCounter );
    mCreatedNode->mType = mType;
    if ( mType == ProjectTreeNodeType::Folder )
        mCreatedNode->mState = "folder"s;
    else
        mCreatedNode->mState = CreateEntityFor( mType, mProject, mSpawnAt );

    mCreatedNode->mParent = mParent;
    mParent->mChildren.push_back( mCreatedNode );
}

void CreateNodeCommand::Redo()
{
    if ( not mCreatedNode or not mParent )
        return;

    if ( mBackupEntity != INVALID_ENTITY )
    {
        // The entity waited in the backup under its original id.
        Scene& scene = mProject.mLevel.mScene;
        mCreatedNode->mState = mBackupScene.CopyEntityIntoWithId( scene, mBackupEntity, (size_t)mBackupEntity );
        mBackupScene.RemoveEntity( mBackupEntity );
        mBackupEntity = INVALID_ENTITY;
    }

    mCreatedNode->mParent = mParent;
    mParent->mChildren.push_back( mCreatedNode );
}

void CreateNodeCommand::Undo()
{
    if ( not mCreatedNode or not mParent )
        return;
    EraseFrom( mParent, mCreatedNode );

    if ( mCreatedNode->IsEntity() )
    {
        Scene& scene = mProject.mLevel.mScene;
        const Entity entity = mCreatedNode->AsEntity();
        mBackupEntity = scene.CopyEntityIntoWithId( mBackupScene, entity, (size_t)entity );
        scene.RemoveEntity( entity );
    }
}

CreateNodeCommand::~CreateNodeCommand()
{
    if ( mBackupEntity != INVALID_ENTITY )
        mBackupScene.RemoveEntity( mBackupEntity );
}

/// MoveNodeCommand

MoveNodeCommand::MoveNodeCommand( Ref<ProjectTreeNode> node, Ref<ProjectTreeNode> newParent )
    : mNode( node ),
      mOldParent( node->mParent.lock() ),
      mNewParent( newParent )
{
    if ( mOldParent )
        mOldIndexInParent = IndexIn( mOldParent, mNode );
}

void MoveNodeCommand::Execute()
{
    if ( not mOldParent or not mNewParent )
        return;
    EraseFrom( mOldParent, mNode );
    mNode->mParent = mNewParent;
    mNewParent->mChildren.push_back( mNode );
}

void MoveNodeCommand::Undo()
{
    if ( not mOldParent or not mNewParent )
        return;
    EraseFrom( mNewParent, mNode );
    mNode->mParent = mOldParent;
    InsertAt( mOldParent, mNode, mOldIndexInParent );
}

/// TransformChangeCommand

namespace
{
void SetTransform( Scene& scene, Entity entity, const Transform& t )
{
    if ( not scene.HasComponent<TransformComponent>( entity ) )
        return;
    auto& transform = scene.GetComponent<TransformComponent>( entity );
    transform.mPosition = t.mPosition;
    transform.mRotation = t.mRotation;
    transform.mScale = t.mScale;
}
}

TransformChangeCommand::TransformChangeCommand( Entity entity, Scene& scene,
                                                const Transform& oldTransform, const Transform& newTransform )
    : mEntity( entity ),
      mScene( scene ),
      mOldTransform( oldTransform ),
      mNewTransform( newTransform )
{
}

void TransformChangeCommand::Execute() { SetTransform( mScene, mEntity, mNewTransform ); }
void TransformChangeCommand::Undo() { SetTransform( mScene, mEntity, mOldTransform ); }

MultiTransformChangeCommand::MultiTransformChangeCommand( const set<Entity>& entities,
                                                          Scene& scene,
                                                          const map<Entity, Transform>& oldTransforms,
                                                          const map<Entity, Transform>& newTransforms )
    : mEntities( entities ),
      mScene( scene ),
      mOldTransforms( oldTransforms ),
      mNewTransforms( newTransforms )
{
}

void MultiTransformChangeCommand::Execute()
{
    for ( const auto entity : mEntities )
        if ( const auto it = mNewTransforms.find( entity ); it != mNewTransforms.end() )
            SetTransform( mScene, entity, it->second );
}

void MultiTransformChangeCommand::Undo()
{
    for ( const auto entity : mEntities )
        if ( const auto it = mOldTransforms.find( entity ); it != mOldTransforms.end() )
            SetTransform( mScene, entity, it->second );
}

/// AddComponentCommand

AddComponentCommand::AddComponentCommand( Entity entity, ComponentTypeId componentId, Project& project )
    : mEntity( entity ),
      mComponentId( componentId ),
      mProject( project ),
      mName( std::format( "Add {}", ComponentManager::GetName( componentId ) ) )
{
}

void AddComponentCommand::Execute()
{
    Scene& scene = mProject.mLevel.mScene;
    if ( mComponentId == StateComponent::ID() )
        scene.AddComponent<StateComponent>( mEntity, mProject.mScriptingEngine.CreateTable() );
    else
        scene.EntityAddComponentId( mEntity, mComponentId );
}

void AddComponentCommand::Undo()
{
    mProject.mLevel.mScene.EntityRemoveComponentId( mEntity, mComponentId );
}

/// RemoveComponentCommand

RemoveComponentCommand::RemoveComponentCommand( Entity entity, ComponentTypeId componentId, Scene& scene )
    : mEntity( entity ),
      mComponentId( componentId ),
      mScene( scene ),
      mName( std::format( "Remove {}", ComponentManager::GetName( componentId ) ) )
{
}

void RemoveComponentCommand::Execute()
{
    // Only the one component is parked, on a stand-in entity of the backup.
    if ( mBackupEntity == INVALID_ENTITY )
        mBackupEntity = mBackupScene.CreateEntity();
    mScene.CopyComponentInto( mBackupScene, mEntity, mComponentId, mBackupEntity );
    mScene.EntityRemoveComponentId( mEntity, mComponentId );
}

void RemoveComponentCommand::Undo()
{
    mBackupScene.CopyComponentInto( mScene, mBackupEntity, mComponentId, mEntity );
    mBackupScene.EntityRemoveComponentId( mBackupEntity, mComponentId );
}

}
