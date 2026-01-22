#pragma once
#include "engine/types/string.hpp"
#include "engine/types/set.hpp"
#include "engine/scene/scene.hpp"
#include "engine/log/log.hpp"
#include "engine/types/pointer.hpp"
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

struct ProjectTreeNode : std::enable_shared_from_this<ProjectTreeNode>
{
    using StateType = std::variant<Entity, string>;

    ProjectTreeNode();
    Ref<ProjectTreeNode> CreateChild( ProjectTreeNodeType type, StateType state );
    void RemoveNode( Ref<ProjectTreeNode> node );

    ProjectTreeNodeType Type() const { return mType; }
    bool IsEntity() const;
    Entity AsEntity() const { return std::get<Entity>( mState ); }
    opt<Entity> TryGetEntity() const;

    const vector<Ref<ProjectTreeNode>>& Children() const { return mChildren; }
    vector<Ref<ProjectTreeNode>>& Children() { return mChildren; }

    const StateType& State() const { return mState; }
    StateType& State() { return mState; }

    bool operator== ( const ProjectTreeNode& other ) const { return mID == other.mID; }

private:
    static u64 mIDCounter;
    u64 mID = 0;
    ProjectTreeNodeType mType = ProjectTreeNodeType::Level;
    StateType mState = "Level"s;
    WeakRef<ProjectTreeNode> mParent;
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