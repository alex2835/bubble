#pragma once
#include "bubble/scene/scene.hpp"
#include "bubble/types/containers.hpp"

namespace bubble
{
// What is selected in a document: entities in the order they were picked,
// the last one active - the one the inspector shows and the gizmo sits on.
// Not an edit: undo takes back the scene, not the clicks.
//
// An entity that goes is kept, only not shown: undo brings it back under its
// own number, and it is selected again.
class Selection
{
public:
    explicit Selection( const Scene& scene ) : mScene( scene ) {}
    Selection( const Selection& ) = delete;
    Selection& operator=( const Selection& ) = delete;

    // Each takes an entity of the tree; another is a logic_error.

    // `entity` alone.
    void Select( Entity entity );
    // `entity` with the rest, and active; it may have been selected already.
    void Add( Entity entity );
    void Remove( Entity entity );
    // Removes `entity` if it is selected and adds it if not: a click with
    // ctrl held.
    void Toggle( Entity entity );
    void Clear();

    bool Has( Entity entity ) const;
    bool Empty() const { return All().empty(); }
    // In the order they were picked, the active one last.
    vector<Entity> All() const;
    // Entity::Null when nothing is selected.
    Entity Active() const;
    // Those not under another selected one, in the order of the tree: what a
    // delete or a copy takes - /props with /props/chair is /props alone.
    vector<Entity> TopLevel() const;

private:
    bool Shown( Entity entity ) const;
    void Require( Entity entity, string_view what ) const;

    const Scene& mScene;
    vector<Entity> mPicked;
};
}
