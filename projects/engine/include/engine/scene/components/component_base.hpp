#pragma once
#include <concepts>
#include <sol/forward.hpp>
#include "engine/types/string.hpp"
#include "engine/types/number.hpp"
#include "engine/types/json.hpp"
#include "engine/types/pointer.hpp"
#include "engine/types/glm.hpp"
#include "engine/scene/entity.hpp"

// Shared by every component header: the component id enum and the handful of
// forward declarations their static hooks take by reference.
namespace bubble
{
class Project;
class Scene;
struct InspectorContext;
struct LuaTableRoot;

enum class ComponentID
{
	Tag,
	Transform,
	Camera,
	Model,
	Light,
	Shader,
	Script,
	RigidBody,
	CharacterController,
	State,
	AudioSource,
	AudioListener,
	Animator,
	// Appended, never inserted: these numbers are what a level file lists
	// under "Entity components".
	Hierarchy,
	PrefabInstance,
	Folder
};

// A component type at run time: its ComponentID as an int.
using ComponentTypeId = int;

template <typename T>
concept ComponentType = requires { { T::ID() } -> std::same_as<int>; };

}
