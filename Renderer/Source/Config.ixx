module;
export module ConfigMod;
import std;

export struct Config
{
	bool enableValidation;

	Config(std::string configPath);
};