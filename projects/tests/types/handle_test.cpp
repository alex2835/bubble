// Handles and the storage that hands them out: links to things that can go
// away, which find nothing once they have.
#include "bubble/types/handle.hpp"
#include <doctest.h>
#include <string>

using namespace bubble;

namespace
{
using ThingId = Handle<struct ThingTag>;
using Things = SlotMap<std::string, struct ThingTag>;
}

TEST_CASE( "A handle finds its entry until the entry is removed" )
{
    Things things;
    const ThingId sword = things.Add( "sword" );
    const ThingId shield = things.Add( "shield" );
    CHECK( things.Size() == 2 );
    CHECK( *things.Get( sword ) == "sword" );

    CHECK( things.Remove( sword ) );
    CHECK_FALSE( things.Alive( sword ) );
    CHECK_FALSE( things.Get( sword ) );
    CHECK_FALSE( things.Remove( sword ) );
    CHECK( *things.Get( shield ) == "shield" );
    CHECK( things.Size() == 1 );
}

TEST_CASE( "A reused slot does not answer to the old handle" )
{
    Things things;
    const ThingId first = things.Add( "first" );
    things.Remove( first );
    const ThingId second = things.Add( "second" );
    CHECK( second.mIndex == first.mIndex );
    CHECK( second.mGeneration != first.mGeneration );
    CHECK_FALSE( things.Get( first ) );
    CHECK( *things.Get( second ) == "second" );
}

TEST_CASE( "An entry stays in place while others are added" )
{
    Things things;
    const ThingId first = things.Add( "first" );
    const std::string* address = &*things.Get( first );
    for ( int i = 0; i < 10000; ++i )
        things.Add( std::to_string( i ) );
    CHECK( &*things.Get( first ) == address );
}

TEST_CASE( "Ids come in slot order and are a copy to change things by" )
{
    Things things;
    const ThingId a = things.Add( "a" );
    const ThingId b = things.Add( "b" );
    const ThingId c = things.Add( "c" );
    things.Remove( b );
    for ( const ThingId id : things.Ids() )
        things.Remove( id );
    CHECK( things.Size() == 0 );
    CHECK_FALSE( things.Alive( a ) );
    CHECK_FALSE( things.Alive( c ) );
    CHECK_FALSE( ThingId() );
}

TEST_CASE( "OptRef is a reference or nothing" )
{
    int value = 3;
    OptRef<int> some( value );
    OptRef<int> none;
    CHECK( some );
    CHECK_FALSE( none );
    *some = 4;
    CHECK( value == 4 );
    const OptRef<const int> constant = some;
    CHECK( *constant == 4 );
    CHECK( none == std::nullopt );
}
