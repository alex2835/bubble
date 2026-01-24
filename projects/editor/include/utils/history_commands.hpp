#pragma once
#include "engine/project/project_tree.hpp"
#include <vector>
#include <memory>
#include <map>

namespace bubble
{

// Base command interface
class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
};

// Command: Delete node
class DeleteNodeCommand : public ICommand
{
public:
    DeleteNodeCommand( Ref<ProjectTreeNode> node, Scene& scene )
        : mNode( node )
        , mScene( scene )
        , mParent( node->mParent.lock() )
    {
        // Find the index where the node was in parent's children
        if ( mParent )
        {
            const auto& children = mParent->mChildren;
            auto it = std::ranges::find( children, mNode );
            if ( it != children.end() )
                mIndexInParent = std::distance( children.begin(), it );
        }
    }

    void Execute() override
    {
        if ( not mParent )
            return;

        // Collect all entities in the subtree
        set<Entity> entities;
        FillEntitiesInSubTree( entities, mNode );

        // Copy entities from main scene to our backup scene, preserving IDs
        for ( auto entity : entities )
        {
            Entity backupEntity = mScene.CopyEntityIntoWithId( mBackupScene, entity, (size_t)entity );
            mEntityMapping[entity] = backupEntity;
        }

        // Remove entities from main scene
        for ( auto entity : entities )
        {
            auto entityCopy = entity;
            mScene.RemoveEntity( entityCopy );
        }

        // Remove node from parent's children
        auto& children = mParent->mChildren;
        auto it = std::ranges::find( children, mNode );
        if ( it != children.end() )
            children.erase( it );
    }

    void Undo() override
    {
        if ( not mParent )
            return;

        // Restore entities from backup scene back to main scene
        RestoreNodeEntities( mNode );

        // Re-insert node at original index
        if ( mIndexInParent < mParent->mChildren.size() )
            mParent->mChildren.insert( mParent->mChildren.begin() + mIndexInParent, mNode );
        else
            mParent->mChildren.push_back( mNode );
    }

    ~DeleteNodeCommand()
    {
        // Backup scene and all its entities are automatically cleaned up
        // when this command is destroyed (goes out of scope from history)
    }

private:
    void RestoreNodeEntities( Ref<ProjectTreeNode>& node )
    {
        // If node has entity, restore it from backup
        if ( node->IsEntity() )
        {
            Entity originalEntity = node->AsEntity();
            if ( mEntityMapping.contains( originalEntity ) )
            {
                // Copy entity back from backup scene to main scene with original ID
                Entity backupEntity = mEntityMapping[originalEntity];
                Entity restoredEntity = mBackupScene.CopyEntityIntoWithId( mScene, backupEntity, (size_t)originalEntity );
                node->mState = restoredEntity;

                // Update mapping for potential future redo
                mEntityMapping[originalEntity] = backupEntity;
            }
        }

        // Recursively restore children
        for ( auto& child : node->mChildren )
        {
            child->mParent = node;
            RestoreNodeEntities( child );
        }
    }

    Ref<ProjectTreeNode> mNode;
    Ref<ProjectTreeNode> mParent;
    Scene& mScene;
    Scene mBackupScene; // Each command has its own backup scene
    map<Entity, Entity> mEntityMapping; // Main scene entity -> Backup scene entity
    size_t mIndexInParent = 0;
};

// Command: Delete multiple nodes (for multi-selection)
class DeleteMultipleNodesCommand : public ICommand
{
public:
    DeleteMultipleNodesCommand( const vector<Ref<ProjectTreeNode>>& nodes, Scene& scene, Ref<ProjectTreeNode> root )
        : mScene( scene )
        , mRoot( root )
    {
        // Store node info for each node
        for ( const auto& node : nodes )
        {
            auto parent = node->mParent.lock();
            if ( not parent )
                continue;

            NodeInfo info;
            info.mNode = node;
            info.mParent = parent;

            // Find index
            const auto& children = parent->mChildren;
            auto it = std::ranges::find( children, node );
            if ( it != children.end() )
                info.mIndexInParent = std::distance( children.begin(), it );

            mNodeInfos.push_back( info );
        }
    }

