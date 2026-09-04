#pragma once
#include <sol/forward.hpp>
#include "engine/types/string.hpp"
#include "engine/types/number.hpp"
#include "engine/types/json.hpp"
#include "engine/types/pointer.hpp"
#include "engine/types/glm.hpp"

namespace recs
{
class Entity;
}

// Shared by every component header: the component id enum and the handful of
// forward declarations their static hooks take by reference.
namespace bubble
{
using namespace recs;
class Project;

enum class ComponentID
{
	Tag,
	Transform,
	Camera,
	Model,
	Light,
	Shader,
	RigidBody,
	CharacterController,
	Script,
	State
};

}
