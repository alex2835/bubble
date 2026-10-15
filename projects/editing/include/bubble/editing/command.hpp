#pragma once
#include "bubble/scene/scene.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
// One change to a document's scene that can be taken back: a field set, an
// entity made, removed, moved or renamed, a component added or removed. The
// edits in bubble/editing/edits.hpp make them, History keeps them.
//
// A command names entities by Entity. Undo brings a removed entity back
// under its own number (Scene::Revive), so a command made before the removal
// still means the same entity after it. What a command needs to go back it
// owns - copies, never a reference into the scene's storage, which moves.
class Command
{
public:
    virtual ~Command() = default;

    // Makes the change, the first time. It may find it cannot, and then it
    // changes nothing and says why.
    virtual expected<void, string> TryDo( Scene& scene ) = 0;
    // Takes the change back; the scene is as TryDo or Redo left it.
    virtual void Undo( Scene& scene ) = 0;
    // Makes the change again after Undo. The scene is as it was before
    // TryDo, so it cannot fail. Unless overridden it is TryDo over again -
    // a command whose second time differs from its first says so here.
    virtual void Redo( Scene& scene );
    // Whether `next`, done right after this one, carries the same change on -
    // the same field set again while a gizmo is dragged - and this one took
    // it in: it now ends where `next` does, and `next` is dropped.
    virtual bool Absorb( const Command& next );
};
}
