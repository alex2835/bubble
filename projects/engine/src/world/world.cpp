#include "bubble/world/world.hpp"
#include "bubble/core/profile.hpp"

namespace bubble
{
World::World( App& app, Scene scene, WorldOptions options )
    : mApp( app ),
      mClock( options.mStepsPerSecond ),
      mRandom( options.mSeed ),
      mScene( std::move( scene ) )
{
    mScene.ListenToFields( mFieldWatch );
    for ( const ModuleType& type : app.Modules() )
        if ( type.mCreate )
            mModules.push_back( type.mCreate( *this ) );

    // Every component there is, once: whatever a module did to the scene
    // while being made is in the scene now, so what it queued goes.
    mPending.clear();
    for ( const Name component : mHookOrder )
        for ( const Entity entity : mHooks.at( component ).mAll( mScene ) )
            Queue( DataEventKind::Added, component, entity );
    DeliverDataEvents();
}

World::~World()
{
    // The scene's runtime components go first, while the modules that built
    // them are there to let go of what they hold.
    {
        const Scene gone = std::move( mScene );
    }
}

void World::Frame( f64 elapsed )
{
    BUBBLE_PROFILE_ZONE();
    if ( not( elapsed >= 0 ) )
        throw logic_error( format( "World::Frame( {} ): time goes forward", elapsed ) );
    const u32 due = mPaused ? 0 : mClock.Advance( elapsed );
    for ( u32 i = 0; i < due; ++i )
        Step();

    DeliverDataEvents();
    const f32 dt = static_cast<f32>( elapsed );
    for ( const Phase phase : { Phase::Update, Phase::Animation, Phase::Transforms, Phase::Audio, Phase::Render } )
        Run( phase, dt );
}

void World::Step()
{
    BUBBLE_PROFILE_ZONE();
    const f32 dt = mClock.Dt();
    DeliverDataEvents();
    Run( Phase::FixedUpdate, dt );
    Run( Phase::Physics, dt );
    mEvents.Deliver();
    ++mSteps;
}

void World::AddStep( Phase phase, function<void( f32 dt )> step )
{
    if ( mRunning )
        throw logic_error( "World::AddStep while the world runs a phase" );
    mPhases[static_cast<size_t>( phase )].push_back( std::move( step ) );
}

void World::Run( Phase phase, f32 dt )
{
    mRunning = true;
    for ( const function<void( f32 )>& step : mPhases[static_cast<size_t>( phase )] )
        step( dt );
    mRunning = false;
}

World::Hooks& World::AddHooks( Name component,
                               function<bool( const Scene&, Entity )> has,
                               function<vector<Entity>( const Scene& )> all )
{
    Hooks& hooks = mHooks[component];
    hooks.mHas = std::move( has );
    hooks.mAll = std::move( all );
    hooks.mWatch = CreateScope<ComponentWatch>( *this, component );
    mHookOrder.push_back( component );
    return hooks;
}

void World::FieldWatch::FieldChanged( Entity entity, Name component, Name field )
{
    mWorld.Queue( DataEventKind::Changed, component, entity, field );
}

void World::Queue( DataEventKind kind, Name component, Entity entity, Name field )
{
    // A type no module hooks has no one to tell.
    if ( mHooks.contains( component ) )
        mPending.push_back( DataEvent{ kind, component, entity, field } );
}

void World::DeliverDataEvents()
{
    if ( mDelivering )
        throw logic_error( "World: data events delivered from inside a data event" );
    mDelivering = true;
    // By position: a module that changes the scene where it hears queues
    // more, delivered in this same pass.
    for ( size_t at = 0; at < mPending.size(); ++at )
    {
        const DataEvent event = mPending[at];
        const Hooks& hooks = mHooks.at( event.mComponent );
        switch ( event.mKind )
        {
            case DataEventKind::Added:
                if ( hooks.mHas( mScene, event.mEntity ) )
                    for ( const auto& fn : hooks.mAdded )
                        fn( event.mEntity );
                break;
            case DataEventKind::Changed:
                if ( hooks.mHas( mScene, event.mEntity ) )
                    for ( const auto& [field, fn] : hooks.mChanged )
                        if ( field == event.mField )
                            fn( event.mEntity );
                break;
            case DataEventKind::Removed:
                for ( const auto& fn : hooks.mRemoved )
                    fn( event.mEntity );
                break;
        }
    }
    mPending.clear();
    mDelivering = false;
}
}
