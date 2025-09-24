module;

export module ConfigMod;
import GlmMod;
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
		float movementSpeed;
	}camera;

	struct
	{
		glm::vec3 pos;
		glm::vec3 color;
	}light;
	struct HBAOBase
	{
		float Radius;
		float StepNum;
		float DirNum;
	}HBAO;
};