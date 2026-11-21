#pragma once
#include "bubble/animation/animation_graph.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/editing/data_document.hpp"
#include "bubble/editor/editor_camera.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
// An animation graph played on a model, with no game playing: a world of
// its own - every module, as the animator steps only where a game would -
// with the model under a sun, its animator playing the graph as the
// document has it, saved or not. The model is the one an entity of the
// scene shown plays this graph on, with its materials, or one picked among
// the project's models with clips; the parameters are set by hand.
class GraphPreview
{
public:
    GraphPreview( EditorProject& project, const DataDocument& document );

    World& Get() { return *mWorld; }
    EditorCamera& Camera() { return mCamera; }
    // Each frame: the graph made again from the document when it changed -
    // one that does not read keeps the last that did -, the parameters set,
    // the world's frame; the camera framed on the model once it has come.
    void Frame( const DataDocument& document, f64 elapsed );

    // The models it can play on - the project's with clips -, and the one
    // it plays on; nothing while there is none.
    vector<AssetPath> Models() const;
    OptRef<const AssetPath> TryModel() const;
    void SetModel( const AssetPath& path );
    // A parameter as set here: a number, or a bool as 0 and 1; a trigger
    // fired once, at the next step.
    f32 Parameter( const string& name ) const;
    void SetParameter( const string& name, f32 value );
    void Trigger( const string& name );
    // The state the animator plays; nothing before its first step.
    opt<string> TryState() const;
    // Why the graph does not read; empty when it does.
    const string& Error() const { return mError; }

private:
    // The model an entity of the scene shown plays this graph on, with its
    // materials; the first of Models() else.
    void PickModel( const DataDocument& document );

    EditorProject& mProject;
    AssetId mGraphId = AssetId::Generate();
    AssetRef<AnimationGraph> mGraph;
    u64 mShownVersion = ~u64( 0 );
    string mError;
    opt<AssetPath> mModelMaybe;
    hmap<string, f32> mParameters;
    vector<string> mTriggers;
    EditorCamera mCamera;
    bool mFramed = false;
    // Last: its modules hold the refs above.
    Scope<World> mWorld;
    Entity mSubject = Entity::Null;
};
}
