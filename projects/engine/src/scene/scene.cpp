#include "bubble/scene/scene.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/random.hpp"

namespace bubble
{
namespace
{
// random_device gives 32 bits a call.
u64 RandomSeed()
{
    random_device device;
    return u64( device() ) << 32 | device();
}
}

Scene::Scene() : Scene( RandomSeed() )
{
}

Scene::Scene( u64 seed ) : mUidState( seed )
{
}

Entity Scene::Create()
{
    return Make( NextUid() );
}

expected<Entity, string> Scene::Create( Uid uid )
{
    if ( not uid )
        return unexpected( "an entity's uid is never 0"s );
    if ( const auto other = mByUid.find( uid ); other != mByUid.end() )
        return unexpected(
            format( "uid {} is taken by entity {}", uid.ToString(), static_cast<u32>( other->second ) ) );
    return Make( uid );
}

Entity Scene::Make( Uid uid )
{
    const Entity entity = mRegistry.create();
    mRegistry.emplace<UidComponent>( entity, uid );
    mByUid.emplace( uid, entity );
    return entity;
}

// SplitMix64: every state gives a different number, so uids from one seed
// repeat only after 2^64 of them. A loaded uid may still be in the way.
Uid Scene::NextUid()
{
    for ( ;; )
    {
        mUidState += 0x9e3779b97f4a7c15ull;
        u64 value = mUidState;
        value = ( value ^ ( value >> 30 ) ) * 0xbf58476d1ce4e5b9ull;
        value = ( value ^ ( value >> 27 ) ) * 0x94d049bb133111ebull;
        value ^= value >> 31;
        const Uid uid{ value };
        if ( uid and not mByUid.contains( uid ) )
            return uid;
    }
}

void Scene::Destroy( Entity entity )
{
    if ( not Alive( entity ) )
        return;
    mByUid.erase( mRegistry.get<UidComponent>( entity ).mUid );
    mRegistry.destroy( entity );
}

bool Scene::Alive( Entity entity ) const
{
    return mRegistry.valid( entity );
}

size_t Scene::Count() const
{
    // Every entity has exactly one uid.
    return mByUid.size();
}

opt<Entity> Scene::Find( Uid uid ) const
{
    const auto found = mByUid.find( uid );
    if ( found == mByUid.end() )
        return nullopt;
    return found->second;
}

opt<Uid> Scene::UidOf( Entity entity ) const
{
    if ( not Alive( entity ) )
        return nullopt;
    return mRegistry.get<UidComponent>( entity ).mUid;
}

void Scene::RequireAlive( Entity entity, string_view what ) const
{
    if ( not Alive( entity ) )
        throw logic_error( format( "{} on entity {}, which is gone", what, static_cast<u32>( entity ) ) );
}
}
