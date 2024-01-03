#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <optional>
#include <unordered_map>
#include <string_view>
#include <array>

#include "utils.hpp"
#include "entity.hpp"
#include "view.hpp"

namespace recs
{

/**
 * @brief Store components names and
 * information what components each Entity contains
 */
class RegistryMeta
{
public:
    Entity CreateEntity( void* );

    OptRef<std::vector<ComponentTypeID>> GetEntityComponents( Entity entity );

    bool HasComponent( Entity entity, ComponentTypeID component );

    template <typename ...Components>
    bool HasComponents( Entity entity );

    void Remove( Entity entity );

    void RemoveComponent( Entity entity, ComponentTypeID component );

    void AddComponent( Entity entity, ComponentTypeID component );

    template <typename T>
    ComponentTypeID GetComponentTypeID();

    template <typename ...Components>
    std::array<ComponentTypeID, sizeof...( Components )> GetComponentTypeIDs();

    void AddComponentTypeID( ComponentTypeID id );

    bool HasComponentTypeID( ComponentTypeID id );

    template <typename ...Components>
    bool HasComponentsTypeID();

private:
    RegistryMeta();
    bool HasComponent( Ref<std::vector<ComponentTypeID>> components, ComponentTypeID component );

    uint32_t mEntityCounter;
    std::vector<ComponentTypeID> mComponents;
    std::unordered_map<Entity, std::vector<ComponentTypeID>> mEntityComponentsMeta;

    friend class Registry;
};

extern int TYPE_ID_SEQUENCE;
template<typename T> inline const int TYPE_ID = TYPE_ID_SEQUENCE++;

template <typename T>
ComponentTypeID RegistryMeta::GetComponentTypeID()
{
    return TYPE_ID<T>;
}

template <typename ...Components>
std::array<ComponentTypeID, sizeof...( Components )> RegistryMeta::GetComponentTypeIDs()
{
    return std::array<ComponentTypeID, sizeof...( Components )>{GetComponentTypeID<Components>()...};
}

template <typename ...Components>
bool RegistryMeta::HasComponentsTypeID()
{
    for ( auto component : GetComponentTypeIDs<Components...>() )
        if ( !HasComponentTypeID( component ) )
            return false;
    return true;
}

}