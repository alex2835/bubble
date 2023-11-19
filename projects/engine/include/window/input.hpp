#pragma once 
#include <glm/glm.hpp>

namespace bubble
{

struct MouseInput
{
	glm::vec2 mMousePos = { 0, 0 };
	glm::vec2 mMouseOffset = { 0, 0 };
};

}