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
    // Each frame, once the world has gathered what it shows and before it
    // is drawn. The camera is placed the first time - where a level's first
    // camera is, or around what the scene shows once its models have come -,
    // and a prefab with no light of its own is lit by the editor's sun,
    // which the scene never holds.
    void Gathered();

private:
    Document& mDocument;
    Scope<World> mWorld;
    EditorCamera mCamera;
    bool mPlaced = false;
    // Frames waited for models to come before the camera is placed.
    u32 mWaited = 0;
};

// What lights a prefab that has no light: from above, over the left
// shoulder of a camera looking down -Z.
RenderLight EditorSun();

// The document's world, made the first time it is asked for.
DocumentWorld& ViewOf( App& app, Document& document );
World& WorldOf( App& app, Document& document );
}
