#pragma once
#include "bubble/core/random.hpp"
#include "bubble/scene/scene.hpp"
#include "bubble/types/format.hpp"
#include "bubble/world/app.hpp"
#include "bubble/world/events.hpp"
#include "bubble/world/fixed_clock.hpp"
#include "bubble/world/services.hpp"

namespace bubble
{
// Where a module's work goes in the frame. The order is World's alone, one
// list in world.cpp: each fixed step runs FixedUpdate, then Physics, then
// delivers events; once a frame, after the fixed steps, come Update,
// Animation, Transforms, Audio and Render. Within a phase, work runs in the
// order it was added - the order of App's modules.
enum class Phase : u8
{
    // Each fixed step, with the same dt every time.
    FixedUpdate, // game logic: Luau's on_fixed_update
    Physics,     // one physics step
             // Once a frame, with the frame's time.
    Update, // Luau's on_update, the camera
    Animation,
    Transforms, // between the last two steps, by Alpha()
    Audio,
    Render,
};

struct WorldOptions
{
    u32 mStepsPerSecond = 60;
    // Where GetRandom() - and, later, Luau's math.random - start from: the
    // same seed and the same input play the same game.
    u64 mSeed = 0;
};

// One live scene and its modules: the game, a prefab's preview, a test,
// each with physics and Luau of its own. What happens in it follows from
// the scene, the seed and the input alone - fixed steps, one order of work,
// a seeded Random - so the same input plays the same game at any frame rate.
class World
{
public:
    // Takes the scene - a level loaded, the editor's copy of one - and makes
    // App's modules for it, in order. They hear of every component the scene
    // has before the constructor returns.
    World( App& app, Scene scene, WorldOptions options = {} );
    ~World();
    World( const World& ) = delete;
    World& operator=( const World& ) = delete;

    App& GetApp() { return mApp; }
    Scene& GetScene() { return mScene; }
    Events& GetEvents() { return mEvents; }
    Services& GetServices() { return mServices; }
    Random& GetRandom() { return mRandom; }
    // A service by its type: world.Get<PhysicsWorld>().
    template <typename T>
    T& Get()
    {
        return mServices.Get<T>();
    }

    // A frame `elapsed` real seconds after the last: the fixed steps that
    // time makes due, then the frame's own phases. Negative or NaN time is a
    // logic_error.
    void Frame( f64 elapsed );
    // One fixed step now, paused or not: the editor's "step" button.
    void Step();
    // Paused, a frame runs its own phases but no fixed steps: logic and
    // physics stand, and a menu still works.
    void SetPaused( bool paused ) { mPaused = paused; }
    bool Paused() const { return mPaused; }

    // Fixed steps done since the world was made.
    u64 Steps() const { return mSteps; }
    f32 FixedDt() const { return mClock.Dt(); }
    f32 Alpha() const { return mClock.Alpha(); }

    // Data events: a module hears components of T come, change and go - T
    // being described in App's reflection. They are delivered at the start
    // of each fixed step and of the frame's phases, in the order they
    // happened, rather than in the middle of a change: a module sees an
    // entity whole and may change the scene where it hears. An Added or a
    // Changed whose component is gone by then is not delivered; a Removed
    // always is, and the module drops what it built, if anything. A
    // component replaced whole is Added again.
    template <typename T>
    void OnAdded( function<void( Entity )> fn )
    {
        HooksOf<T>().mAdded.push_back( std::move( fn ) );
    }

    template <typename T>
    void OnRemoved( function<void( Entity )> fn )
    {
        HooksOf<T>().mRemoved.push_back( std::move( fn ) );
    }

    // A field of a T set, as ComponentTypes::TrySetField tells it: by the
    // path's first step, so light.shadow.bias is heard as light.shadow. A
    // field T does not have is a logic_error.
    //
    // Hooks are added when a module is made; adding one from inside a data
    // event is a logic_error.
    template <typename T>
    void OnChanged( Name field, function<void( Entity )> fn )
    {
        Hooks& hooks = HooksOf<T>();
        if ( not mApp.GetReflection().TryFind<T>()->data( field.Hash() ) )
            throw logic_error(
                format( "World::OnChanged: {} has no field {}", entt::type_name<T>::value(), field.ToString() ) );
        hooks.mChanged.push_back( { field, std::move( fn ) } );
    }

