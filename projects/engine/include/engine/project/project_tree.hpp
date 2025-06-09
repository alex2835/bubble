#pragma once
#include "engine/types/string.hpp"
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
    // UI
    bool mEditing = false;
    friend Project;
};


Ref<ProjectTreeNode> FindNodeByEntity( Entity entity, const Ref<ProjectTreeNode>& node );

void FillProjectTreeNodeEntities( vector<Entity>& entities, const Ref<ProjectTreeNode>& node );


}