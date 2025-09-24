module;
#include <cmath>

module LightMod;
import <random>;


void Lights::InitLights(uint32_t num1, bool castShadow1, uint32_t num2, bool castShadow2, uint32_t num3, bool castShadow3)
{
	InitDirLights(num1, castShadow1);
	InitPointLights(num2, castShadow2);
	InitSpotLights(num3, castShadow3);
}
void Lights::InitDirLights(uint32_t num, bool castShadow)
{
	uint32_t cast = 0;
	int index = 0;
	if (castShadow) cast = 1;

	if (num > Lights::MAX_DIR_LIGHT_NUM)
	{
		throw std::runtime_error("Dir Light Too Many");
	}
	if (num == 0) return;
	DirectionalLight light;
	light.castShadow = cast;
	light.color = glm::vec4(1.0f, 0.95f, 0.85f, 0.0f);
	light.direction = glm::vec4(-1.0f, -1.0f, -1.0f, 0.0f);
	light.intensity = 1.0f;
	light.pos = glm::vec4(20.0f, 20.0f, 20.0f, 0.0f);
	if (light.castShadow == 1) light.index = index++;
	DirLights.push_back(light);
	if (num > 1)
	{
		std::default_random_engine engine(0);
		std::uniform_real_distribution<float> randomInten(0.5f, 1.0f);
		std::uniform_real_distribution<float> randomColor(0.1f, 1.0f);
		std::uniform_real_distribution<float> randomDir(-1.0f, 1.0f);
		for (int i = 1; i < num; ++i)
		{
			DirectionalLight light;
			light.castShadow = cast;
			light.color = glm::vec4(randomColor(engine), randomColor(engine), randomColor(engine), 0.0f);
			light.direction = glm::vec4(randomDir(engine), -1.0f, randomDir(engine), 0.0f);
			light.intensity = 3;
			light.pos = -light.direction * 10.0f;
			if (light.castShadow == 1) light.index = index++;
			DirLights.push_back(light);
		}
	}
}
void Lights::InitPointLights(uint32_t num, bool castShadow)
{
	if (num > Lights::MAX_POINT_LIGHT_NUM)
	{
		throw std::runtime_error("Point Light Too Many");
	}
	uint32_t cast = 0;
	if (castShadow)cast = 1;
	std::default_random_engine rndEngine(0);
	std::uniform_real_distribution<float> rndPos(-5.0f, 5.0f);
	std::uniform_real_distribution<float> rndColor(0.1f, 1.0f);

	PointLights.reserve(num);
	for (int i = 0; i < num; ++i)
	{
		PointLights.emplace_back(
			glm::vec4(rndPos(rndEngine), rndPos(rndEngine), rndPos(rndEngine), 1.0f),
			glm::vec4(rndColor(rndEngine), rndColor(rndEngine), rndColor(rndEngine), 1.0f),
			2.0f,
			3.0f,
			cast,
			i
		);
	}

	//PointLights.emplace_back(
	//	glm::vec4(5, 0, 0, 1.0f),
	//	glm::vec4(rndColor(rndEngine), rndColor(rndEngine), rndColor(rndEngine), 1.0f),
	//	3.0f,
	//	1.0f,
	//	cast,
	//	0
	//);
	//PointLights.emplace_back(
	//	glm::vec4(-5, 0, 0, 1.0f),
	//	glm::vec4(rndColor(rndEngine), rndColor(rndEngine), rndColor(rndEngine), 1.0f),
	//	3.0f,
	//	1.0f,
	//	cast,
	//	1
	//);
}

void Lights::InitSpotLights(uint32_t num, bool castShadow)
{
	if (num > Lights::MAX_SPOT_LIGHT_NUM)
	{
		throw std::runtime_error("Point Light Too Many");
	}
	uint32_t cast = 0;
	if (castShadow) cast = 1;
	std::default_random_engine rndEngine(0);
	std::uniform_real_distribution<float> rndPos(-10.0f, 10.0f);
	std::uniform_real_distribution<float> rndColor(-1.0f, 1.0f);

	SpotLights.reserve(num);
	for (int i = 0; i < num; ++i)
	{
		SpotLights.emplace_back(
			glm::vec4(rndPos(rndEngine), rndPos(rndEngine), rndPos(rndEngine), 1.0f),
			glm::vec4(0.0f),
			glm::vec4(rndColor(rndEngine), rndColor(rndEngine), rndColor(rndEngine), 1.0f),
			3.0f,
			glm::radians(30.0f),
			1.0f,
			cast,
			-1
		);
	}
}

Light InitLight(glm::vec3 pos, glm::vec3 target, glm::vec3 color)
{
	Light light;
	light.position = glm::vec4(pos, 1.0f);
	light.target = glm::vec4(target, 0.0f);
	light.color = glm::vec4(color, 0.0f);
	return light;
}