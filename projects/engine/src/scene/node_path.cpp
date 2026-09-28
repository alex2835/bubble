#include "engine/pch/pch.hpp"
#include "engine/scene/node_path.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/log/log.hpp"
#include "engine/types/any.hpp"
#include <sol/sol.hpp>

namespace bubble
{
namespace
{
void ResolveIn( Scene& scene, Entity owner, Table table, int depth )
{
    // A table that holds itself somewhere below would otherwise be walked
    // for ever.
    if ( depth > 32 )
        return;
    vector<std::pair<sol::object, sol::object>> changes;
    for ( const auto& [key, value] : table )
    {
        if ( value.get_type() == sol::type::table )
            ResolveIn( scene, owner, value.as<Table>(), depth + 1 );
        else if ( value.is<NodePath>() )
        {
            const string& path = value.as<NodePath>().mPath;
            const Entity found = FindByPath( scene, owner, path );
            if ( found == Entity::Null )
                LogWarning( "State of {}: NodePath '{}' leads nowhere - {}", DescribeEntity( scene, owner ), path,
                            WhyPathFails( scene, owner, path ) );
            changes.emplace_back( key, found == Entity::Null ? sol::make_object( table.lua_state(), sol::lua_nil )
                                                               : sol::make_object( table.lua_state(), found ) );
        }
    }
    // After the walk: setting keys while pairs() is going is only safe for
    // nil, and these are not.
    for ( auto& [key, value] : changes )
        table[key] = value;
}
}

void ResolveNodePaths( Scene& scene, Entity owner )
{
    if ( not scene.HasEntity( owner ) or not scene.HasComponent<StateComponent>( owner ) )
        return;
    const auto& state = scene.GetComponent<StateComponent>( owner ).mState;
    if ( state and state->value().get_type() == sol::type::table )
        ResolveIn( scene, owner, state->as<Table>(), 0 );
}

void ResolveAllNodePaths( Scene& scene )
{
    vector<Entity> owners;
    scene.ForEach<StateComponent>( [&]( Entity entity, StateComponent& ) { owners.push_back( entity ); } );
    for ( const Entity owner : owners )
        ResolveNodePaths( scene, owner );
}

}
