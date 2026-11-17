#pragma once
#include "bubble/scene/entity.hpp"
#include "bubble/types/matrix.hpp"
#include "bubble/world/module.hpp"

namespace bubble
{
class Scene;

// Where an entity is, relative to its parent: what files and the editor
// hold. The rotation is three angles in radians, edited as degrees - about
// Y, then X, then Z: yaw, pitch, roll, as Godot and Unity turn. Forward is
// -Z, up is +Y, as in glTF.
struct Transform
{
    vec3 mPosition{ 0.0f };
    vec3 mRotation{ 0.0f };
    vec3 mScale{ 1.0f };
};

// Where an entity is in the world: its transform after every ancestor's,
// an entity with none taking its parent's. Made each frame, in
// Phase::Transforms, for every entity in the tree and for one outside it
// that has a transform. Runtime: never described, never saved.
struct WorldTransform
{
    mat4 mMatrix{ 1.0f };
};

mat4 LocalMatrix( const Transform& transform );
quat RotationOf( const Transform& transform );
// The angles a Transform turns by for `rotation`: about Y, X, then Z. Looking
// straight up or down, the roll is 0 and the yaw takes it all.
vec3 AnglesOf( const quat& rotation );

// The world transform as Phase::Transforms will make it, worked out now up
// the tree: for a module that needs it between frames - a body made the
// moment its entity comes.
mat4 ComputeWorldMatrix( const Scene& scene, Entity entity );

// "transform", and the world transforms made from it.
ModuleType TransformModule();
}