    void Execute() override
    {
        // Collect all entities from all nodes
        set<Entity> allEntities;
        for ( const auto& info : mNodeInfos )
        {
            set<Entity> nodeEntities;
            FillEntitiesInSubTree( nodeEntities, info.mNode );
            allEntities.insert( nodeEntities.begin(), nodeEntities.end() );
        }

        // Copy all entities to backup scene, preserving IDs
        for ( auto entity : allEntities )
        {
            Entity backupEntity = mScene.CopyEntityIntoWithId( mBackupScene, entity, (size_t)entity );
            mEntityMapping[entity] = backupEntity;
        }

        // Remove all entities from main scene
        for ( auto entity : allEntities )
        {
            auto entityCopy = entity;
            mScene.RemoveEntity( entityCopy );
        }

        // Remove all nodes from their parents
        for ( const auto& info : mNodeInfos )
        {
            auto& children = info.mParent->mChildren;
            auto it = std::ranges::find( children, info.mNode );
            if ( it != children.end() )
                children.erase( it );
        }
    }

    void Undo() override
    {
        // Restore all nodes in reverse order to maintain proper indexing
        for ( auto it = mNodeInfos.rbegin(); it != mNodeInfos.rend(); ++it )
        {
            auto& info = *it;
            // Restore entities for this node
            RestoreNodeEntities( info.mNode );

            // Re-insert node at original index
            if ( info.mIndexInParent < info.mParent->mChildren.size() )
                info.mParent->mChildren.insert( info.mParent->mChildren.begin() + info.mIndexInParent, info.mNode );
            else
                info.mParent->mChildren.push_back( info.mNode );
        }
    }

    ~DeleteMultipleNodesCommand()
    {
        // Backup scene and all its entities are automatically cleaned up
    }

private:
    void RestoreNodeEntities( Ref<ProjectTreeNode>& node )
    {
        // If node has entity, restore it from backup
        if ( node->IsEntity() )
        {
            Entity originalEntity = node->AsEntity();
            if ( mEntityMapping.contains( originalEntity ) )
            {
                Entity backupEntity = mEntityMapping[originalEntity];
                Entity restoredEntity = mBackupScene.CopyEntityIntoWithId( mScene, backupEntity, (size_t)originalEntity );
                node->mState = restoredEntity;
                mEntityMapping[originalEntity] = backupEntity;
            }
        }

        // Recursively restore children
        for ( auto& child : node->mChildren )
        {
            child->mParent = node;
            RestoreNodeEntities( child );
        }
    }

    struct NodeInfo
    {
        Ref<ProjectTreeNode> mNode;
        Ref<ProjectTreeNode> mParent;
        size_t mIndexInParent = 0;
    };

    vector<NodeInfo> mNodeInfos;
    Scene& mScene;
    Scene mBackupScene;
    Ref<ProjectTreeNode> mRoot;
    map<Entity, Entity> mEntityMapping;
};

// Command: Copy node (add new node)
class CopyNodeCommand : public ICommand
{
public:
    CopyNodeCommand( Ref<ProjectTreeNode> sourceNode,
                     Ref<ProjectTreeNode> targetParent,
                     Scene& scene )
        : mSourceNode( sourceNode )
        , mTargetParent( targetParent )
        , mScene( scene )
    {
    }

    void Execute() override
    {
        // Create the copy
        mCopiedNode = ProjectTreeNode::CopyNode( mSourceNode, mScene );
        mCopiedNode->mParent = mTargetParent;
        mTargetParent->mChildren.push_back( mCopiedNode );
    }

    void Undo() override
    {
        if ( not mCopiedNode or not mTargetParent )
            return;

        // Remove the copied node
        auto& children = mTargetParent->mChildren;
        auto it = std::ranges::find( children, mCopiedNode );
        if ( it != children.end() )
            children.erase( it );

        // Delete all entities in the copied subtree
        set<Entity> entitiesToRemove;
        FillEntitiesInSubTree( entitiesToRemove, mCopiedNode );
        for ( auto entity : entitiesToRemove )
            mScene.RemoveEntity( entity );

        mCopiedNode = nullptr;
    }

private:
    Ref<ProjectTreeNode> mSourceNode;
    Ref<ProjectTreeNode> mTargetParent;
    Ref<ProjectTreeNode> mCopiedNode;
    Scene& mScene;
};

// Command: Create node (for new entity/folder creation)
class CreateNodeCommand : public ICommand
{
public:
    CreateNodeCommand( Ref<ProjectTreeNode> parent,
                      ProjectTreeNodeType type,
                      ProjectTreeNode::StateType state,
                      Scene& scene )
        : mParent( parent )
        , mType( type )
        , mState( state )
        , mScene( scene )
    {
    }

