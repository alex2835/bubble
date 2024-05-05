
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

Pool Pool::Clone() const
{
    Pool pool;
    pool.mEntities = mEntities;
    pool.mDoInit = mDoInit;
    pool.mDoDelete = mDoDelete;
    pool.mDoCopy = mDoCopy;

    pool.mComponentSize = mComponentSize;
    pool.Realloc( mCapacity );
    pool.mSize = mSize;

    for ( size_t i = 0; i < pool.mSize; i++ )
        pool.mDoCopy( GetElemAddressConst( i ), pool.GetElemAddress( i ) );

    return pool;
}

Pool::~Pool()
{
    if ( !mDoDelete )
        return;

    for ( size_t i = 0; i < mSize; i++ )
        mDoDelete( GetElemAddress( i ) );
}

void* Pool::PushEmpty( Entity entity )
{
    if ( mCapacity <= mSize + 1 )
        Realloc( 2 * mSize + 1 );

    auto iterator = std::lower_bound( mEntities.begin(), mEntities.end(), entity );
    size_t position = iterator != mEntities.end() ? iterator - mEntities.begin() : mSize;

    mEntities.insert( iterator, entity );
    std::memmove( GetElemAddress( position + 1 ), GetElemAddress( position ), mComponentSize * ( mSize - position ) );
    void* new_elem_address = GetElemAddress( position );
    mDoInit( new_elem_address );

    mSize++;
    return new_elem_address;
}

void Pool::Remove( Entity entity )
{
    auto iterator = std::lower_bound( mEntities.begin(), mEntities.end(), entity );
    assert( iterator != mEntities.end() );

    auto position = std::distance( mEntities.begin(), iterator );
    mEntities.erase( iterator );
    mDoDelete( GetElemAddress( position ) );
    std::memmove( GetElemAddress( position ), GetElemAddress( position + 1 ), mComponentSize * ( mSize - position - 1 ) );
    mSize--;
}

size_t Pool::Size() const noexcept
{
    return mSize;
}

void* Pool::GetRaw( Entity entity )
{
    auto iterator = std::lower_bound( mEntities.begin(), mEntities.end(), entity );
    if ( iterator != mEntities.end() )
    {
        auto position = std::distance( mEntities.begin(), iterator );
        return GetElemAddress( position );
    }
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
    if ( mCapacity < new_capacity )
    {
        char* new_data = new char[new_capacity * mComponentSize];
        memmove( new_data, mData.get(), mComponentSize * mSize );
        mData.reset( new_data );
        mCapacity = new_capacity;
    }
}

void* Pool::GetElemAddress( size_t size )
{
    return &mData[mComponentSize * size];
}

const void* Pool::GetElemAddressConst( size_t size ) const
{
    return &mData[mComponentSize * size];
}
}