#pragma once
#include "engine/scene/scene.hpp"
#include "engine/renderer/transform.hpp"
#include "engine/types/set.hpp"


namespace bubble
{
// Selection metadata - manages selected entities and their transforms
class Selection
{
public:
    Selection() = default;

    // Clear all selection
    void Clear();

    // Drop whatever is gone from the scene. Undo and redo move things in and
    // out of the level under the selection, so this runs after each of them.
    void Prune( const Scene& scene );

    // Just this one. What is under it follows it through the hierarchy.
    void Select( Entity entity, const Scene& scene );

    // Add entity to selection (updates group transform)
    void AddEntity( Entity entity, const Scene& scene );

    // Add multiple entities to selection
    void AddEntities( const set<Entity>& entities, const Scene& scene );

    // Remove entity from selection
    void RemoveEntity( Entity entity, const Scene& scene );

    // Query methods
    const set<Entity>& GetEntities() const { return mEntities; }

    const Transform& GetGroupTransform() const { return mGroupTransform; }
    Transform& GetGroupTransform() { return mGroupTransform; }

    bool IsEmpty() const { return mEntities.empty(); }
    size_t Count() const { return mEntities.size(); }
    bool IsSingleSelection() const { return mEntities.size() == 1; }
    bool IsMultiSelection() const { return mEntities.size() > 1; }

    // Get single entity (asserts if not exactly one selected)
    Entity GetSingleEntity() const;

    // Update group transform based on current entity positions
    void UpdateGroupTransform( const Scene& scene );

    // Apply transform delta to all selected entities

private:
    set<Entity> mEntities;
    Transform mGroupTransform;
};

} // namespace bubble
