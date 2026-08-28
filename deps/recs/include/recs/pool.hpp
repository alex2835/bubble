#pragma once
#include <memory>
#include <mutex>
#include <cstring>
#include <span>
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
 *
 * Components are stored unboxed in one buffer, kept sorted by entity id, and
 * relocated with memmove. A component type must therefore be trivially
 * relocatable: no self-references and no pointers registered elsewhere that
 * point back into the object. std::string / std::vector members are fine.
 */
class RECS_EXPORT Pool
{
public:
    Pool() = default;

    template <ComponentType T>
    static Pool CreatePool()
    {
        // Realloc allocates with plain new[], which only guarantees alignment up
        // to __STDCPP_DEFAULT_NEW_ALIGNMENT__. Fail loudly rather than handing
        // back misaligned storage for an over-aligned (e.g. SIMD) component.
        static_assert( alignof( T ) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                       "recs: over-aligned component types are not supported" );

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
    Pool& operator=( const Pool& );
    Pool( Pool&& ) noexcept;
    Pool& operator=( Pool&& ) noexcept;
    ~Pool();

    template <ComponentType T, typename ...Args>
    T& Push( Entity entity, Args&& ...args );
    void* PushEmpty( Entity entity );

    void Remove( Entity entity );

    // Batch erase. `entities` must be sorted ascending; ids that are not in this
    // pool are ignored. Runs one compaction pass over the pool - O(Size()) total
    // rather than a full tail shift per entity.
    void Remove( std::span<const Entity> entities );

    template <ComponentType T>
    T& Get( Entity entity );

    template <ComponentType T>
    const T& Get( Entity entity ) const;

    template <ComponentType T>
    T& Get( size_t index );

    template <ComponentType T>
    const T& Get( size_t index ) const;

    // Defined in-class: these sit in the per-element hot loop of ForEach/View,
    // where an out-of-line call costs more than the work they do.
    size_t Size() const noexcept { return mSize; }

    // Entities in this pool, sorted by id and parallel to the component
    // storage: the component at index i belongs to Entities()[i].
    const std::vector<Entity>& Entities() const noexcept { return mEntities; }

    // Index of the first entity at or after `from` whose id is >= entityId,
    // or Size() if there is none.
    size_t AdvanceTo( size_t from, size_t entityId ) const noexcept
    {
        // Fast path: dense overlap, where the entry we want is already here.
        // This is the common case and must stay inline and branch-predictable.
        if ( from >= mSize || (size_t)mEntities[from] >= entityId )
            return from;

        // Sparse overlap: skipping a large gap costs O(log gap), not O(gap).
        return GallopTo( from, entityId );
    }

    void* GetRaw( Entity entity );
    void* GetRaw( size_t index );

    const void* GetRaw( Entity entity ) const;
    const void* GetRaw( size_t index ) const;


private:
    Pool( size_t component_size,
          void( *init_func )( void* ),
          void( *delete_func )( void* ),
          void( *copy_func )( const void*, void* ) );

    void Clear();
    void Clone( Pool& pool ) const;
    void Realloc( size_t new_capacity );
    size_t GallopTo( size_t from, size_t entityId ) const noexcept;

    void* GetElemAddress( size_t index ) { return &mData[mComponentSize * index]; }
    const void* GetElemAddressConst( size_t index ) const { return &mData[mComponentSize * index]; }

    std::vector<Entity>::iterator BFind( Entity entity );
    std::vector<Entity>::const_iterator BFind( Entity entity ) const;

private:
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
    // One slot per entity. Pushing an entity that is already here replaces its
    // component instead of inserting a duplicate, which would silently corrupt
    // the sorted merge-join that ForEach and View rely on.
    if ( void* existing = GetRaw( entity ) )
    {
        mDoDelete( existing );
        new ( existing ) T( std::forward<Args>( args )... );
        return *static_cast<T*>( existing );
    }

    if ( mCapacity <= mSize + 1 )
        Realloc( 2 * mSize + 1 );

    size_t position = mSize;
    auto iterator = std::lower_bound( mEntities.begin(), mEntities.end(), entity );
    if ( iterator != mEntities.end() )
    {
        position = iterator - mEntities.begin();
        std::memmove( GetElemAddress( position + 1 ), GetElemAddress( position ), mComponentSize * ( mSize - position ) );
    }
    mEntities.insert( iterator, entity );
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
    throw std::runtime_error( "Entity has component, but pool doesn't (Incorrect working)" );
}

template <ComponentType T>
const T& Pool::Get( Entity entity ) const
{
    const void* raw_data = GetRaw( entity );
    if ( raw_data )
        return *static_cast<const T*>( raw_data );
    throw std::runtime_error( "Entity has component, but pool doesn't (Incorrect working)" );
}

template <ComponentType T>
T& Pool::Get( size_t index )
{
    if ( index < Size() )
        return *static_cast<T*>( GetElemAddress( index ) );
    throw std::runtime_error( "Pool::Get, out of bound access" );
}

template <ComponentType T>
const T& Pool::Get( size_t index ) const
{
    if ( index < Size() )
        return *static_cast<const T*>( GetElemAddressConst( index ) );
    throw std::runtime_error( "Pool::Get, out of bound access" );
}

}