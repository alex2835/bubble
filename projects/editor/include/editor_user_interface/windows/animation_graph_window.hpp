#pragma once
#include "editor_user_interface/windows/window_base.hpp"
#include "engine/animation/animation_controller.hpp"
#include "engine/types/map.hpp"
#include "engine/types/set.hpp"
#include <deque>

namespace ax::NodeEditor { struct EditorContext; }

namespace bubble
{
// An animation controller as a graph: states are nodes, transitions are
// links, with an "any state" node for transitions from "*" and a "return"
// node for those going back. One tab per machine - the base and each layer.
//
// The window edits a draft copy of the controller and writes the file on
// Save; the editor's hot reloader then swaps the saved file into every
// animator on it, which finds its state again by name. The file is the
// document; the graph is a view of it, and node positions ride along in it
// under "editor". While an entity in the scene runs the controller, its
// current state and the transitions whose conditions hold are lit up.
class AnimationGraphWindow : public UserInterfaceWindowBase
{
public:
    AnimationGraphWindow( BubbleEditor& editor );
    ~AnimationGraphWindow();

    string_view Name();
    void OnUpdate( DeltaTime dt );
    void OnDraw( DeltaTime dt );

private:
    // Takes a controller from the loader as the document.
    void Open( const Ref<AnimationController>& controller );
    void Close();
    void Save();
    // The draft's machine on the current tab: the base, or a layer's.
    StateMachine& Machine();
    const StateMachine& Machine() const;
    // The key a state's node position is filed under.
    string PositionKey( const string& state ) const;
    // Lays out the current tab's nodes from the saved positions, or a grid.
    void PlaceNodes();
    // Reads the current tab's node positions back into the draft.
    void StorePositions();

    void DrawToolbar();
    void DrawTabs();
    void DrawGraph();
    void DrawNode( i32 state );
    void HandleCreate();
    void HandleDelete();
    void HandleContextMenus();
    void DrawSidebar();
    void DrawParameters();
    void DrawLayerSettings();
    void DrawStateProperties( i32 state );
    void DrawTransitionProperties( i32 transition );
    // The live entity's playback - parameters, state, time, pause, events -
    // for trying the controller out. Runtime state only: nothing here is
    // saved, to the .anim or to the scene.
    void DrawPreview();

    void AddState( string name, const ImVec2& position );
    void RemoveState( i32 state );
    void AddTransition( i32 from, i32 to );
    // The names the model behind this controller has, if an entity in the
    // scene carries one: what the clip combos offer.
    vector<string> ClipNames() const;
    vector<string> JointNames() const;
    // The entity the preview shows: the selected one if it runs this
    // controller, otherwise the first in the scene that does.
    Entity LiveEntity() const;
    // Its Playback on the current tab; null when there is none.
    struct Playback* LivePlayback() const;
    struct AnimatorComponent* LiveAnimator() const;

    Ref<AnimationController> mSource;
    AnimationController mDraft;
    bool mDirty = false;
    // -1 is the base machine, otherwise a layer index.
    i32 mTab = -1;
    // One node editor per tab: node ids are state indices and would collide.
    map<i32, ax::NodeEditor::EditorContext*> mEditors;
    // Tabs whose nodes have been placed since the last Open.
    set<i32> mPlaced;

    i32 mSelectedState = -1;
    i32 mSelectedTransition = -1;
    // Scratch for the sidebar's text fields.
    string mNewParameter;
    int mNewParameterType = 0;
    string mNewLayer;
    string mRename;
    i32 mRenaming = -1;
    // The live entity's latest clip events, newest last.
    std::deque<string> mRecentEvents;
};

}
