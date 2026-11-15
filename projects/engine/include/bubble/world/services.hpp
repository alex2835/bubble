#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/utility.hpp"
#include <entt/core/any.hpp>
#include <entt/core/type_info.hpp>

namespace bubble
{
// The systems of a world that others ask things of as a whole - the physics
// world for a raycast, audio to play a sound - found by their type:
// world.Get<PhysicsWorld>(). A module adds what it owns, and the world holds
// a reference: the module outlives everyone who asks.
class Services
{
public:
    // A type added twice is a logic_error.
    template <typename T>
    void Add( T& service )
    {
        const auto [at, added] = mServices.try_emplace( entt::type_hash<T>::value(), entt::forward_as_any( service ) );
        if ( not added )
            throw logic_error( format( "Services::Add of {} twice", entt::type_name<T>::value() ) );
    }

    template <typename T>
    OptRef<T> TryGet()
    {
        const auto found = mServices.find( entt::type_hash<T>::value() );
        if ( found == mServices.end() )
            return nullopt;
        return *entt::any_cast<T>( &found->second );
    }

    // One nobody added is a logic_error: the module that adds it is missing.
    template <typename T>
    T& Get()
    {
        const auto serviceMaybe = TryGet<T>();
        if ( not serviceMaybe )
            throw logic_error(
                format( "no service {}: the module that adds it is not in the world", entt::type_name<T>::value() ) );
        return *serviceMaybe;
    }

private:
    hmap<u32, entt::any> mServices;
};
}
