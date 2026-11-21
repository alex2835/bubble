#pragma once
#include "bubble/editing/data_document.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/types/math.hpp"

namespace bubble
{
// What shows an animation graph's document: its states as nodes on a canvas
// - the any state and the return beside them -, its transitions as links
// from a node's right to another's left, each node where the document keeps
// it (editor.states, editor.any, editor.return). The mouse pans and zooms,
// adds a state from the canvas's menu, draws a transition between two nodes,
// moves nodes - several at once one step - and deletes what is selected, all
// through graph.* operators; the inspector shows what is selected. While a
// game plays, the state an animator of this graph plays there - the one
// selected in the game's tree, or the first - is lit.
class GraphView final : public DocumentView
{
public:
    explicit GraphView( EditorProject& project );
    ~GraphView() override;

    // Its widgets, inside an ImGui window, as large as what is left of it.
    void Show( DataDocument& document );

    // What is selected on the canvas: one state by its name, or one
    // transition by its position from 1.
    const string& SelectedState() const { return mSelectedState; }
    opt<size_t> SelectedTransition() const { return mSelectedTransition; }
    // A node selected - a state's, "*", "return" - as of the next Show:
    // the state the inspector renamed keeps its selection.
    void Select( string node );
    // Where a node stood when last drawn, in the canvas's units; its middle
    // on the screen, in ImGui's.
    opt<vec2> TryNodeAt( string_view node ) const;
    opt<vec2> TryNodeOnScreen( string_view node ) const;
    // The state lit as last shown, and whose it is: empty while no game
    // plays the graph.
    const string& LiveState() const { return mLiveState; }
    const string& LiveEntity() const { return mLiveEntity; }

private:
    struct Canvas;
    struct Node;

    u32 IndexOf( const string& node );
    // The state an animator of the graph plays in the game, if one does.
    void Live( const DataDocument& document );
    void Nodes( const DataDocument& document, span<const Node> nodes );
    void Links( const DataDocument& document );
    void Create();
    void Delete();
    void Menus( const DataDocument& document );
    // Nodes the mouse let go of somewhere else than the document keeps them.
    void Moved( span<const Node> nodes );
    void Run( string_view op, Value::Record args );
    void RunPending( DataDocument& document );

    EditorProject& mProject;
    Scope<Canvas> mCanvas;
    // A node's place among the canvas's ids, by its name: "*" first, then
    // "return", then each state as it was first seen.
    hmap<string, u32> mIndices;
    vector<string> mNames;
    // Where each node was put last - from the document or by the mouse -,
    // where it is drawn, and its middle on the screen.
    hmap<string, vec2> mPut;
    hmap<string, vec2> mDrawn;
    hmap<string, vec2> mOnScreen;
    u64 mShownVersion = ~u64( 0 );
    u32 mFramesShown = 0;
    string mSelectedState;
    opt<size_t> mSelectedTransition;
    string mLiveState;
    string mLiveEntity;
    string mSelect;
    vec2 mMenuAt{ 0.0f };
    string mMenuNode;
    opt<size_t> mMenuLink;
    vector<pair<string, Value::Record>> mPending;
    string mPendingStep;
};

// The graph document's view, made the first time it is asked for.
GraphView& GraphViewOf( EditorProject& project, DataDocument& document );
}
