#include "engine/pch/pch.hpp"
#include "engine/project/project_tree.hpp"

namespace bubble
{
u64 ProjectTreeNode::mIDCounter = 0;


ProjectTreeNode::ProjectTreeNode()
    : mID( mIDCounter++ )
{

}

Ref<ProjectTreeNode> ProjectTreeNode::CreateChild( ProjectTreeNodeType type, StateType state )
{
    auto& child = mChildren.emplace_back( CreateRef<ProjectTreeNode>() );
    child->mType = type;
    child->mParent = weak_from_this();
    child->mState = state;
    return child;
}

void ProjectTreeNode::RemoveNode( Ref<ProjectTreeNode> node )
{
    auto parent = node->mParent.lock();
    if ( not parent )
    {
        LogWarning( "Impossible to remove root project node" );
        return;
    }

    auto iter = std::ranges::find_if( parent->mChildren,
                                      [id = node->mID]( const Ref<ProjectTreeNode>& node ) { return node->mID == id; } );
    if ( iter == parent->mChildren.end() )
        throw std::runtime_error( std::format( "Node: {} failed parent check", node->mID ) );

    // Recursively remove all children - optimized by clearing vector
    node->mChildren.clear();

    parent->mChildren.erase( iter );
}


bool ProjectTreeNode::IsEntity() const
{
    return mType != ProjectTreeNodeType::Level and
           mType != ProjectTreeNodeType::Folder;
}

opt<Entity> ProjectTreeNode::TryGetEntity() const
{
    if ( auto* e = std::get_if<Entity>( &mState ) )
        return *e;
    return std::nullopt;
}



Ref<ProjectTreeNode> FindNodeByEntity( Entity entity, const Ref<ProjectTreeNode>& node )
{
    if ( node->IsEntity() and entity == node->AsEntity() )
        return node;

    for ( const auto& child : node->Children() )
    {
        auto res = FindNodeByEntity( entity, child );
        if ( res )
            return res;
    }
    return nullptr;
}

void FillEntitiesInSubTree( set<Entity>& entities, const Ref<ProjectTreeNode>& node )
{
    if ( node->IsEntity() )
        entities.insert( node->AsEntity() );

    for ( const auto& child : node->Children() )
        FillEntitiesInSubTree( entities, child );
}

bool RemoveNode( const Ref<ProjectTreeNode>& root, const Ref<ProjectTreeNode>& node )
{
    auto& children = root->Children();

    // Try to find node directly in children
    auto it = std::ranges::find( children, node );
    if ( it != children.end() )
    {
        children.erase( it );
        return true;
    }

    // Recursively search in children's subtrees
    for ( const auto& child : children )
    {
        if ( RemoveNode( child, node ) )
            return true;
    }

    return false;
}

void RemoveNodeByEntities( const Ref<ProjectTreeNode>& root, const set<Entity>& entitiesToRemove )
{
    auto& children = root->Children();
    for ( size_t i = 0; i < children.size(); )
    {
        const auto& child = children[i];
        if ( child->IsEntity() and entitiesToRemove.contains( child->AsEntity() ) )
        {
            children.erase( children.begin() + i );
        }
        else
        {
            if ( not child->IsEntity() )
                RemoveNodeByEntities( child, entitiesToRemove );
            i++;
        }
    }

}

}