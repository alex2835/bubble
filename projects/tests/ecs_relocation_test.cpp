// The component pool moves components when it grows and when an insert or
// erase shifts it. A component that points into itself - as a standard
// container does under MSVC's checked iterators - has to survive that.
#include "test.hpp"
#include <recs/registry.hpp>

namespace
{
struct SelfPointing
{
    static int ID() { return 0; }

    SelfPointing( int value = 0 ) : mValue( value ) {}
    SelfPointing( const SelfPointing& other ) : mValue( other.mValue ) {}
    SelfPointing( SelfPointing&& other ) noexcept : mValue( other.mValue ) {}
    SelfPointing& operator=( const SelfPointing& other ) { mValue = other.mValue; return *this; }
    SelfPointing& operator=( SelfPointing&& other ) noexcept { mValue = other.mValue; return *this; }

    bool Intact() const { return mSelf == this; }

    const SelfPointing* mSelf = this;
    int mValue;
};

bool AllIntact( recs::Registry& registry, int& count )
{
    bool intact = true;
    count = 0;
    registry.ForEach<SelfPointing>( [&]( recs::Entity entity, SelfPointing& c )
    {
        intact = intact and c.Intact() and c.mValue == (int)(size_t)entity;
        count++;
    } );
    return intact;
}
}

TEST( Ecs_ComponentsSurviveRelocation )
{
    recs::Registry registry;
    registry.AddComponent<SelfPointing>();
    vector<recs::Entity> entities;
    for ( int i = 0; i < 200; i++ )
        entities.push_back( registry.CreateEntity() );

    // Added from the highest id down: every add is an insert at the front,
    // shifting all the others, and the pool grows several times on the way.
    for ( auto it = entities.rbegin(); it != entities.rend(); ++it )
        registry.AddComponent<SelfPointing>( *it, (int)(size_t)*it );
    int count = 0;
    CHECK( AllIntact( registry, count ) and count == 200 );

    // One erase from the front shifts the rest down; a batch compacts.
    registry.RemoveEntity( entities[0] );
    CHECK( AllIntact( registry, count ) and count == 199 );
    vector<recs::Entity> every3rd;
    for ( size_t i = 1; i < entities.size(); i += 3 )
        every3rd.push_back( entities[i] );
    registry.RemoveEntities( every3rd );
    CHECK( AllIntact( registry, count ) and count == 199 - (int)every3rd.size() );

    // And the hierarchy, whose children live in a std::vector: many children
    // made under the root, as a script spawning in a loop does.
    Fixture f;
    for ( int i = 0; i < 300; i++ )
        f.Create( EntityKind::ModelObject );
    CHECK( f.Top().size() == 300 );
    SetParent( f.scene, f.Top()[0], f.Top()[299] );
    CHECK( f.Top().size() == 299 );
}
