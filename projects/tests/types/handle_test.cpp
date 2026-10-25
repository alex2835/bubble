// Handles and the storage that hands them out: links to things that can go
// away, which find nothing once they have.
#include "bubble/types/handle.hpp"
#include "bubble/types/pointer.hpp"
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

TEST_CASE( "A freed slot is taken again, and the handles to what was there stay dead" )
{
    Things things;
    const ThingHandle a = things.Add( "a" );
    const ThingHandle b = things.Add( "b" );
    const ThingHandle c = things.Add( "c" );
    things.Remove( b );
    things.Remove( a );
    // Whichever slot the new entry lands in, neither old handle finds it.
    const ThingHandle d = things.Add( "d" );
    CHECK_FALSE( things.TryGet( a ) );
    CHECK_FALSE( things.TryGet( b ) );
    CHECK( *things.TryGet( c ) == "c" );
    CHECK( *things.TryGet( d ) == "d" );
    CHECK( things.Size() == 2 );
    CHECK( things.Handles().size() == 2 );
}

TEST_CASE( "A handle nobody handed out finds nothing" )
{
    Things things;
    const ThingHandle entry = things.Add( "entry" );
    const ThingHandle never;
    const ThingHandle farAway{ 1000000u, entry.mGeneration };
    const ThingHandle wrongGeneration{ entry.mIndex, entry.mGeneration + 1 };
    for ( const ThingHandle emptyMaybe : { never, farAway, wrongGeneration } )
    {
        CHECK_FALSE( things.Alive( emptyMaybe ) );
        CHECK_FALSE( things.TryGet( emptyMaybe ) );
        CHECK_FALSE( things.Remove( emptyMaybe ) );
    }
    // A number where the index should be is still nothing to hold on to.
    CHECK_FALSE( ThingHandle{ ThingHandle::cNone, 7 } );
    CHECK( *things.TryGet( entry ) == "entry" );
    CHECK( things.Size() == 1 );
}

TEST_CASE( "A snapshot of the live entries does not see what comes after it" )
{
    Things things;
    things.Add( "a" );
    things.Add( "b" );
    const ThingHandle late = things.Add( "c" );
    const vector<ThingHandle> snapshot = things.Handles();
    const ThingHandle extra = things.Add( "d" );
    CHECK( snapshot.size() == 3 );
    for ( const ThingHandle handle : snapshot )
        CHECK( things.Remove( handle ) );
    CHECK( things.Size() == 1 );
    CHECK( *things.TryGet( extra ) == "d" );
    CHECK_FALSE( things.TryGet( late ) );
}

TEST_CASE( "A handle knows its slot but not its map" )
{
    // A handle is a plain index and generation, so it means something only
    // inside the SlotMap that handed it out: another map with the same Tag
    // reads it as its own slot. A map per thing type is what keeps that apart.
    Things mine;
    Things someoneElse;
    const ThingHandle sword = mine.Add( "sword" );
    someoneElse.Add( "shield" );
    CHECK( someoneElse.Alive( sword ) );
    CHECK( *someoneElse.TryGet( sword ) == "shield" );
    CHECK( *mine.TryGet( sword ) == "sword" );
}

TEST_CASE( "The hash tells index and generation apart" )
{
    Things things;
    const ThingHandle first = things.Add( "first" );
    things.Remove( first );
    const ThingHandle second = things.Add( "second" );
    // first and second share one slot, and second's numbers read as index 1,
    // generation 0 the other way round: adding the two numbers together, or
    // hashing one of them, would put these three on the same key.
    CHECK( first.mIndex == second.mIndex );
    const std::hash<ThingHandle> hash;
    CHECK( hash( first ) != hash( second ) );
    CHECK( hash( second ) != hash( ThingHandle{ second.mIndex + 1, 0 } ) );
    CHECK( hash( first ) == hash( ThingHandle{ first.mIndex, first.mGeneration } ) );
    hmap<ThingHandle, string> byHandle;
    byHandle[first] = "old";
    byHandle[second] = "new";
    byHandle[ThingHandle{ second.mIndex + 1, 0 }] = "neighbour";
    CHECK( byHandle.size() == 3 );
    CHECK( byHandle[first] == "old" );
    CHECK( byHandle[second] == "new" );
    CHECK( byHandle[ThingHandle{ 1, 0 }] == "neighbour" );
}

TEST_CASE( "A slot can hold something that only moves" )
{
    using Numbers = SlotMap<Scope<int>, struct NumberTag>;
    Numbers numbers;
    const auto first = numbers.Add( CreateScope<int>( 7 ) );
    const auto second = numbers.Add( CreateScope<int>( 9 ) );
    CHECK( **numbers.TryGet( first ) == 7 );
    numbers.Remove( first );
    const auto third = numbers.Add( CreateScope<int>( 11 ) );
    CHECK( **numbers.TryGet( third ) == 11 );
    CHECK( **numbers.TryGet( second ) == 9 );
    CHECK_FALSE( numbers.TryGet( first ) );
}

namespace
{
// A value that asks the map about itself while it is being destroyed, so the
// test can see what a destructor of an entry finds there.
struct Looker
{
    SlotMap<Looker, struct LookTag>* mMap = nullptr;
    Handle<struct LookTag> mSelf;
    opt<bool>* mFoundMaybe = nullptr;

    Looker() = default;
    Looker( SlotMap<Looker, struct LookTag>& map, opt<bool>& foundMaybe ) : mMap( &map ), mFoundMaybe( &foundMaybe ) {}
    // Only the copy that lives in the slot gets to report: a moved-from shell
    // stays silent.
    Looker( Looker&& other ) : mMap( other.mMap ), mSelf( other.mSelf ), mFoundMaybe( other.mFoundMaybe )
    {
        other.mMap = nullptr;
        other.mFoundMaybe = nullptr;
    }
    ~Looker()
    {
        if ( mMap != nullptr )
            *mFoundMaybe = mMap->Alive( mSelf );
    }
};
}

TEST_CASE( "An entry being removed already looks gone to its own destructor" )
{
    SlotMap<Looker, struct LookTag> lookers;
    opt<bool> foundMaybe;
    const Handle<struct LookTag> handle = lookers.Add( Looker( lookers, foundMaybe ) );
    // The entry does not know its own handle until it is in the map.
    lookers.TryGet( handle )->mSelf = handle;
    CHECK( lookers.Remove( handle ) );
    REQUIRE( foundMaybe.has_value() );
    CHECK_FALSE( *foundMaybe );
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
    // A link can be moved to another thing or to nothing at all; it is a
    // temporary, not a seat held for life.
    int other = 8;
    someMaybe = OptRef<int>( other );
    CHECK( *someMaybe == 8 );
    CHECK( value == 4 );
    someMaybe = nullopt;
    CHECK_FALSE( someMaybe );
    CHECK( constantMaybe == OptRef<const int>( value ) );
}

TEST_CASE( "A link to a thing reads as a link to what it also is" )
{
    struct Base
    {
        int mNumber = 1;
    };
    struct Derived : Base
    {
        int mExtra = 2;
    };
    Derived derived;
    const OptRef<Derived> derivedMaybe( derived );
    const OptRef<Base> baseMaybe = derivedMaybe;
    REQUIRE( baseMaybe );
    CHECK( baseMaybe->mNumber == 1 );
    CHECK( baseMaybe == derivedMaybe );
    const OptRef<const Base> constantBaseMaybe = derivedMaybe;
    CHECK( constantBaseMaybe->mNumber == 1 );
}
