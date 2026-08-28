#pragma once
#include <tuple>
#include <array>
#include "recs/impex.hpp"
#include "recs/utils.hpp"
#include "recs/pool.hpp"

namespace recs
{

// Zero-allocation lazy view. Stores only pool pointers (stack) and runs the
// merge-join one step per iterator increment — no heap allocation.
// Invariant: a view must not outlive the pools it points at, and adding or
// removing components of the viewed types while iterating invalidates it, since
// the pool's storage can reallocate and shift. Merely registering new component
// types is safe: std::unordered_map never invalidates pointers to existing
// elements, not even on rehash.
template<ComponentType ...Components>
class View
{
    static constexpr size_t N = sizeof...( Components );

public:
    struct Sentinel {};

    class Iterator
    {
    public:
        Iterator() = default;

        explicit Iterator( std::array<Pool*, N> pools )
            : mPools( pools )
        {
            mIndices.fill( 0 );
            for ( auto p : mPools )
                if ( !p ) 
                    return;
            mAtEnd = false;
            advance();
        }

        // Returns references directly into pool storage — no copies.
        std::tuple<Components&...> operator*() const
        {
            return makeResult( std::make_index_sequence<N>{} );
        }

        Iterator& operator++()
        {
            for ( size_t i = 0; i < N; i++ )
                mIndices[i]++;
            advance();
            return *this;
        }

        bool operator==( const Sentinel& ) const { return mAtEnd; }
        bool operator!=( const Sentinel& ) const { return !mAtEnd; }

    private:
        void advance()
        {
            while ( !mAtEnd )
            {
                for ( size_t i = 0; i < N; i++ )
                    if ( mIndices[i] >= mPools[i]->Size() ) { mAtEnd = true; return; }

                size_t max_id = 0;
                for ( size_t i = 0; i < N; i++ )
                    max_id = std::max( max_id, (size_t)mPools[i]->Entities()[mIndices[i]] );

                bool found = true;
                for ( size_t i = 0; i < N; i++ )
                {
                    while ( mIndices[i] < mPools[i]->Size() &&
                            (size_t)mPools[i]->Entities()[mIndices[i]] < max_id )
                        mIndices[i]++;
                    if ( mIndices[i] >= mPools[i]->Size() ) { mAtEnd = true; return; }
                    if ( (size_t)mPools[i]->Entities()[mIndices[i]] != max_id )
                    {
                        found = false;
                        break;
                    }
                }

                if ( found ) return;
            }
        }

        template<size_t ...Is>
        std::tuple<Components&...> makeResult( std::index_sequence<Is...> ) const
        {
            return std::forward_as_tuple(
                mPools[Is]->template Get<std::tuple_element_t<Is, std::tuple<Components...>>>( mIndices[Is] )...
            );
        }

        std::array<Pool*, N> mPools    = {};
        std::array<size_t, N> mIndices = {};
        bool mAtEnd = true;
    };

    View() = default;
    explicit View( std::array<Pool*, N> pools ) : mPools( pools ) {}

    Iterator begin() { return Iterator( mPools ); }
    Sentinel end()   { return {}; }

    // O(n) — traverses the full result set.
    size_t Size()
    {
        size_t n = 0;
        for ( auto it = begin(); it != end(); ++it )
            n++;
        return n;
    }

private:
    std::array<Pool*, N> mPools = {};
};

} // namespace recs
