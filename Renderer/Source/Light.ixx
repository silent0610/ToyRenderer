module;
#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"

export module LightMod;

export struct Light
{
	glm::vec4 position;
	glm::vec3 color;
	float radius;
};