module;
#include "nlohmann/json.hpp"
#include "glm/glm.hpp"
module ConfigMod;
using Json = nlohmann::json;
Config::Config(std::string configPath)
{
	std::ifstream f(configPath);
	Json config = Json::parse(f);
	f.close();


	enableValidation = config["enableValidation"];
	shadersPath.push_back(config["shaderPath"][0]);
	shadersPath.push_back(config["shaderPath"][1]);
	modelPath = std::string(config["modelPath"][0]);
	camera.pos = glm::vec3(config["camera"]["pos"][0], config["camera"]["pos"][1], config["camera"]["pos"][2]);
	camera.movementSpeed = config["camera"]["movementSpeed"];
	//std::cout << modelPath << std::endl;
}