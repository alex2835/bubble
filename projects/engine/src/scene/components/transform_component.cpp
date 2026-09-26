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
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include "engine/scripting/lua_value_property.hpp"

namespace bubble
{
namespace
{
// The inspector shows the rotation as Euler degrees. Angles read back from a
// quaternion may come out as another spelling of the same rotation - past 90
// degrees of Y, (0, 100, 0) reads (180, 80, 180) - which would make a field
// jump under the mouse. So the angles last shown for an entity are kept, and
// shown again for as long as its rotation is still the one they made.
struct ShownAngles
{
    quat mRotation;
    vec3 mDegrees;
};
hash_map<Entity, ShownAngles> gShownAngles;

vec3 DegreesFor( Entity entity, const quat& rotation )
{
    const auto it = gShownAngles.find( entity );
    if ( it != gShownAngles.end() and it->second.mRotation == rotation )
        return it->second.mDegrees;
    return glm::degrees( Transform::ToEuler( rotation ) );
}
}

void TransformComponent::OnComponentDraw( InspectorContext& ctx, const Entity& entity, TransformComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "TransformComponent" );
    DragFloat3Field<TransformComponent>( ctx, entity, "Scale", &TransformComponent::mScale, 0.01f, 0.01f );
    EditProperty<TransformComponent>( ctx, entity, "Rotation", component.mRotation,
        [entity]( quat& rotation )
        {
            vec3 degrees = DegreesFor( entity, rotation );
            if ( not ImGui::DragFloat3( "Rotation", glm::value_ptr( degrees ), 0.5f, 0.0f, 0.0f, "%.1f" ) )
                return false;
            rotation = Transform::FromEuler( glm::radians( degrees ) );
            gShownAngles[entity] = { rotation, degrees };
            return true;
        },
        []( TransformComponent& c, const quat& rotation ) { c.mRotation = rotation; } );
    DragFloat3Field<TransformComponent>( ctx, entity, "Position", &TransformComponent::mPosition, 0.1f );
}

// Rotation is saved as the quaternion, [x, y, z, w]. Levels saved before it
// was one hold three Euler radians there, read as such.
void TransformComponent::ToJson( json& json, const Project& project, const TransformComponent& transformComponent )
{
    const quat& r = transformComponent.mRotation;
    json["Position"] = transformComponent.mPosition;
    json["Rotation"] = { r.x, r.y, r.z, r.w };
    json["Scale"] = transformComponent.mScale;
}

void TransformComponent::FromJson( const json& json, Project& project, TransformComponent& transformComponent )
{
    transformComponent.mPosition = json["Position"];
    const auto& rotation = json["Rotation"];
    if ( rotation.is_array() and rotation.size() == 4 )
        transformComponent.mRotation = glm::normalize( quat( rotation[3].get<f32>(), rotation[0].get<f32>(),
                                                             rotation[1].get<f32>(), rotation[2].get<f32>() ) );
    else
        transformComponent.SetEuler( rotation.get<vec3>() );
    transformComponent.mScale = json["Scale"];
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
