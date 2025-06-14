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
    child->mParent = this;
    child->mState = state;
    return child;
}

void ProjectTreeNode::RemoveNode( Ref<ProjectTreeNode> node )
{
    if ( not node->mParent )
    {
        LogWarning( "Impossible to remove root project node" );
        return;
    }
    auto iter = std::ranges::find_if( node->mParent->mChildren,
                                      [id = node->mID]( const Ref<ProjectTreeNode>& node ) { return node->mID == id; } );
    if ( iter == node->mParent->mChildren.end() )
        throw std::runtime_error( std::format( "Node: {} failed parent check", node->mID ) );

    while ( node->mChildren.size() > 0 )
        node->RemoveNode( node->mChildren.front() );

    node->mParent->mChildren.erase( iter );
}


bool ProjectTreeNode::IsEntity() const
{
    return mType != ProjectTreeNodeType::Level and 
           mType != ProjectTreeNodeType::Folder;
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
    for ( size_t i = 0; i < children.size(); i++ )
    {
        const auto& child = children[i];
        if ( child == node )
        {
            children.erase( children.begin() + i );
            return true;
        }
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