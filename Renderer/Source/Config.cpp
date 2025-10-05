module;
#include "nlohmann/json.hpp"
#include "glm/glm.hpp"
#include <fstream>
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
	camera.znear = config["camera"]["znear"];
	camera.zfar = config["camera"]["zfar"];
	camera.fov = config["camera"]["fov"];
	HBAO.Radius = config["HBAOBase"]["radius"];
	HBAO.DirNum = config["HBAOBase"]["dirNum"];
	HBAO.StepNum = config["HBAOBase"]["stepNum"];
    if (config.contains("SdfConfig"))
    {
        const auto& sdfConfig{config["SdfConfig"]};
		Sdf.WorldSize = sdfConfig.value("WorldSize", 2.0f); // 默认值
        Sdf.Resolution = sdfConfig.value("Resolution", 64);
        Sdf.SdfMode = sdfConfig.value("SdfMode", 1);
        Sdf.MeshToSdfMode = sdfConfig.value("MeshToSdfMode", 1);
        Sdf.MeshToSdfIteration = sdfConfig.value("MeshToSdfIteration", 10);
        Sdf.MeshToSdfDistanceMode = sdfConfig.value("MeshToSdfDistanceMode", 1);
        Sdf.MeshToSdfQuality = sdfConfig.value("MeshToSdfQuality", 1);
        Sdf.AnalyticalSampledLevel = sdfConfig.value("AnalyticalSampledLevel", 1);
    }
}