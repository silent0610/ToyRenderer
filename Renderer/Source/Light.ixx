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

struct SpotLight
{
	glm::vec4 position;
	float range;
	glm::vec4 direction; // 单位向量，朝向
	float angle;    // 半角弧度（cutoff）
	glm::vec4 color;
	float intensity;
};
export struct PointLight
{
	int index;
	glm::vec4 position;       // 光源位置（世界空间或视空间）
	float radius;             // 影响范围（用于衰减）
	glm::vec4 color;          // 光的颜色（通常是 RGB 强度）
	float intensity;          // 光照强度（乘在颜色上）
	bool castsShadow;
};
constexpr int MAX_LIGHTS_PER_TILE{ 10 };
export struct TileLightList
{
	uint32_t count;
	uint32_t lightIndices[MAX_LIGHTS_PER_TILE];
};

export Light InitLight(glm::vec3 pos, glm::vec3 target, glm::vec3 color)
{
	Light light;
	light.position = glm::vec4(pos, 1.0f);
	light.target = glm::vec4(target, 0.0f);
	light.color = glm::vec4(color, 0.0f);
	return light;
}