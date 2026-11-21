#pragma once
#include "bubble/editor/editor_camera.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/render/debug_draw.hpp"
#include "bubble/render/gpu/renderer.hpp"
#include "bubble/types/matrix.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
// What a click on `picked` in the viewport selects: an instance of a prefab
// whole; with it or a part of it active, the part.
Entity PickTarget( const Scene& scene, const Selection& selection, Entity picked );

// What the viewport's gizmo does to the active entity.
enum class GizmoTool : u8
{
    Move,
    Turn,
    Scale,
};

// A transform's matrix back into where, how turned and how large: what the
// gizmo's matrix is written to a transform as.
struct Decomposed
{
    vec3 mPosition{ 0.0f };
    quat mRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    vec3 mScale{ 1.0f };
};
Decomposed Decompose( const mat4& matrix );

// What a gizmo's drag of the active entity, from `before` to `after` in the
// world, does to each of `selected` with a transform - the topmost of them,
// as what is under one goes with it -: moved and turned with it about the
// active one, scaled as it is along its own axes. Each the transform's own
// values to set, in its parent's space.
vector<pair<Entity, Decomposed>> MovedTogether( const Scene& scene,
                                                span<const Entity> selected,
                                                Entity active,
                                                const mat4& before,
                                                const mat4& after,
                                                GizmoTool tool );

// What a rectangle drawn over the picture takes: each of `boxes` - an
// entity and its box in the world - whose box seen through `viewProjection`
// falls in it, the rectangle and the picture in the same units.
vector<Entity> InRectangle( span<const pair<Entity, pair<vec3, vec3>>> boxes,
                            const mat4& viewProjection,
                            vec2 size,
                            vec2 from,
                            vec2 to );

// What the viewport's gizmo does to the active entity.

// The floor's grid under `camera`: on y = 0 around its focus, a line every
// step - the power of ten that makes its distance a few dozen steps across -,
// a brighter one every ten steps, the X axis red and the Z axis blue; in
// pieces that fade out fifty steps from the focus.
vector<DebugLine> FloorGrid( const EditorCamera& camera );

// The panel that shows the active document's world through the editor's
// camera: a picture the panel's size in pixels. The mouse over it drives the
// camera - the right button turns it, the middle one slides it, the wheel
// brings it closer, F frames the selection -; a click picks what is drawn
// there - an instance of a prefab first, a part of it at the second click,
// ctrl flipping it in the selection; an element of the scene's UI before
// what is behind it, one selected and placed by left and top dragged -, a
// rectangle drawn selects what is in it; the gizmo moves, turns or scales the active entity and
// the rest selected with it (W, E, R; X its axes or the world's; ctrl snaps), each drag one step; a prefab or a model
// dropped from the assets panel stands on the ground under the mouse. What is selected shows its box, its colliders, a
// light its reach, a camera its view; the scene's UI over the picture, laid
// out for its size (UI). A game played is shown through its
// own camera with its UI over it, and only looked at: the mouse and the
// keys are the game's.
class Viewport
{
public:
    Viewport( EditorProject& project, Gpu& gpu, Renderer& renderer );
    Viewport( const Viewport& ) = delete;
    Viewport& operator=( const Viewport& ) = delete;

    // Each frame, before the world's: a pick come back from the GPU
    // selected, a drag of the gizmo that ended closing its step, and what
    // the editor draws over the selection handed to the world's debug
    // drawing.
    void Prepare( OptRef<Document> documentMaybe, OptRef<World> worldMaybe );
    // Its widgets, inside an ImGui window: the picture last drawn - made
    // again here when the panel's size changed, before ImGui's lists name it
    // -, the tools, the gizmo, `camera` moved and picks made by the mouse. A
    // world with no scene document - a material's preview - is only looked
    // at.
    void Show( OptRef<Document> documentMaybe, EditorCamera& camera );
    // `world` drawn into the picture, as `camera` sees it now: after the
    // widgets, before ImGui's lists are drawn.
    void Draw( World& world, const EditorCamera& camera );

    const GpuTexture& Picture() const { return mPicture; }
    GizmoTool Tool() const { return mTool; }
    // Where the picture was last shown, in ImGui's units - the window's.
    vec2 Corner() const { return mCorner; }
    vec2 Size() const { return mSize; }

private:
    struct UiBoxShown
    {
        Entity mEntity;
        vec2 mMin{ 0.0f };
        vec2 mMax{ 0.0f };
        f32 mToCanvas = 1;
    };
    struct UiDragging
    {
        Entity mEntity;
        vec2 mFrom{ 0.0f };
        f32 mLeft = 0;
        f32 mTop = 0;
        f32 mToCanvas = 1;
    };
    void Tools();
    void Keys( Document& document, EditorCamera& camera );
    void Gizmo( Document& document, const EditorCamera& camera, vec2 corner, vec2 size );
    void Picked( Document& document, Entity entity );
    // What a rectangle over the picture took, selected - added with ctrl.
    void Rectangle( Document& document, const EditorCamera& camera, vec2 from, vec2 to );
    // The innermost UI element at a point of the picture.
    OptRef<const UiBoxShown> TryUiAt( vec2 at ) const;
    // A drag of an element going on: its left and top set, one step.
    void UiDrag( Document& document );
    void Helpers( Document& document, World& world );
    // Each light and camera of the scene marked where it stands; not a
    // camera at `eye`, looked through.
    static void Icons( const Scene& scene, DebugDraw& draw, vec3 eye );
    // The camera turned to the selection's box, near enough to fill the view.
    void Frame( Document& document, EditorCamera& camera );
    // Where the ground - the plane y = 0 - is under `mouse`, in the window's
    // units; the camera's focus when the ground is not under it.
    static vec3 GroundAt( const EditorCamera& camera, vec2 mouse, vec2 corner, vec2 size );

    EditorProject& mProject;
    Gpu& mGpu;
    Renderer& mRenderer;
    RenderView mView;
    GpuTexture mPicture;
    GizmoTool mTool = GizmoTool::Move;
    bool mLocal = false;
    // The floor's grid: asked for, and drawn - a scene's document is shown.
    bool mGrid = true;
    bool mGridShown = false;
    // The scene's UI the same way: its canvases laid out for the picture.
    bool mUi = true;
    bool mUiShown = false;
    // A game played is shown: through its camera, its UI over it.
    bool mPlayed = false;
    vec2 mCorner{ 0.0f };
    vec2 mSize{ 0.0f };
    // Where the camera was when last shown.
    vec3 mEye{ 0.0f };
    // A drag of the gizmo under way: one step of the document's history.
    bool mGizmoHeld = false;
    // A press of the left button on the picture, where: a pick if it comes
    // up close by.
    opt<vec2> mPressedAt;
    bool mPickToggles = false;
    // The world's things as the last frame gathered them: the selected ones,
    // what F frames; all of them, what a rectangle takes.
    vector<pair<Entity, pair<vec3, vec3>>> mBoxes;
    vector<pair<Entity, pair<vec3, vec3>>> mAllBoxes;
    // The screen UI's elements as last laid out over the picture, in its
    // units, and how many of their canvas's pixels one of them is: a click
    // on one picks it before what is drawn behind.
    vector<UiBoxShown> mUiBoxes;
    // A selected element dragged by its left and top: where the mouse and
    // they started.
    opt<UiDragging> mUiDragMaybe;
};
}
