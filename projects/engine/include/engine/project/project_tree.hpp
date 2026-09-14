#pragma once
#include "engine/types/string.hpp"
#include "engine/types/set.hpp"
#include "engine/scene/scene.hpp"
#include "engine/log/log.hpp"
#include "engine/types/pointer.hpp"
#include <variant>

namespace bubble
{
class Level;

enum class ProjectTreeNodeType
{
    Root, // the one node a level's tree hangs from; serialized as "Level" in old files
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

    explicit ProjectTreeNode( u64& idCounter );
    Ref<ProjectTreeNode> CreateChild( ProjectTreeNodeType type, StateType state );
    static void RemoveNode( Ref<ProjectTreeNode> node, Scene& scene );
    static Ref<ProjectTreeNode> CopyNode( const Ref<ProjectTreeNode>& node, Scene& scene );

    ProjectTreeNodeType Type() const { return mType; }
    // Unique within the level and stable across a session: how an operator
    // argument names a node.
    u64 ID() const { return mID; }
    bool IsEntity() const;
    Entity AsEntity() const { return std::get<Entity>( mState ); }
    opt<Entity> TryGetEntity() const;

    const StateType& State() const { return mState; }
    StateType& State() { return mState; }

    bool operator== ( const ProjectTreeNode& other ) const { return mID == other.mID; }

private:
    u64 mID = 0;
    u64* mIDCounter = nullptr; // non-owning ref back to Level::mNodeIDCounter
public:
    ProjectTreeNodeType mType = ProjectTreeNodeType::Root;
    StateType mState = "Level"s;
    WeakRef<ProjectTreeNode> mParent;
    vector<Ref<ProjectTreeNode>> mChildren;
    bool mIsEditingInUI = false;
    friend Level;
};


Ref<ProjectTreeNode> FindNodeByEntity( Entity entity, const Ref<ProjectTreeNode>& node );
Ref<ProjectTreeNode> FindNodeById( u64 id, const Ref<ProjectTreeNode>& node );

void FillEntitiesInSubTree( set<Entity>& entities, const Ref<ProjectTreeNode>& node );


// Cant remove root
bool RemoveNode( const Ref<ProjectTreeNode>& root, const Ref<ProjectTreeNode>& node );

// remove if present
void RemoveNodeByEntities( const Ref<ProjectTreeNode>& root, const set<Entity>& entities );


}