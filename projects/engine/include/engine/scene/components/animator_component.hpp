#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/animation/blend_space.hpp"
#include "engine/animation/animation_controller.hpp"

namespace bubble
{
class Animator;
struct Model;

// Plays one of the clips that came with the entity's skinned model, or a
// blend of them along a parameter - directly, from a script, or under an
// AnimationController that picks them from the script's parameters.
//
// The component is the playback state - which clip or blend, where in it, how
// fast - and nothing else; that is what the inspector edits, a scene
// serialises and a script drives. The pose and the posed vertices live in an
// Animator the engine's animation update creates for the entity and keeps in
// mAnimator. It is not copied and not serialised: a copy of the component
// starts a playback of its own, and a loaded scene poses on its first frame.
//
// Clips are named rather than indexed. Re-exporting a model reorders its
// clips more easily than it renames them, and a script says play( "walk" ).
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

    // Restarts the clip from the beginning. With a transition time the pose
    // eases into the new clip over that many seconds instead of snapping -
    // inertialized, so the old clip is not evaluated and a transition can
    // interrupt a transition. A name the model has no clip for leaves the
    // character at rest.
    void Play( string_view clip, f32 transition = 0.0f );
    // Plays a blend space under `name` - what mClip reports while it plays -
    // driven by mBlendValue. Playing the blend that is already playing
    // changes nothing, so a script may call this every frame it wants the
    // blend, the way it would test mClip before Play.
    void PlayBlend( string_view name, BlendSpace space, f32 transition = 0.0f );
    void Stop();
    bool IsPlaying() const { return mPlaying; }
    bool InTransition() const;
    bool IsBlend() const { return not mBlend.Empty(); }
    // 0..1 through the clip, or through the blend's shared cycle.
    f32 NormalizedTime( const Model& model ) const;

    // Hands playback to a controller: from the next frame it decides what
    // plays, from mParameters. Null detaches, leaving the current playback
    // as it is. The parameters take the controller's declared defaults, so
    // a script may set only the ones it drives.
    void SetController( const Ref<AnimationController>& controller );
    // The controller's current state name, or empty without one.
    string_view CurrentState() const;

    // Advances the playback by dt, poses and skins through mAnimator. The
    // engine calls this once per frame for an entity whose model is skinned.
    void Advance( const Ref<Model>& model, f32 dt );

    // The clip's name, or the blend's.
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
    // Animator. Not serialised: a transition is a moment, not a setting.
    f32 mPendingTransition = 0.0f;

    Ref<AnimationController> mController;
    // What a script writes and the controller reads; not serialised, a
    // controller starts from its defaults.
    Parameters mParameters;
    ControllerRuntime mControllerRuntime;

    Scope<Animator> mAnimator;
};

}
