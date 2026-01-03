#pragma once
#include "engine/types/string.hpp"
#include "engine/types/set.hpp"
#include "engine/scene/scene.hpp"
#include "engine/log/log.hpp"
#include <variant>

namespace bubble
{
enum class ProjectTreeNodeType
{
    Level,
    Folder,
    ModelObject,
    PhysicsObject,
    GameObject,
    Camera,
    Script,
    Light,
};

struct ProjectTreeNode
{
    using StateType = std::variant<Entity, string>;

    ProjectTreeNode();
    Ref<ProjectTreeNode> CreateChild( ProjectTreeNodeType type, StateType state );
    void RemoveNode( Ref<ProjectTreeNode> node );

    ProjectTreeNodeType Type() const { return mType; }
    bool IsEntity() const;
    Entity AsEntity() const { return std::get<Entity>( mState ); };

    vector<Ref<ProjectTreeNode>>& Children(){ return mChildren; }
    StateType& State() { return mState; }

    bool operator== ( const ProjectTreeNode& other ) { return mID == other.mID; }

private:
    static u64 mIDCounter;
    u64 mID = 0;
    ProjectTreeNodeType mType = ProjectTreeNodeType::Level;
    StateType mState = "Level"s;
    ProjectTreeNode* mParent = nullptr;
    vector<Ref<ProjectTreeNode>> mChildren;
public:
    bool mIsEditingInUI = false;
    friend Project;
};


Ref<ProjectTreeNode> FindNodeByEntity( Entity entity, const Ref<ProjectTreeNode>& node );

void FillEntitiesInSubTree( set<Entity>& entities, const Ref<ProjectTreeNode>& node );


// Cant remove root
bool RemoveNode( const Ref<ProjectTreeNode>& root, const Ref<ProjectTreeNode>& node );

// remove if present
void RemoveNodeByEntities( const Ref<ProjectTreeNode>& root, const set<Entity>& entities );


}