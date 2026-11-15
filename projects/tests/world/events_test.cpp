// Events: what happened, queued until the world delivers it.
#include "bubble/types/format.hpp"
#include "bubble/types/utility.hpp"
#include "bubble/world/events.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Opened
{
    string mDoor;
};

struct Rang
{
    int mTimes = 0;
};
}

TEST_CASE( "Events wait to be delivered, then go in the order emitted, each to its type's listeners in turn" )
{
    Events events;
    vector<string> heard;
    events.Subscribe<Opened>( [&]( const Opened& opened ) { heard.push_back( "a " + opened.mDoor ); } );
    events.Subscribe<Rang>( [&]( const Rang& rang ) { heard.push_back( format( "rang {}", rang.mTimes ) ); } );
    events.Subscribe<Opened>( [&]( const Opened& opened ) { heard.push_back( "b " + opened.mDoor ); } );

    events.Emit( Opened{ "north" } );
    events.Emit( Rang{ 2 } );
    events.Emit( Opened{ "south" } );
    CHECK( heard.empty() );
    CHECK( events.Waiting() == 3 );

    events.Deliver();
    CHECK( heard == vector<string>{ "a north", "b north", "rang 2", "a south", "b south" } );
    CHECK( events.Waiting() == 0 );
}

TEST_CASE( "What a listener emits comes in the same delivery; who it subscribes hears from the next event" )
{
    Events events;
    vector<string> heard;
    events.Subscribe<Opened>( [&]( const Opened& opened ) {
        heard.push_back( "opened " + opened.mDoor );
        events.Emit( Rang{ 1 } );
        events.Subscribe<Opened>( [&]( const Opened& late ) { heard.push_back( "late " + late.mDoor ); } );
    } );
    events.Subscribe<Rang>( [&]( const Rang& ) { heard.push_back( "rang" ); } );

    events.Emit( Opened{ "north" } );
    events.Emit( Opened{ "south" } );
    events.Deliver();
    CHECK( heard == vector<string>{ "opened north", "opened south", "late south", "rang", "rang" } );
}

TEST_CASE( "A listener may take itself off while it runs, and delivering from inside one is a bug" )
{
    Events events;
    int heard = 0;
    EventHandle once;
    once = events.Subscribe<Rang>( [&]( const Rang& ) {
        ++heard;
        events.Unsubscribe( once );
    } );
    events.Emit( Rang{} );
    events.Emit( Rang{} );
    events.Deliver();
    CHECK( heard == 1 );
    events.Unsubscribe( once );

    events.Subscribe<Opened>( [&]( const Opened& ) { events.Deliver(); } );
    events.Emit( Opened{} );
    CHECK_THROWS_AS( events.Deliver(), logic_error );
}