    void Execute() override
    {
        if ( not mParent )
            return;

        // If this is a redo and we have a backup entity, restore it with original ID
        if ( mCreatedNode and mCreatedNode->IsEntity() and mBackupEntity != INVALID_ENTITY )
        {
            // Restore entity from backup scene with original ID
            Entity restoredEntity = mBackupScene.CopyEntityIntoWithId( mScene, mBackupEntity, (size_t)mBackupEntity );
            mCreatedNode->mState = restoredEntity;
        }

        // Create the new node if first execution
        if ( not mCreatedNode )
        {
            mCreatedNode = CreateRef<ProjectTreeNode>();
            mCreatedNode->mType = mType;
            mCreatedNode->mState = mState;
        }

        // Re-insert node into parent
        mCreatedNode->mParent = mParent;
        mParent->mChildren.push_back( mCreatedNode );
    }

    void Undo() override
    {
        if ( not mCreatedNode or not mParent )
            return;

        // Remove the created node from tree
        auto& children = mParent->mChildren;
        auto it = std::ranges::find( children, mCreatedNode );
        if ( it != children.end() )
            children.erase( it );

        // If node has an entity, move it to backup scene
        if ( mCreatedNode->IsEntity() )
        {
            Entity entity = mCreatedNode->AsEntity();

            // Copy entity to backup scene with same ID for future redo
            mBackupEntity = mScene.CopyEntityIntoWithId( mBackupScene, entity, (size_t)entity );

            // Remove from main scene
            mScene.RemoveEntity( entity );
        }
    }

    ~CreateNodeCommand()
    {
        // Cleanup backup entity when command is destroyed
        if ( mBackupEntity != INVALID_ENTITY )
        {
            auto entityCopy = mBackupEntity;
            mBackupScene.RemoveEntity( entityCopy );
        }
    }

    Ref<ProjectTreeNode> GetCreatedNode() const { return mCreatedNode; }

private:
    Ref<ProjectTreeNode> mParent;
    Ref<ProjectTreeNode> mCreatedNode;
    ProjectTreeNodeType mType;
    ProjectTreeNode::StateType mState;
    Scene& mScene;
    Scene mBackupScene; // Backup scene for undo/redo
    Entity mBackupEntity = INVALID_ENTITY; // Backup entity in backup scene
};

// Command: Move node
class MoveNodeCommand : public ICommand
{
public:
    MoveNodeCommand( Ref<ProjectTreeNode> node,
                     Ref<ProjectTreeNode> newParent )
        : mNode( node )
        , mOldParent( node->mParent.lock() )
        , mNewParent( newParent )
    {
        // Store old index
        if ( mOldParent )
        {
            const auto& children = mOldParent->mChildren;
            auto it = std::ranges::find( children, mNode );
            if ( it != children.end() )
                mOldIndexInParent = std::distance( children.begin(), it );
        }
    }

    void Execute() override
    {
        if ( not mOldParent or not mNewParent )
            return;

        // Remove from old parent
        auto& oldChildren = mOldParent->mChildren;
        auto it = std::ranges::find( oldChildren, mNode );
        if ( it != oldChildren.end() )
            oldChildren.erase( it );

        // Add to new parent
        mNode->mParent = mNewParent;
        mNewParent->mChildren.push_back( mNode );
    }

    void Undo() override
    {
        if ( not mOldParent or not mNewParent )
            return;

        // Remove from new parent
        auto& newChildren = mNewParent->mChildren;
        auto it = std::ranges::find( newChildren, mNode );
        if ( it != newChildren.end() )
            newChildren.erase( it );

        // Restore to old parent at original index
        mNode->mParent = mOldParent;
        if ( mOldIndexInParent < mOldParent->mChildren.size() )
            mOldParent->mChildren.insert( mOldParent->mChildren.begin() + mOldIndexInParent, mNode );
        else
            mOldParent->mChildren.push_back( mNode );
    }

private:
    Ref<ProjectTreeNode> mNode;
    Ref<ProjectTreeNode> mOldParent;
    Ref<ProjectTreeNode> mNewParent;
    size_t mOldIndexInParent = 0;
};

// Command: Transform change (for entity transforms)
class TransformChangeCommand : public ICommand
{
public:
    TransformChangeCommand( Entity entity,
                            Scene& scene,
                            const Transform& oldTransform,
                            const Transform& newTransform )
        : mEntity( entity )
        , mScene( scene )
        , mOldTransform( oldTransform )
        , mNewTransform( newTransform )
    {
    }

