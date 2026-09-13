#include "engine/pch/pch.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/geometry.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include "engine/scripting/lua_value_property.hpp"

namespace bubble
{
void TransformComponent::OnComponentDraw( const Project& project, const Entity& entity, TransformComponent& transformComponent )
{
    ImGui::TextColored( TEXT_COLOR, "TransformComponent" );
    ImGui::DragFloat3( "Scale", (float*)&transformComponent.mScale, 0.01f, 0.01f );
    ImGui::DragFloat3( "Rotation", (float*)&transformComponent.mRotation, 0.01f );
    ImGui::DragFloat3( "Position", (float*)&transformComponent.mPosition, 0.1f );
}

void TransformComponent::ToJson( json& json, const Project& project, const TransformComponent& transformComponent )
{
    json["Position"] = transformComponent.mPosition;
    json["Rotation"] = transformComponent.mRotation;
    json["Scale"] = transformComponent.mScale;
}

void TransformComponent::FromJson( const json& json, Project& project, TransformComponent& transformComponent )
{
    transformComponent.mPosition = json["Position"];
    transformComponent.mRotation = json["Rotation"];
    transformComponent.mScale = json["Scale"];
}

void TransformComponent::CreateLuaBinding( sol::state& lua )
{
    auto to_string = []( const TransformComponent& t )
    {
        const vec3& p = t.mPosition;
        const vec3& r = t.mRotation;
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
        "rotation",  ValueProperty( &TransformComponent::mRotation ),
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
        "rotate",
        sol::overload(
            []( TransformComponent& t, f32 x, f32 y, f32 z ) { t.mRotation += vec3( x, y, z ); },
            []( TransformComponent& t, const vec3& d )       { t.mRotation += d; }
        ),
        "set_rotation",
        sol::overload(
            []( TransformComponent& t, f32 x, f32 y, f32 z ) { t.mRotation = vec3( x, y, z ); },
            []( TransformComponent& t, const vec3& r )       { t.mRotation = r; }
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
