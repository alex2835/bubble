#pragma once
#include <stdexcept>
#include "recs/impex.hpp"
#include "recs/utils.hpp"
#include "recs/entity.hpp"

namespace recs
{
template<ComponentType ...Components>
class RECS_EXPORT View
{
public:
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
    View( std::vector<Entity>&& entities,
          std::vector<std::tuple<Components&...>>&& components )
        : mEntities( std::move( entities ) ),
          mComponents( std::move( components ) )
    {}

    std::vector<Entity> mEntities;
    std::vector<std::tuple<Components&...>> mComponents;
    friend class Registry;
};

}