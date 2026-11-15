#include "bubble/core/jobs.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/utility.hpp"
#ifndef BUBBLE_JOBS_INLINE
#include <TaskScheduler.h>
#endif

namespace bubble
{
namespace
{
// Nobody waits for background work where it runs: the main thread hears
// what went wrong.
void RunBackground( Jobs& jobs, const function<void()>& work )
{
    try
    {
        work();
    }
    catch ( ... )
    {
        jobs.RunOnMain( [error = current_exception()] { rethrow_exception( error ); } );
    }
}
}

// Where jobs run: threads, or the main thread alone.
struct Jobs::Pool
{
    virtual ~Pool() = default;
    virtual u32 Threads() const = 0;
    virtual void ParallelFor( u32 count, u32 grain, const function<void( u32, u32 )>& fn ) = 0;
    virtual void Background( function<void()> work ) = 0;
    // The main thread's turn, at RunMainQueue.
    virtual void MainTurn() = 0;
};

namespace
{
// Everything on the main thread: the web without threads, and Jobs( 1 ) -
// a test that wants the order of things fixed, a machine of one core.
class MainThreadPool final : public Jobs::Pool
{
public:
    explicit MainThreadPool( Jobs& jobs ) : mJobs( jobs ) {}

    // As a pool waits for its threads.
    ~MainThreadPool() override { MainTurn(); }

    u32 Threads() const override { return 1; }

    void ParallelFor( u32 count, u32, const function<void( u32, u32 )>& fn ) override
    {
        if ( count > 0 )
            fn( 0, count );
    }

    // Run when the main queue is: what a thread would have got to by then.
    void Background( function<void()> work ) override { mBackground.push_back( std::move( work ) ); }

    // The background work waiting runs. Taken first - it may add more, for
    // the next turn.
    void MainTurn() override
    {
        const vector<function<void()>> due = exchange( mBackground, {} );
        for ( const function<void()>& work : due )
            RunBackground( mJobs, work );
    }

private:
    Jobs& mJobs;
    vector<function<void()>> mBackground;
};
}

#ifndef BUBBLE_JOBS_INLINE
// A background job's task: enkiTS keeps a pointer to it until it is done,
// so it lives on the heap, owned by the Pool, and goes once it is complete.
class BackgroundTask final : public enki::ITaskSet
{
public:
    BackgroundTask( Jobs& jobs, function<void()> work ) : mJobs( jobs ), mWork( std::move( work ) )
    {
        // Below a phase's work, so a thread waiting on a phase never picks
        // it up.
        m_Priority = enki::TASK_PRIORITY_LOW;
    }

    void ExecuteRange( enki::TaskSetPartition, uint32_t ) override
    {
        BUBBLE_PROFILE_ZONE();
        RunBackground( mJobs, mWork );
    }

private:
    Jobs& mJobs;
    function<void()> mWork;
};

namespace
{
class ThreadPool final : public Jobs::Pool
{
public:
    ThreadPool( Jobs& jobs, u32 threads ) : mJobs( jobs ) { mScheduler.Initialize( threads ); }

    // Everything finishes before the tasks it runs go.
    ~ThreadPool() override { mScheduler.WaitforAllAndShutdown(); }

    u32 Threads() const override { return mScheduler.GetNumTaskThreads(); }

    void ParallelFor( u32 count, u32 grain, const function<void( u32, u32 )>& fn ) override
    {
        mutex errorLock;
        exception_ptr error;
        // enkiTS takes its own minimum range as a hint and cuts finer, so
        // what it splits is whole pieces `grain` long.
        const u32 pieces = count / grain + ( count % grain == 0 ? 0 : 1 );
        enki::TaskSet task( pieces, [&]( enki::TaskSetPartition range, uint32_t ) {
            try
            {
                const u64 end = min( u64{ range.end } * grain, u64{ count } );
                fn( range.start * grain, static_cast<u32>( end ) );
            }
            catch ( ... )
            {
                const lock_guard lock( errorLock );
                if ( not error )
                    error = current_exception();
            }
        } );
        task.m_Priority = enki::TASK_PRIORITY_HIGH;
        mScheduler.AddTaskSetToPipe( &task );
        mScheduler.WaitforTask( &task, enki::TASK_PRIORITY_HIGH );
        if ( error )
            rethrow_exception( error );
    }

    void Background( function<void()> work ) override
    {
        mBackground.push_back( CreateScope<BackgroundTask>( mJobs, std::move( work ) ) );
        mScheduler.AddTaskSetToPipe( mBackground.back().get() );
    }

    // The tasks done go.
    void MainTurn() override
    {
        erase_if( mBackground, []( const Scope<BackgroundTask>& task ) { return task->GetIsComplete(); } );
    }

private:
    Jobs& mJobs;
    enki::TaskScheduler mScheduler;
    // The main thread's: Background adds, MainTurn drops what is done.
    vector<Scope<BackgroundTask>> mBackground;
};
}
#endif

namespace
{
Scope<Jobs::Pool> MakePool( Jobs& jobs, u32 threads )
{
#ifdef BUBBLE_JOBS_INLINE
    static_cast<void>( threads );
    return CreateScope<MainThreadPool>( jobs );
#else
    const u32 count = threads == 0 ? enki::GetNumHardwareThreads() : threads;
    // One thread is the main one, with no pool to hand work to.
    if ( count <= 1 )
        return CreateScope<MainThreadPool>( jobs );
    return CreateScope<ThreadPool>( jobs, count );
#endif
}
}

Jobs::Jobs( u32 threads ) : mMain( this_thread::get_id() ), mPool( MakePool( *this, threads ) )
{
}

Jobs::~Jobs() = default;

u32 Jobs::Threads() const
{
    return mPool->Threads();
}

void Jobs::ParallelFor( u32 count, u32 grain, const function<void( u32 begin, u32 end )>& fn )
{
    BUBBLE_PROFILE_ZONE();
    if ( grain == 0 )
        throw logic_error( "Jobs::ParallelFor in ranges of 0" );
    if ( count > 0 )
        mPool->ParallelFor( count, grain, fn );
}

void Jobs::Background( function<void()> work )
{
    RequireMain( "Background" );
    mPool->Background( std::move( work ) );
}

void Jobs::RunOnMain( function<void()> work )
{
    const lock_guard lock( mQueueLock );
    mQueue.push_back( std::move( work ) );
}

void Jobs::RunMainQueue()
{
    BUBBLE_PROFILE_ZONE();
    RequireMain( "RunMainQueue" );
    mPool->MainTurn();
    vector<function<void()>> due;
    {
        const lock_guard lock( mQueueLock );
        due.swap( mQueue );
    }
    for ( size_t at = 0; at < due.size(); ++at )
    {
        try
        {
            due[at]();
        }
        catch ( ... )
        {
            // What has not run waits for the next call, ahead of what came
            // meanwhile.
            const lock_guard lock( mQueueLock );
            mQueue.insert( mQueue.begin(), due.begin() + static_cast<i64>( at + 1 ), due.end() );
            throw;
        }
    }
}

void Jobs::RequireMain( string_view what ) const
{
    if ( this_thread::get_id() != mMain )
        throw logic_error( format( "Jobs::{} off the main thread", what ) );
}
}
