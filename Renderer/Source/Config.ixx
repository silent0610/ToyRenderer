module;
#include "glm/glm.hpp"
export module ConfigMod;
import std;

export struct Config
{
	Config(std::string configPath);
	bool enableValidation;
	std::string modelPath;
	std::vector<std::string> shadersPath;
	struct
	{
		glm::vec3 pos;
		std::string type;
		std::string flipY;
		float fov;
		float znear;
		float zfar;
	}camera;

	struct
	{
		glm::vec3 pos;
		glm::vec3 color;
	}light;
};