    // Work for a phase, after what it has. Adding while the world runs a
    // phase is a logic_error: modules add theirs when they are made.
    void AddStep( Phase phase, function<void( f32 dt )> step );

private:
    enum class DataEventKind : u8
    {
        Added,
        Changed,
        Removed,
    };

    struct DataEvent
    {
        DataEventKind mKind = DataEventKind::Added;
        Name mComponent;
        Entity mEntity;
        Name mField;
    };

    // Hears one type's components come and go, for the world to deliver later.
    class ComponentWatch final : public ComponentListener
    {
    public:
        ComponentWatch( World& world, Name component ) : mWorld( world ), mComponent( component ) {}
        void Added( Entity entity ) override { mWorld.Queue( DataEventKind::Added, mComponent, entity ); }
        void Replaced( Entity entity ) override { mWorld.Queue( DataEventKind::Added, mComponent, entity ); }
        void Removed( Entity entity ) override { mWorld.Queue( DataEventKind::Removed, mComponent, entity ); }

    private:
        World& mWorld;
        Name mComponent;
    };

    class FieldWatch final : public FieldListener
    {
    public:
        explicit FieldWatch( World& world ) : mWorld( world ) {}
        void FieldChanged( Entity entity, Name component, Name field ) override;

    private:
        World& mWorld;
    };

    // Who hears one type's components, and how to ask the scene about them.
    struct Hooks
    {
        function<bool( const Scene&, Entity )> mHas;
        function<vector<Entity>( const Scene& )> mAll;
        Scope<ComponentWatch> mWatch;
        vector<function<void( Entity )>> mAdded;
        vector<function<void( Entity )>> mRemoved;
        vector<pair<Name, function<void( Entity )>>> mChanged;
    };

    template <typename T>
    Hooks& HooksOf()
    {
        if ( mDelivering )
            throw logic_error( "World: a data event hook added from inside a data event" );
        const auto typeMaybe = mApp.GetReflection().TryFind<T>();
        if ( not typeMaybe )
            throw logic_error(
                format( "World: data events of {}, which reflection has not described", entt::type_name<T>::value() ) );
        const Name component = Name::FromHash( typeMaybe->id() );
        if ( const auto found = mHooks.find( component ); found != mHooks.end() )
            return found->second;
        Hooks& hooks = AddHooks(
            component, []( const Scene& scene, Entity entity ) { return scene.Has<T>( entity ); },
            []( const Scene& scene ) { return scene.EntitiesWith<T>(); } );
        mScene.Listen<T>( *hooks.mWatch );
        return hooks;
    }

    Hooks& AddHooks( Name component,
                     function<bool( const Scene&, Entity )> has,
                     function<vector<Entity>( const Scene& )> all );
    void Queue( DataEventKind kind, Name component, Entity entity, Name field = {} );
    void DeliverDataEvents();
    void Run( Phase phase, f32 dt );

    static constexpr size_t cPhases = static_cast<size_t>( Phase::Render ) + 1;

    App& mApp;
    FixedClock mClock;
    Random mRandom;
    Scene mScene;
    Events mEvents;
    Services mServices;
    // Node-based: a Hooks stays where it is while others are added.
    hmap<Name, Hooks> mHooks;
    // The types in the order they were first hooked: the order the
    // components there are at the start are told in.
    vector<Name> mHookOrder;
    FieldWatch mFieldWatch{ *this };
    vector<DataEvent> mPending;
    bool mDelivering = false;
    array<vector<function<void( f32 )>>, cPhases> mPhases;
    bool mRunning = false;
    bool mPaused = false;
    u64 mSteps = 0;
    // Made last, gone first: the rest of the world is there for them both
    // ways.
    vector<Scope<Module>> mModules;
};
}
