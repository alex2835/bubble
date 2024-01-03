#pragma once
#include "entity.hpp"
#include "utils.hpp"
#include <stdexcept>

namespace recs
{
template<typename ...Components>
class View
{
public:
    View( std::vector<Entity>&& entities,
          std::vector<std::tuple<Components&...>>&& components )
        : mEntities( std::move( entities ) ),
        mComponents( std::move( components ) )
    {
    }

    template <typename F>
    void ForEach( F&& func )
    {
        for ( auto& components : mComponents )
            std::apply( std::forward<F>( func ), components );
    }

    std::tuple<Components&...> Get( Entity entity )
    {
        auto iterator = std::find( begin(), end(), entity );
        if ( iterator == end() )
            throw std::runtime_error( "View::Get - Invalid entity" );
        return *iterator;
    }

    std::tuple<Components&...> Get( size_t pos )
    {
        return mComponents[pos];
    }

    size_t Size()
    {
        return mEntities.size();
    }

    auto& GetEntities()
    {
        return mEntities;
    }
    auto& GetComponets()
    {
        return mComponents;
    }

    auto begin()
    {
        return mComponents.begin();
    }
    auto end()
    {
        return mComponents.end();
    }

private:
    std::vector<Entity> mEntities;
    std::vector<std::tuple<Components&...>> mComponents;
};

}