#pragma once
#include "bubble/editing/document.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
// The world that shows a document: an editing world over its scene, kept
// with the document as its view - the modules that show a scene, and those
// alone, hearing each edit as data changing. It goes with the document,
// before its scene, and leaves the scene its data.
class DocumentWorld final : public DocumentView
{
public:
    DocumentWorld( App& app, Document& document );
    World& Get() { return *mWorld; }

private:
    Scope<World> mWorld;
};

// The document's world, made the first time it is asked for.
World& WorldOf( App& app, Document& document );
}
