module;
#include <cmath>


export module LightMod;
import GlmMod;
import <vector>;

export struct Light
{
	alignas(16)glm::vec4 position;
	alignas(16)glm::vec4 target;
	alignas(16)glm::vec4 color;
	alignas(16)glm::mat4 viewMatrix;
};
export struct DirectionalLight
{
	alignas(16)glm::vec4 pos{ 0.0f };
	alignas(16)glm::vec4 direction{ 0.0f,-1.0f,0.0f,0.0f };
	alignas(16)glm::vec4 color{ 1.0f,1.0f,1.0f,0.0f };
	alignas(4)float intensity{ 1.0f };
	alignas(4) uint32_t castShadow { 1 };
	alignas(4) int index{ -1 };

};
export struct SpotLight
{
	alignas(16)glm::vec4 position;
	alignas(16)glm::vec4 target; // 单位向量，朝向
	alignas(16)glm::vec4 color;
	alignas(4)float range;
	alignas(4)float angle;    // 半角弧度（cutoff）
	alignas(4)float intensity;
	alignas(4) uint32_t castShadow { 1 };
	alignas(4) int index{ -1 };
};
export struct PointLight
{
	alignas(16)glm::vec4 position;       // 光源位置（世界空间或视空间）
	alignas(16)glm::vec4 color;          // 光的颜色（通常是 RGB 强度）
	alignas(4)float radius;             // 影响范围（用于衰减）
	alignas(4)float intensity;          // 光照强度（乘在颜色上）
	alignas(4) uint32_t castShadow { 1 };
	alignas(4) int index{ -1 };
};

export struct Lights
{
public:
	std::vector<DirectionalLight> DirLights;
	std::vector<PointLight> PointLights;
	std::vector<SpotLight> SpotLights;
	static constexpr uint32_t MAX_DIR_LIGHT_NUM = 4;
	static constexpr uint32_t MAX_POINT_LIGHT_NUM = 100;
	static constexpr uint32_t MAX_SPOT_LIGHT_NUM = 10;

	void InitLights(uint32_t num1, bool castShadow1, uint32_t num2, bool castShadow2, uint32_t num3, bool castShadow3);
	void InitDirLights(uint32_t num, bool castShadow);
	void InitPointLights(uint32_t num, bool castShadow);
	void InitSpotLights(uint32_t num, bool castShadow);
};
export Light InitLight(glm::vec3 pos, glm::vec3 target, glm::vec3 color);