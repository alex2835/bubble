#pragma once
#include <sol/sol.hpp>

namespace bubble
{
// A vec/mat member of a component, exposed to Lua BY VALUE.
//
// sol's default for a member whose type is itself a usertype - binding it as
// `"position", &T::mPosition` - hands Lua a reference into the owning object.
// For a component that is a pointer into a pool: fine to use on the spot, and a
// dangling pointer the moment a script keeps it, since any add or remove can
// reallocate or compact the pool underneath it. `state.p = t.position` was the
// single easiest way to corrupt memory from a script.
//
// A copy makes that safe, and makes the field behave the same way the Entity
// shorthands (`entity.position`) already did. The cost is that
//
//     t.position.x = 5
//
// now mutates the copy and is a silent no-op. Assign the whole value:
//
//     t.position = vec3( 5, t.position.y, t.position.z )
//
// Primitive members (f32, bool, string) are already pushed as copies and do
// not need this.
template <class T, class Member>
auto ValueProperty( Member T::*member )
{
    return sol::property(
        [member]( const T& self ) -> Member { return self.*member; },
        [member]( T& self, const Member& value ) { self.*member = value; } );
}

}
