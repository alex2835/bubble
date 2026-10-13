// Handles and the storage that hands them out: links to things that can go
// away, which find nothing once they have.
#include "bubble/types/handle.hpp"
#include "bubble/types/string.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
using ThingHandle = Handle<struct ThingTag>;
using Things = SlotMap<string, struct ThingTag>;
}

TEST_CASE( "A handle finds its entry until the entry is removed" )
{
    Things things;
    const ThingHandle sword = things.Add( "sword" );
    const ThingHandle shield = things.Add( "shield" );
    CHECK( things.Size() == 2 );
    CHECK( *things.TryGet( sword ) == "sword" );

    CHECK( things.Remove( sword ) );
    CHECK_FALSE( things.Alive( sword ) );
    CHECK_FALSE( things.TryGet( sword ) );
    CHECK_FALSE( things.Remove( sword ) );
    CHECK( *things.TryGet( shield ) == "shield" );
    CHECK( things.Size() == 1 );
}

TEST_CASE( "A reused slot does not answer to the old handle" )
{
    Things things;
    const ThingHandle first = things.Add( "first" );
    things.Remove( first );
    const ThingHandle second = things.Add( "second" );
    CHECK( second.mIndex == first.mIndex );
    CHECK( second.mGeneration != first.mGeneration );
    CHECK_FALSE( things.TryGet( first ) );
    CHECK( *things.TryGet( second ) == "second" );
}

TEST_CASE( "An entry stays in place while others are added" )
{
    Things things;
    const ThingHandle first = things.Add( "first" );
    const string* address = &*things.TryGet( first );
    for ( int i = 0; i < 10000; ++i )
        things.Add( to_string( i ) );
    CHECK( &*things.TryGet( first ) == address );
}

TEST_CASE( "Handles come in slot order and are a copy to change things by" )
{
    Things things;
    const ThingHandle a = things.Add( "a" );
    const ThingHandle b = things.Add( "b" );
    const ThingHandle c = things.Add( "c" );
    things.Remove( b );
    for ( const ThingHandle handle : things.Handles() )
        things.Remove( handle );
    CHECK( things.Size() == 0 );
    CHECK_FALSE( things.Alive( a ) );
    CHECK_FALSE( things.Alive( c ) );
    CHECK_FALSE( ThingHandle() );
}

TEST_CASE( "OptRef is a reference or nothing" )
{
    int value = 3;
    OptRef<int> someMaybe( value );
    OptRef<int> noneMaybe;
    CHECK( someMaybe );
    CHECK_FALSE( noneMaybe );
    *someMaybe = 4;
    CHECK( value == 4 );
    const OptRef<const int> constantMaybe = someMaybe;
    CHECK( *constantMaybe == 4 );
    CHECK( noneMaybe == nullopt );
}
