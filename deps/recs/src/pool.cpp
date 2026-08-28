
#include "recs/pool.hpp"
#include <cassert>

namespace recs
{
Pool::Pool( size_t component_size,
            void( *init_func )( void* ),
            void( *delete_func )( void* ),
            void( *copy_func )( const void*, void* ) )
    : mComponentSize( component_size ),
      mDoInit( init_func ),
      mDoDelete( delete_func ),
      mDoCopy( copy_func )
{
    Realloc( 10 );
}

Pool::Pool( const Pool& other )
{
    other.Clone( *this );
}

Pool& Pool::operator=( const Pool& other )
{
    if ( this != &other )
        other.Clone( *this );
    return *this;
}

void Pool::Clone( Pool& pool ) const
{
    // Destroy whatever the destination already held. Without this, copy-assigning
    // over a non-empty pool leaks every component that was in it.
    pool.Clear();

    pool.mEntities = mEntities;
    pool.mDoInit = mDoInit;
    pool.mDoDelete = mDoDelete;
    pool.mDoCopy = mDoCopy;

    pool.mComponentSize = mComponentSize;
    pool.mSize = 0;
    pool.Realloc( mCapacity );

    for ( size_t i = 0; i < mSize; i++ )
    {
        pool.mDoCopy( GetElemAddressConst( i ), pool.GetElemAddress( i ) );
        pool.mSize++;
    }
}

Pool::Pool( Pool&& other ) noexcept
    : mSize( other.mSize ),
      mCapacity( other.mCapacity ),
      mComponentSize( other.mComponentSize ),
      mData( std::move( other.mData ) ),
      mEntities( std::move( other.mEntities ) ),
      mDoInit( other.mDoInit ),
      mDoDelete( other.mDoDelete ),
      mDoCopy( other.mDoCopy )
{
    // The moved-from pool no longer owns any storage, so its size has to follow.
    // Otherwise its destructor walks a null buffer calling component destructors.
    other.mSize = 0;
    other.mCapacity = 0;
}

Pool& Pool::operator=( Pool&& other ) noexcept
{
    if ( this == &other )
        return *this;

    Clear();

    mSize = other.mSize;
    mCapacity = other.mCapacity;
    mComponentSize = other.mComponentSize;
    mData = std::move( other.mData );
    mEntities = std::move( other.mEntities );
    mDoInit = other.mDoInit;
    mDoDelete = other.mDoDelete;
    mDoCopy = other.mDoCopy;

    other.mSize = 0;
    other.mCapacity = 0;
    return *this;
}

Pool::~Pool()
{
    Clear();
}

// Destroys every stored component and empties the pool, leaving the allocated
// buffer and the component type's function pointers intact.
void Pool::Clear()
{
    if ( mDoDelete )
    {
        for ( size_t i = 0; i < mSize; i++ )
            mDoDelete( GetElemAddress( i ) );
    }
    mSize = 0;
    mEntities.clear();
}

void* Pool::PushEmpty( Entity entity )
{
    // Idempotent, for the same reason as Push: an entity already in this pool
    // keeps the component it has rather than gaining a second slot.
    if ( void* existing = GetRaw( entity ) )
        return existing;

    if ( mCapacity <= mSize + 1 )
        Realloc( 2 * mSize + 1 );

    const auto iterator = std::lower_bound( mEntities.begin(), mEntities.end(), entity );
    size_t position = iterator != mEntities.end() ? iterator - mEntities.begin() : mSize;

    mEntities.insert( iterator, entity );
    std::memmove( GetElemAddress( position + 1 ), GetElemAddress( position ), mComponentSize * ( mSize - position ) );
    void* new_elem_address = GetElemAddress( position );
    mDoInit( new_elem_address );

    mSize++;
    return new_elem_address;
}

std::vector<Entity>::iterator Pool::BFind( Entity entity )
{
    auto it = std::lower_bound( mEntities.begin(), mEntities.end(), entity );
    return ( it != mEntities.end() && *it == entity ) ? it : mEntities.end();
}

std::vector<Entity>::const_iterator Pool::BFind( Entity entity ) const
{
    auto it = std::lower_bound( mEntities.begin(), mEntities.end(), entity );
    return ( it != mEntities.end() && *it == entity ) ? it : mEntities.end();
}

void Pool::Remove( Entity entity )
{
    const auto iterator = BFind( entity );
    assert( iterator != mEntities.end() );

    const auto position = std::distance( mEntities.begin(), iterator );
    mEntities.erase( iterator );
    mDoDelete( GetElemAddress( position ) );
    std::memmove( GetElemAddress( position ), GetElemAddress( position + 1 ), mComponentSize * ( mSize - position - 1 ) );
    mSize--;
}

void Pool::Remove( std::span<const Entity> entities )
{
    if ( entities.empty() || mSize == 0 )
        return;

    // Merge walk over the pool and the (sorted) removal list, compacting
    // survivors down as we go.
    size_t write = 0;
    size_t next = 0;
    for ( size_t read = 0; read < mSize; read++ )
    {
        const size_t id = (size_t)mEntities[read];

        while ( next < entities.size() && (size_t)entities[next] < id )
            next++;

        if ( next < entities.size() && (size_t)entities[next] == id )
        {
            mDoDelete( GetElemAddress( read ) );
            next++;
            continue;
        }

        if ( write != read )
        {
            std::memmove( GetElemAddress( write ), GetElemAddress( read ), mComponentSize );
            mEntities[write] = mEntities[read];
        }
        write++;
    }

    mSize = write;
    mEntities.resize( write );
}

size_t Pool::GallopTo( size_t from, size_t entityId ) const noexcept
{
    // Gallop outwards to bracket the answer in [lo, hi).
    size_t lo = from;
    size_t step = 1;
    while ( lo + step < mSize && (size_t)mEntities[lo + step] < entityId )
    {
        lo += step;
        step *= 2;
    }
    size_t hi = std::min( lo + step + 1, mSize );

    // Then binary search the bracket.
    while ( lo < hi )
    {
        const size_t mid = lo + ( hi - lo ) / 2;
        if ( (size_t)mEntities[mid] < entityId )
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

void* Pool::GetRaw( Entity entity )
{
    const auto iterator = BFind( entity );
    if ( iterator != mEntities.end() )
        return GetElemAddress( std::distance( mEntities.begin(), iterator ) );
    return nullptr;
}

void* Pool::GetRaw( size_t index )
{
    if ( index < mSize )
        return GetElemAddress( index );
    return nullptr;
}

const void* Pool::GetRaw( Entity entity ) const
{
    return const_cast<Pool&>( *this ).GetRaw( entity );
}

const void* Pool::GetRaw( size_t index ) const
{
    return const_cast<Pool&>( *this ).GetRaw( index );
}

void Pool::Realloc( size_t new_capacity )
{
    char* new_data = new char[new_capacity * mComponentSize];
    if ( mData )
        std::memmove( new_data, mData.get(), mComponentSize * mSize );
    mData.reset( new_data );
    mCapacity = new_capacity;
}

} // namespace recs