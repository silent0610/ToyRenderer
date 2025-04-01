module;
#include "nlohmann/json.hpp"
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
	//std::cout << modelPath << std::endl;
}