#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/renderer/transform.hpp"

namespace bubble
{
// Where the entity is, relative to its parent (see HierarchyComponent) - or
// to the world, for an entity without one. The Transform it is made of is
// that local transform: what the inspector, the gizmo, scripts and files
// read and write.
//
// Beside it sits where that puts the entity in the world, cached. Everything
// that places something in the world - drawing, physics, lights, audio, the
// camera - reads the cache. UpdateWorldTransforms (engine/scene/hierarchy.hpp)
// fills it, from the roots down, once a frame after the scripts ran; a local
// transform changed since then is not in the world yet. Not saved and not
// meant to be written by anyone else.
struct TransformComponent : public Transform
{
    TransformComponent() = default;
    TransformComponent( vec3 position, vec3 rotation = vec3( 0 ), vec3 scale = vec3( 1 ) )
        : TransformComponent( Transform( position, rotation, scale ) )
    {
    }
    // A new entity is where its transform says until the first update, so
    // that one made mid frame is not drawn at the origin.
    explicit TransformComponent( const Transform& t )
        : Transform( t ), mWorld( t ), mWorldMatrix( t.TransformMat() )
    {
    }

    static int ID() { return static_cast<int>( ComponentID::Transform ); }
	static string_view Name() { return "Transform"sv; }

    static void OnComponentDraw( InspectorContext& ctx, const Entity& entity, TransformComponent& component );
    // Fields for engine/reflection: what is saved, shown and set by path.
    static void Reflect();
    static void CreateLuaBinding( sol::state& lua );

    // The world transform: exact for a root, taken back out of the matrix for
    // a child (shear, which a parent's non uniform scale can put into a
    // rotated child, is lost there and kept in the matrix).
    const Transform& World() const { return mWorld; }
    const mat4& WorldMatrix() const { return mWorldMatrix; }

    Transform mWorld;
    mat4 mWorldMatrix = mat4( 1.0f );
};

}
