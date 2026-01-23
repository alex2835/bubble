#pragma once
#include "engine/project/project_tree.hpp"
#include <vector>
#include <memory>

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

        // Collect all entities before deletion
        FillEntitiesInSubTree( mDeletedEntities, mNode );

        // Remove from parent's children
        auto& children = mParent->mChildren;
        auto it = std::ranges::find( children, mNode );
        if ( it != children.end() )
            children.erase( it );

        // Remove all entities from scene
        for ( auto entity : mDeletedEntities )
            mScene.RemoveEntity( entity );
    }

    void Undo() override
    {
        if ( not mParent )
            return;

        // Restore the node structure (entities are still deleted, need to restore them)
        // For now, we'll create copies of the entities
        RestoreNode( mNode );

        // Re-insert node at original index
        if ( mIndexInParent < mParent->mChildren.size() )
            mParent->mChildren.insert( mParent->mChildren.begin() + mIndexInParent, mNode );
        else
            mParent->mChildren.push_back( mNode );
    }

private:
    void RestoreNode( Ref<ProjectTreeNode>& node )
    {
        // If node has entity, create a new entity (we can't restore the exact same entity)
        if ( node->IsEntity() )
        {
            Entity newEntity = mScene.CreateEntity();
            node->mState = newEntity;
            // Note: Component data is lost. For full restoration, we'd need to serialize components
        }

        // Recursively restore children
        for ( auto& child : node->mChildren )
        {
            child->mParent = node;
            RestoreNode( child );
        }
    }

    Ref<ProjectTreeNode> mNode;
    Ref<ProjectTreeNode> mParent;
    Scene& mScene;
    set<Entity> mDeletedEntities;
    size_t mIndexInParent = 0;
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
    {}

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

// History manager
class History
{
public:
    void ExecuteCommand( std::unique_ptr<ICommand> command )
    {
        command->Execute();

        // Clear redo stack when new command is executed
        mRedoStack.clear();

        mUndoStack.push_back( std::move( command ) );

        // Limit history size
        if ( mUndoStack.size() > mMaxHistorySize )
            mUndoStack.erase( mUndoStack.begin() );
    }

    void Undo()
    {
        if ( mUndoStack.empty() )
            return;

        auto command = std::move( mUndoStack.back() );
        mUndoStack.pop_back();

        command->Undo();

        mRedoStack.push_back( std::move( command ) );
    }

    void Redo()
    {
        if ( mRedoStack.empty() )
            return;

        auto command = std::move( mRedoStack.back() );
        mRedoStack.pop_back();

        command->Execute();

        mUndoStack.push_back( std::move( command ) );
    }

    bool CanUndo() const { return not mUndoStack.empty(); }
    bool CanRedo() const { return not mRedoStack.empty(); }

    void Clear()
    {
        mUndoStack.clear();
        mRedoStack.clear();
    }

    void SetMaxHistorySize( size_t size ) { mMaxHistorySize = size; }

private:
    std::vector<std::unique_ptr<ICommand>> mUndoStack;
    std::vector<std::unique_ptr<ICommand>> mRedoStack;
    size_t mMaxHistorySize = 50;
};

} // namespace bubble
