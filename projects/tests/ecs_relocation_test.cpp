// Removing a component moves the last one of its type into the hole, by
// moving it. A component that points into itself - as a standard container
// does under MSVC's checked iterators - has to survive that.
#include "test.hpp"

namespace
{
struct SelfPointing
{
    // Not registered with ComponentManager: only the typed API reaches it.
    static int ID() { return 1000; }

    SelfPointing( int value = 0 ) : mValue( value ) {}
    SelfPointing( const SelfPointing& other ) : mValue( other.mValue ) {}
    SelfPointing( SelfPointing&& other ) noexcept : mValue( other.mValue ) {}
    SelfPointing& operator=( const SelfPointing& other ) { mValue = other.mValue; return *this; }
    SelfPointing& operator=( SelfPointing&& other ) noexcept { mValue = other.mValue; return *this; }

    bool Intact() const { return mSelf == this; }

    const SelfPointing* mSelf = this;
    int mValue;
};

bool AllIntact( Scene& scene, int& count )
{
    bool intact = true;
    count = 0;
    scene.ForEach<SelfPointing>( [&]( Entity entity, SelfPointing& c )
    {
        intact = intact and c.Intact() and c.mValue == (int)(size_t)entity;
        count++;
    } );
    return intact;
}
}

TEST( Ecs_ComponentsSurviveRelocation )
{
    Scene scene;
    vector<Entity> entities;
    for ( int i = 0; i < 200; i++ )
        entities.push_back( scene.CreateEntity() );

    // Past a page of storage, so it grows on the way.
    for ( auto it = entities.rbegin(); it != entities.rend(); ++it )
        scene.AddComponent<SelfPointing>( *it, (int)(size_t)*it );
    int count = 0;
    CHECK( AllIntact( scene, count ) and count == 200 );

    // One removal from the front moves the last one into it; a batch moves
    // many.
    scene.RemoveEntity( entities[0] );
    CHECK( AllIntact( scene, count ) and count == 199 );
    vector<Entity> every3rd;
    for ( size_t i = 1; i < entities.size(); i += 3 )
        every3rd.push_back( entities[i] );
    scene.RemoveEntities( every3rd );
    CHECK( AllIntact( scene, count ) and count == 199 - (int)every3rd.size() );

    // And the hierarchy, whose children live in a std::vector: many children
    // made under the root, as a script spawning in a loop does.
    Fixture f;
    for ( int i = 0; i < 300; i++ )
        f.Create( EntityKind::ModelObject );
    CHECK( f.Top().size() == 300 );
    SetParent( f.scene, f.Top()[0], f.Top()[299] );
    CHECK( f.Top().size() == 299 );
}
