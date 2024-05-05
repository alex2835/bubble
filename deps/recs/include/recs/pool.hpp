#pragma once
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include "recs/impex.hpp"
#include "recs/utils.hpp"
#include "recs/entity.hpp"

namespace recs
{
/**
 * @brief Store sorted components data
 */
class RECS_EXPORT Pool
{
public:
    Pool() = default;

    template <ComponentType T>
    static Pool CreatePool()
    {
        auto init_func = []( void* component )
        {
            new ( component ) T();
        };
        auto delete_func = []( void* component )
        {
            static_cast<T*>( component )->~T();
        };
        auto copy_func = []( const void* from, void* to )
        {
            new ( to ) T( *static_cast<const T*>( from ) );
        };
        return Pool( sizeof( T ), init_func, delete_func, copy_func );
    }

    Pool( const Pool& );
    Pool& operator=( const Pool& ) = delete;
    Pool( Pool&& ) = default;
    Pool& operator=( Pool&& ) = default;
    ~Pool();

    template <ComponentType T, typename ...Args>
    T& Push( Entity entity, Args&& ...args );
    void* PushEmpty( Entity entity );

    void Remove( Entity entity );

    template <ComponentType T>
    T& Get( Entity entity );

    template <ComponentType T>
    T& Get( size_t index );

    size_t Size() const noexcept;

    void* GetRaw( Entity entity );
    void* GetRaw( size_t index );

    const void* GetRaw( Entity entity ) const;
    const void* GetRaw( size_t index ) const;

    Pool Clone() const;

private:
    Pool( size_t component_size,
          void( *init_func )( void* ),
          void( *delete_func )( void* ),
          void( *copy_func )( const void*, void* ) );

    void Realloc( size_t new_capacity );
    void* GetElemAddress( size_t size );
    const void* GetElemAddressConst( size_t size ) const;

public:
    size_t mSize = 0;
    size_t mCapacity = 0;
    size_t mComponentSize = 0;
    std::unique_ptr<char[]> mData;
    std::vector<Entity> mEntities;

    void( *mDoInit )( void* component ) = nullptr;
    void( *mDoDelete )( void* component ) = nullptr;
    void( *mDoCopy )( const void* from, void* to ) = nullptr;

    friend class Registry;
};


// Implementation

template <ComponentType T, typename ...Args>
T& Pool::Push( Entity entity, Args&& ...args )
{
    if ( mCapacity <= mSize + 1 )
        Realloc( 2 * mSize + 1 );

    auto iterator = std::lower_bound( mEntities.begin(), mEntities.end(), entity );
    size_t position = iterator != mEntities.end() ? iterator - mEntities.begin() : mSize;

    mEntities.insert( iterator, entity );
    std::memmove( GetElemAddress( position + 1 ), GetElemAddress( position ), mComponentSize * ( mSize - position ) );
    void* new_elem_mem = GetElemAddress( position );
    new( new_elem_mem ) T( std::forward<Args>( args )... );

    mSize++;
    return *static_cast<T*>( new_elem_mem );
}

template <ComponentType T>
T& Pool::Get( Entity entity )
{
    void* raw_data = GetRaw( entity );
    if ( raw_data )
        return *static_cast<T*>( raw_data );
    throw std::runtime_error( "Entity has component, but pool doesn't (Incorrent working)" );
}

template <ComponentType T>
T& Pool::Get( size_t index )
{
    if ( index < Size() )
        return *static_cast<T*>( GetElemAddress( index ) );
    throw std::runtime_error( "Pool::Get, out of bound access" );
}

}