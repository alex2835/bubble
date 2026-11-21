#pragma once
#include "bubble/editing/document.hpp"
#include "bubble/editor/editor_camera.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
// The world that shows a document: an editing world over its scene, kept
// with the document as its view - the modules that show a scene, and those
// alone, hearing each edit as data changing - and the editor's camera on
// it, each document's its own. It goes with the document, before its scene,
// and leaves the scene its data.
class DocumentWorld final : public DocumentView
{
public:
    DocumentWorld( App& app, Document& document );
    World& Get() { return *mWorld; }
    EditorCamera& Camera() { return mCamera; }
    // The first time the world has gathered its scene: the camera where
    // the scene's first camera is, if it has one.
    void PlaceCamera( const RenderScene& scene );

private:
    Scope<World> mWorld;
    EditorCamera mCamera;
    bool mPlaced = false;
};

// The document's world, made the first time it is asked for.
DocumentWorld& ViewOf( App& app, Document& document );
World& WorldOf( App& app, Document& document );
}
