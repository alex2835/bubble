#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/animation/blend_space.hpp"
#include "engine/animation/animation_controller.hpp"

namespace bubble
{
class Animator;
struct Model;

// One stream of playback: a clip or a blend space, where in it, how fast,
// and - under a controller - the state machine that chooses them. The base
// of an AnimatorComponent is one; each overlay layer is another.
//
// Clips are named rather than indexed. Re-exporting a model reorders its
// clips more easily than it renames them, and a script says play( "walk" ).
struct Playback
{
    // Restarts the clip from the beginning. With a transition time the pose
    // eases into the new clip over that many seconds instead of snapping -
    // inertialized, so the old clip is not evaluated and a transition can
    // interrupt a transition. A name the model has no clip for leaves the
    // stream at rest.
    void Play( string_view clip, f32 transition = 0.0f );
    // Plays a blend space under `name` - what mClip reports while it plays -
    // driven by mBlendValue. Playing the blend that is already playing
    // changes nothing, so a script may call this every frame it wants the
    // blend, the way it would test mClip before Play.
    void PlayBlend( string_view name, BlendSpace space, f32 transition = 0.0f );
    // Plays nothing: rest on the base; on an overlay, the clip stays while
    // the layer's weight fades out over the transition, then nothing.
    void PlayNothing( f32 transition = 0.0f );
    void Stop() { mPlaying = false; }
    bool IsBlend() const { return not mBlend.Empty(); }
    bool IsEmpty() const { return mClip.empty() or mFadingOut; }

    // The clip's name, or the blend's; empty for nothing.
    string mClip;
    // Seconds into the clip; for a blend, the phase 0..1 through the cycle
    // every clip in it shares.
    f32 mTime = 0.0f;
    f32 mSpeed = 1.0f;
    bool mLoop = true;
    bool mPlaying = true;
    // Set while a blend plays; its parameter is mBlendValue.
    BlendSpace mBlend;
    f32 mBlendValue = 0.0f;

    // A Play with a transition time, until the update hands it to the
    // track; and the last one handed over, which is how long a layer's
    // weight takes to follow a state change. Not serialised: a transition
    // is a moment, not a setting.
    f32 mPendingTransition = 0.0f;
    f32 mLastTransition = 0.0f;
    // PlayNothing was called: the clip is kept for the fade, and IsEmpty.
    bool mFadingOut = false;

    // Under a controller: where its state machine is, and the state entered
    // by the last update, if one was.
    ControllerRuntime mRuntime;
    string mEnteredState;
};


// A stream played over the base on part of the skeleton.
struct OverlayLayer
{
    string mName;
    // Joint names, each with its subtree; "!name" takes one back out.
    vector<string> mMask;
    // The weight the layer is asked for...
    f32 mWeight = 1.0f;
    string mWeightParameter;
    // ...and the one it shows, which follows it at the pace of the last
    // transition, and is zero while the layer plays nothing. That is what
    // fades a one-shot wave out instead of cutting it.
    f32 mShownWeight = 0.0f;
    Playback mPlayback;
};


// Plays one of the clips that came with the entity's skinned model, or a
// blend of them along a parameter - directly, from a script, or under an
// AnimationController that picks them from the script's parameters - with
// overlay layers on parts of the skeleton over it.
//
// The component is the playback state and nothing else; that is what the
// inspector edits, a scene serialises and a script drives. The pose and the
// posed vertices live in an Animator the engine's animation update creates
// for the entity and keeps in mAnimator. It is not copied and not
// serialised: a copy of the component starts a playback of its own, and a
// loaded scene poses on its first frame.
struct AnimatorComponent
{
    static int ID() { return static_cast<int>( ComponentID::Animator ); }
    static string_view Name() { return "Animator"sv; }

    static void OnComponentDraw( InspectorContext& ctx, const Entity& entity, AnimatorComponent& component );
    static void ToJson( json& json, const Project& project, const AnimatorComponent& component );
    static void FromJson( const json& json, Project& project, AnimatorComponent& component );
    static void CreateLuaBinding( sol::state& lua );

public:
    AnimatorComponent();
    ~AnimatorComponent();
    // Note that recs relocates components with memmove (recs/pool.hpp), so the
    // move operations below only run for explicit moves in engine code. The
    // Animator is held by pointer, which relocates bitwise.
    AnimatorComponent( const AnimatorComponent& );
    AnimatorComponent& operator=( const AnimatorComponent& );
    AnimatorComponent( AnimatorComponent&& ) noexcept;
    AnimatorComponent& operator=( AnimatorComponent&& ) noexcept;

    // The base stream's, for the common case.
    void Play( string_view clip, f32 transition = 0.0f ) { mBase.Play( clip, transition ); }
    void PlayBlend( string_view name, BlendSpace space, f32 transition = 0.0f ) { mBase.PlayBlend( name, std::move( space ), transition ); }
    void Stop() { mBase.Stop(); }
    bool IsPlaying() const { return mBase.mPlaying; }
    bool IsBlend() const { return mBase.IsBlend(); }
    bool InTransition() const;

    // An overlay layer by name, made if there is none: its mask and weight
    // set, its playback left as it was.
    OverlayLayer& Layer( string_view name, vector<string> mask, f32 weight );
    OverlayLayer* FindLayer( string_view name );

    // Hands playback to a controller: from the next frame it decides what
    // plays, from mParameters, on the base and on the layers it declares
    // (which replace any the script made). Null detaches, leaving the
    // current playback as it is. The parameters take the controller's
    // declared defaults, so a script may set only the ones it drives.
    void SetController( const Ref<AnimationController>& controller );
    // The controller's current base state name, or empty without one.
    string_view CurrentState() const;

    // Marks a moment in a clip for events(), on top of whatever the
    // controller declares. Normalized time.
    void AddEvent( string_view clip, f32 time, string name );

    // Advances every stream by dt, poses and skins through mAnimator. The
    // engine calls this once per frame for an entity whose model is skinned.
    void Advance( const Ref<Model>& model, f32 dt );

    Playback mBase;
    vector<OverlayLayer> mLayers;

    Ref<AnimationController> mController;
    // What a script writes and the controller reads; not serialised, a
    // controller starts from its defaults.
    Parameters mParameters;

    // Events added with AddEvent, beside the controller's.
    ClipEvents mEvents;
    // What the last Advance produced, for a script to poll on its next
    // update: the clip events crossed on any stream.
    vector<string> mFiredEvents;

    Scope<Animator> mAnimator;
};

}
