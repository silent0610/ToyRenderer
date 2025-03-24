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
}