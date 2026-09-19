#pragma once
#include "engine/scene/components/component_base.hpp"

namespace bubble
{
class Animator;

// Plays one of the clips that came with the entity's skinned model.
//
// The component is the playback state - which clip, where in it, how fast -
// and nothing else; that is what the inspector edits, a scene serialises and a
// script drives. The pose and the posed vertices live in an Animator the
// engine's animation update creates for the entity and keeps in mAnimator.
// It is not copied and not serialised: a copy of the component starts a
// playback of its own, and a loaded scene poses on its first frame.
//
// A clip is named rather than indexed. Re-exporting a model reorders its
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
    void Stop();
    bool IsPlaying() const { return mPlaying; }
    bool InTransition() const;

    string mClip;
    f32 mTime = 0.0f;
    f32 mSpeed = 1.0f;
    bool mLoop = true;
    bool mPlaying = true;

    // A Play with a transition time, until the update hands it to the
    // Animator. Not serialised: a transition is a moment, not a setting.
    f32 mPendingTransition = 0.0f;

    Scope<Animator> mAnimator;
};

}