    void Execute() override
    {
        if ( mScene.HasComponent<TransformComponent>( mEntity ) )
        {
            auto& transform = mScene.GetComponent<TransformComponent>( mEntity );
            transform.mPosition = mNewTransform.mPosition;
            transform.mRotation = mNewTransform.mRotation;
            transform.mScale = mNewTransform.mScale;
        }
    }

    void Undo() override
    {
        if ( mScene.HasComponent<TransformComponent>( mEntity ) )
        {
            auto& transform = mScene.GetComponent<TransformComponent>( mEntity );
            transform.mPosition = mOldTransform.mPosition;
            transform.mRotation = mOldTransform.mRotation;
            transform.mScale = mOldTransform.mScale;
        }
    }

private:
    Entity mEntity;
    Scene& mScene;
    Transform mOldTransform;
    Transform mNewTransform;
};

// Command: Multi-entity transform change (for multiple entities at once)
class MultiTransformChangeCommand : public ICommand
{
public:
    MultiTransformChangeCommand( const set<Entity>& entities,
                                 Scene& scene,
                                 const map<Entity, Transform>& oldTransforms,
                                 const map<Entity, Transform>& newTransforms )
        : mEntities( entities )
        , mScene( scene )
        , mOldTransforms( oldTransforms )
        , mNewTransforms( newTransforms )
    {
    }

    void Execute() override
    {
        for ( auto entity : mEntities )
        {
            if ( mScene.HasComponent<TransformComponent>( entity ) and
                 mNewTransforms.contains( entity ) )
            {
                auto& transform = mScene.GetComponent<TransformComponent>( entity );
                const auto& newTransform = mNewTransforms.at( entity );
                transform.mPosition = newTransform.mPosition;
                transform.mRotation = newTransform.mRotation;
                transform.mScale = newTransform.mScale;
            }
        }
    }

    void Undo() override
    {
        for ( auto entity : mEntities )
        {
            if ( mScene.HasComponent<TransformComponent>( entity ) and
                 mOldTransforms.contains( entity ) )
            {
                auto& transform = mScene.GetComponent<TransformComponent>( entity );
                const auto& oldTransform = mOldTransforms.at( entity );
                transform.mPosition = oldTransform.mPosition;
                transform.mRotation = oldTransform.mRotation;
                transform.mScale = oldTransform.mScale;
            }
        }
    }

private:
    set<Entity> mEntities;
    Scene& mScene;
    map<Entity, Transform> mOldTransforms;
    map<Entity, Transform> mNewTransforms;
};

// Command: Delete entities (standalone entities without tree structure)
class DeleteEntitiesCommand : public ICommand
{
public:
    DeleteEntitiesCommand( const set<Entity>& entities, Scene& scene )
        : mEntities( entities )
        , mScene( scene )
    {
        // Store copies of all entities before deletion
        for ( auto entity : entities )
        {
            mEntityCopies[entity] = mScene.CopyEntity( entity );
        }
    }

    void Execute() override
    {
        // Delete the original entities
        for ( auto entity : mEntities )
        {
            auto entityCopy = entity; // Need a copy since RemoveEntity modifies the entity
            mScene.RemoveEntity( entityCopy );
        }
    }

    void Undo() override
    {
        // Restore entities by copying back from the saved copies
        map<Entity, Entity> oldToNewMapping;

        for ( auto entity : mEntities )
        {
            if ( mEntityCopies.contains( entity ) )
            {
                // Create a new entity by copying the saved copy
                Entity copiedEntity = mEntityCopies[entity];
                Entity newEntity = mScene.CopyEntity( copiedEntity );
                oldToNewMapping[entity] = newEntity;
            }
        }

        // Update the mEntities set with new entity IDs
        // (This is needed because entity IDs change after recreation)
        set<Entity> newEntities;
        for ( auto entity : mEntities )
        {
            if ( oldToNewMapping.contains( entity ) )
                newEntities.insert( oldToNewMapping[entity] );
        }

        // Note: This doesn't perfectly restore entity IDs, but restores all component data
    }

    ~DeleteEntitiesCommand()
    {
        // Clean up the copied entities
        for ( auto [originalEntity, copiedEntity] : mEntityCopies )
        {
            auto entityCopy = copiedEntity;
            mScene.RemoveEntity( entityCopy );
        }
    }

private:
    set<Entity> mEntities;
    Scene& mScene;
    map<Entity, Entity> mEntityCopies; // Original entity -> backup copy
};


} // namespace bubble