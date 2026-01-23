#pragma once
#include "engine/project/project_tree.hpp"


namespace bubble
{
// Selection metadata - manages selected entities and their transforms
class Selection
{
public:
    Selection() = default;

    // Clear all selection
    void Clear();

    // Select a tree node (clears previous selection and fills entities from node)
    void SelectTreeNode( const Ref<ProjectTreeNode>& node, Scene& scene );

    // Add entity to selection (updates group transform)
    void AddEntity( Entity entity, Scene& scene );

    // Add multiple entities to selection
    void AddEntities( const set<Entity>& entities, Scene& scene );

    // Remove entity from selection
    void RemoveEntity( Entity entity, Scene& scene );

    // Query methods
    const set<Entity>& GetEntities() const { return mEntities; }
    const Ref<ProjectTreeNode>& GetTreeNode() const { return mProjectTreeNode; }

    const Transform& GetGroupTransform() const { return mGroupTransform; }
    Transform& GetGroupTransform() { return mGroupTransform; }

    bool IsEmpty() const { return mEntities.empty(); }
    size_t Count() const { return mEntities.size(); }
    bool IsSingleSelection() const { return mEntities.size() == 1; }
    bool IsMultiSelection() const { return mEntities.size() > 1; }

    // Get single entity (asserts if not exactly one selected)
    Entity GetSingleEntity() const;

    // Update group transform based on current entity positions
    void UpdateGroupTransform( Scene& scene );

    // Apply transform delta to all selected entities
    void ApplyTransformDelta( const vec3& positionDelta, const vec3& rotationDelta, const vec3& scaleDelta, Scene& scene );

private:
    Ref<ProjectTreeNode> mProjectTreeNode;
    set<Entity> mEntities;
    Transform mGroupTransform;
};

} // namespace bubble
