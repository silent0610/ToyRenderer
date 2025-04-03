module;
#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"

export module LightMod;

export struct Light
{
	glm::vec4 position;
	glm::vec4 target;
	glm::vec4 color;
	glm::mat4 viewMatrix;
};

export Light InitLight(glm::vec3 pos, glm::vec3 target, glm::vec3 color)
{
	Light light;
	light.position = glm::vec4(pos, 1.0f);
	light.target = glm::vec4(target, 0.0f);
	light.color = glm::vec4(color, 0.0f);
	return light;
}