// Jobs: the process's one pool of threads. On the web there are none, and
// everything runs on the main thread - the tests say where that differs.
#include "bubble/core/jobs.hpp"
#include "bubble/types/utility.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
#ifdef __EMSCRIPTEN__
constexpr bool cThreads = false;
#else
constexpr bool cThreads = true;
#endif

// Runs the main queue until `done`, or gives up after a good while.
void RunUntil( Jobs& jobs, const function<bool()>& done )
{
    for ( u32 i = 0; i < 5000 and not done(); ++i )
    {
        jobs.RunMainQueue();
        this_thread::sleep_for( chrono::milliseconds( 1 ) );
    }
}
}

TEST_CASE( "ParallelFor covers every index once, in ranges at least the grain long but the last" )
{
    for ( const u32 threads : { 1u, 4u } )
    {
        Jobs jobs( threads );
        CHECK( jobs.Threads() == ( cThreads ? threads : 1 ) );
        // Each range writes its own indices: no two threads share one.
        vector<u32> hits( 1000, 0 );
        mutex lock;
        vector<pair<u32, u32>> ranges;
        jobs.ParallelFor( 1000, 16, [&]( u32 begin, u32 end ) {
            for ( u32 i = begin; i < end; ++i )
                ++hits[i];
            const lock_guard guard( lock );
            ranges.emplace_back( begin, end );
        } );
        CHECK( hits == vector<u32>( 1000, 1 ) );
        for ( const auto& [begin, end] : ranges )
            CHECK( ( end - begin >= 16 or end == 1000 ) );
    }

    Jobs jobs( 2 );
    jobs.ParallelFor( 0, 1, []( u32, u32 ) { FAIL( "nothing to do" ); } );
    CHECK_THROWS_WITH_AS( jobs.ParallelFor( 10, 0, []( u32, u32 ) {} ), "Jobs::ParallelFor in ranges of 0",
                          logic_error );
}

TEST_CASE( "An exception in ParallelFor is thrown to the caller once every range is done" )
{
    Jobs jobs( 4 );
    atomic<u32> done = 0;
    CHECK_THROWS_WITH_AS( jobs.ParallelFor( 100, 1,
                                            [&]( u32 begin, u32 end ) {
                                                done += end - begin;
                                                if ( begin <= 50 and 50 < end )
                                                    throw logic_error( "fifty" );
                                            } ),
                          "fifty", logic_error );
    CHECK( done == 100 );
}

TEST_CASE( "Background work runs off the main thread and hands what came of it to the main queue" )
{
    Jobs jobs( 2 );
    const thread::id main = this_thread::get_id();
    opt<bool> offMain;
    jobs.Background( [&] {
        const bool off = this_thread::get_id() != main;
        jobs.RunOnMain( [&offMain, off] { offMain = off; } );
    } );
    RunUntil( jobs, [&] { return offMain.has_value(); } );
    REQUIRE( offMain );
    CHECK( *offMain == cThreads );
}

TEST_CASE( "An exception from background work is thrown on the main thread, by the main queue" )
{
    Jobs jobs( 2 );
    jobs.Background( [] { throw logic_error( "broken import" ); } );
    opt<string> heard;
    RunUntil( jobs, [&] {
        try
        {
            jobs.RunMainQueue();
        }
        catch ( const logic_error& error )
        {
            heard = error.what();
        }
        return heard.has_value();
    } );
    CHECK( heard == "broken import" );
}

TEST_CASE( "The main queue runs in order, and what comes after a failure waits for the next run" )
{
    Jobs jobs( 1 );
    vector<string> ran;
    jobs.RunOnMain( [&] { ran.push_back( "first" ); } );
    jobs.RunOnMain( [] { throw logic_error( "second" ); } );
    jobs.RunOnMain( [&] { ran.push_back( "third" ); } );
    CHECK_THROWS_WITH_AS( jobs.RunMainQueue(), "second", logic_error );
    CHECK( ran == vector<string>{ "first" } );
    // Queued meanwhile: after what waited.
    jobs.RunOnMain( [&] { ran.push_back( "fourth" ); } );
    jobs.RunMainQueue();
    CHECK( ran == vector<string>{ "first", "third", "fourth" } );
}

TEST_CASE( "The main queue and background work are the main thread's to start" )
{
    if ( not cThreads )
        return;
    Jobs jobs( 2 );
    vector<string> heard;
    jobs.Background( [&] {
        for ( const auto& tryIt : { function<void()>( [&] { jobs.RunMainQueue(); } ),
                                    function<void()>( [&] { jobs.Background( [] {} ); } ) } )
        {
            try
            {
                tryIt();
            }
            catch ( const logic_error& error )
            {
                jobs.RunOnMain( [&heard, what = string( error.what() )] { heard.push_back( what ); } );
            }
        }
    } );
    RunUntil( jobs, [&] { return heard.size() == 2; } );
    CHECK( heard ==
           vector<string>{ "Jobs::RunMainQueue off the main thread", "Jobs::Background off the main thread" } );
}

TEST_CASE( "Jobs waits for background work before it goes" )
{
    atomic<bool> finished = false;
    {
        Jobs jobs( 2 );
        jobs.Background( [&] {
            this_thread::sleep_for( chrono::milliseconds( 20 ) );
            finished = true;
        } );
    }
    CHECK( finished );
}
