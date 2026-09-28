#include "engine/pch/pch.hpp"
#include "engine/scene/components/camera_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/geometry.hpp"
#include "engine/reflection/reflection.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
void CameraComponent::UpdateOrbit( TransformComponent& transform )
{
    Camera::UpdateOrbit();

    // The look angles come from mForward, which UpdateOrbit pointed at
    // mCenter - not from mYaw/mPitch, which are the place on the sphere and
    // face the other way. This is the inverse of VectorsFromEuler, so the
    // propagation reproduces mForward exactly.
    transform.mPosition = mPosition;
    transform.SetLookAngles( std::asin( glm::clamp( mForward.y, -1.0f, 1.0f ) ),
                             std::atan2( mForward.z, mForward.x ) );
}

void CameraComponent::OrbitFromTransform( const TransformComponent& transform )
{
    // Inverse of Camera::UpdateOrbit:
    //   x = cx + r cos(p) sin(y),  y = cy - r sin(p),  z = cz + r cos(p) cos(y)
    const vec3 offset = transform.mPosition - mCenter;
    const f32 radius = length( offset );
    // On top of the center there is no direction to take; keep the angles
    // and just record the radius.
    if ( radius < 1e-4f )
    {
        mRadius = radius;
        return;
    }
    mRadius = radius;
    mYaw    = std::atan2( offset.x, offset.z );
    mPitch  = -std::asin( glm::clamp( offset.y / radius, -1.0f, 1.0f ) );
}

// Position, forward, up and right are not described: they are the cache the
// transform fills, and the transform is saved on its own.
namespace
{
constexpr u32 cCache = FieldInfo::ReadOnly | FieldInfo::Transient | FieldInfo::Hidden;
}

void CameraComponent::Reflect()
{
    TypeBuilder<CameraComponent>( Name().data() )
        .Note( "Position and orientation come from this entity's transform." )
        .Field<&CameraComponent::mWorldUp>( "world_up", { .mFlags = FieldInfo::Hidden } )
        .Field<&CameraComponent::mNear>( "near", { .mMin = 0.01f, .mSpeed = 0.01f } )
        .Field<&CameraComponent::mFar>( "far", { .mMin = 1.0f, .mMax = 10000.0f } )
        .Field<&CameraComponent::mFov>( "fov", { .mMin = 0.1f, .mMax = 3.14f, .mFlags = FieldInfo::Slider } )
        .Field<&CameraComponent::mYaw>( "yaw", { .mFlags = FieldInfo::Hidden } )
        .Field<&CameraComponent::mPitch>( "pitch", { .mFlags = FieldInfo::Hidden } )
        .Field<&CameraComponent::mRadius>( "radius", { .mMin = 0.1f, .mMax = 100.0f, .mSpeed = 0.1f } )
        // What the orbit turns around; set by scripts, not saved.
        .Field<&CameraComponent::mCenter>( "center", { .mFlags = FieldInfo::Hidden | FieldInfo::Transient } )
        // The cache PropagateCameraTransforms fills from the transform: for
        // scripts to read. A camera moves by moving its entity.
        .Field<&CameraComponent::mPosition>( "position", { .mFlags = cCache } )
        .Field<&CameraComponent::mForward>( "forward", { .mFlags = cCache } )
        .Field<&CameraComponent::mUp>( "up", { .mFlags = cCache } )
        .Field<&CameraComponent::mRight>( "right", { .mFlags = cCache } );
}

void CameraComponent::BindLuaMethods( sol::state&, sol::usertype<CameraComponent>& type )
{
    type["get_lookat_mat"] = &CameraComponent::GetLookatMat;
    type["get_projection_mat"] = &CameraComponent::GetProjectionMat;
    // camera:update_orbit( entity:get_transform() )
    type["update_orbit"] = &CameraComponent::UpdateOrbit;
    // camera:orbit_from_transform( entity:get_transform() ), in on_start
    type["orbit_from_transform"] = &CameraComponent::OrbitFromTransform;
}

}
