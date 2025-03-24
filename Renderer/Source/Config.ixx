module;
export module ConfigMod;
import std;

export struct Config
{
	Config(std::string configPath);
	bool enableValidation;
	std::string modelPath;
	std::string shaderPath;
	
};