#pragma once
#include "engine/scene/component_manager.hpp"
#include "engine/types/string.hpp"
#include <cctype>
#include <format>
#include <stdexcept>

// Helpers shared by the scripting bindings. Private to src/scripting/bindings -
// nothing outside those translation units needs them, so they are deliberately
// not in the public include tree next to the Create*Bindings declarations.
//
// inline rather than static: ComponentLuaName holds a function local static, and
// one shared copy across the bindings beats one per translation unit.
namespace bubble
{
// The Lua API is snake_case throughout, while ComponentID is PascalCase.
inline string ToSnakeCase( string_view name )
{
    string out;
    for ( size_t i = 0; i < name.size(); i++ )
    {
        const unsigned char c = (unsigned char)name[i];
        if ( std::isupper( c ) and i > 0 and not std::isupper( (unsigned char)name[i - 1] ) )
            out += '_';
        out += (char)std::tolower( c );
    }
    return out;
}

// The name a component is addressed by in Lua: Component.transform to select
// it, components.transform to read it back. Both are the component's own
// Name() lowercased, so the two cannot end up disagreeing.
//
// Held in a static because the for_each_entity callback runs per entity, and
// ToSnakeCase allocates.
template <typename Component>
const string& ComponentLuaName()
{
    static const string name = ToSnakeCase( Component::Name() );
    return name;
}

// BuildComponentEnum has ids and not types, so it reaches Name() through the
// registry, which ComponentManager::Add fills from that same Name().
inline string ComponentLuaName( ComponentID id )
{
    return ToSnakeCase( ComponentManager::GetName( static_cast<int>( id ) ) );
}


// A resource loaded by path from a script. Failing loudly here beats handing
// back a null Ref: the script names the file, so the script is where the
// mistake is, and a null model surfaces three frames later as a blank screen.
template <class ResourceRef, class Load>
ResourceRef LoadOrThrow( Load load, string_view what, const string& resourcePath )
{
    ResourceRef resource = load( resourcePath );
    if ( not resource )
        throw std::runtime_error( std::format( "Failed to load {}: {}", what, resourcePath ) );
    return resource;
}

}
