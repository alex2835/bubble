#include "engine/pch/pch.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/types/map.hpp"
#include "engine/utils/geometry.hpp"
#include "engine/reflection/reflection.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include "engine/scripting/lua_value_property.hpp"

namespace bubble
{
// The local transform. Where that puts the entity in the world is not
// saved: UpdateWorldTransforms fills it after a load.
void TransformComponent::Reflect()
{
    TypeBuilder<TransformComponent>( "Transform" )
        .Field<&TransformComponent::mPosition>( "position", { .mSpeed = 0.1f } )
        .Field<&TransformComponent::mRotation>( "rotation" )
        .Field<&TransformComponent::mScale>( "scale", { .mMin = 0.01f, .mSpeed = 0.01f } );
}

void TransformComponent::CreateLuaBinding( sol::state& lua )
{
    auto to_string = []( const TransformComponent& t )
    {
        const vec3& p = t.mPosition;
        const vec3 r = t.Euler();
        const vec3& s = t.mScale;
        return std::format( "pos: [{},{},{}], rot: [{},{},{}], scale: [{},{},{}]",
                            p.x, p.y, p.z, r.x, r.y, r.z, s.x, s.y, s.z );
    };

    lua.new_usertype<TransformComponent>(
        "Transform",
        sol::call_constructor,
        sol::constructors<TransformComponent(), TransformComponent( vec3 ), TransformComponent( vec3, vec3, vec3 )>(),
        // By value - see ValueProperty. A reference here is a pointer into the
        // transform pool, which no script may keep.
        "position",  ValueProperty( &TransformComponent::mPosition ),
        // Euler radians, X then Y then Z; the transform holds a quaternion.
        "rotation",  sol::property( []( const TransformComponent& t ) { return t.Euler(); },
                                    []( TransformComponent& t, const vec3& r ) { t.SetEuler( r ); } ),
        "scale",     ValueProperty( &TransformComponent::mScale ),

        // Mutators for the hot path. With `position` a copy, the only way to
        // move an entity through the field is `t.position = t.position + d`,
        // which builds two vec3 userdatas per call. The scalar forms cross the
        // boundary as plain numbers and allocate nothing - cheaper than even
        // the old by-reference field was. The vec3 forms are for when the
        // caller already holds one (`t:translate( dir * speed * dt )`), where
        // the allocation has already happened.
        "translate",
        sol::overload(
            []( TransformComponent& t, f32 x, f32 y, f32 z ) { t.mPosition += vec3( x, y, z ); },
            []( TransformComponent& t, const vec3& d )       { t.mPosition += d; }
        ),
        "set_position",
        sol::overload(
            []( TransformComponent& t, f32 x, f32 y, f32 z ) { t.mPosition = vec3( x, y, z ); },
            []( TransformComponent& t, const vec3& p )       { t.mPosition = p; }
        ),
        // Turns by the given Euler radians about the transform's own axes.
        "rotate",
        sol::overload(
            []( TransformComponent& t, f32 x, f32 y, f32 z ) { t.mRotation = glm::normalize( t.mRotation * Transform::FromEuler( vec3( x, y, z ) ) ); },
            []( TransformComponent& t, const vec3& d )       { t.mRotation = glm::normalize( t.mRotation * Transform::FromEuler( d ) ); }
        ),
        "set_rotation",
        sol::overload(
            []( TransformComponent& t, f32 x, f32 y, f32 z ) { t.SetEuler( vec3( x, y, z ) ); },
            []( TransformComponent& t, const vec3& r )       { t.SetEuler( r ); }
        ),
        "set_scale",
        sol::overload(
            []( TransformComponent& t, f32 x, f32 y, f32 z ) { t.mScale = vec3( x, y, z ); },
            []( TransformComponent& t, const vec3& s )       { t.mScale = s; }
        ),
        sol::meta_function::to_string, to_string
    );
}

}
