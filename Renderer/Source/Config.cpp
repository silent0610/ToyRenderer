module;
#include "nlohmann/json.hpp"
#include "glm/glm.hpp"
#include <fstream>
#include "spdlog/spdlog.h"
module ConfigMod;
import Logger;
using Json = nlohmann::json;
Config::Config(std::string configPath)
{
    std::ifstream f(configPath);
    Json config = Json::parse(f, nullptr, true, true);
    f.close();

    enableValidation = config["enableValidation"];
    shadersPath.push_back(config["shaderPath"][0]);
    shadersPath.push_back(config["shaderPath"][1]);
    modelPath = std::string(config["modelPath"][0]);
    camera.pos = glm::vec3(config["camera"]["pos"][0], config["camera"]["pos"][1], config["camera"]["pos"][2]);
    camera.Rotation = glm::vec3(config["camera"]["Rotation"][0], config["camera"]["Rotation"][1], config["camera"]["Rotation"][2]);
    camera.movementSpeed = config["camera"]["movementSpeed"];
    camera.znear = config["camera"]["znear"];
    camera.zfar = config["camera"]["zfar"];
    camera.fov = config["camera"]["fov"];
    HBAO.Radius = config["HBAOBase"]["radius"];
    HBAO.DirNum = config["HBAOBase"]["dirNum"];
    HBAO.StepNum = config["HBAOBase"]["stepNum"];
    if (config.contains("SdfConfig"))
    {
        const auto &sdfConfig{config["SdfConfig"]};
        Sdf.WorldSize = sdfConfig.value("WorldSize", 2.0f); // 默认值
        Sdf.Resolution = sdfConfig.value("Resolution", 64);
        if (Sdf.Resolution < 8)
        {
            Log::Error("SDF Resolution must be at least 8");
            throw std::runtime_error("SDF Resolution must be at least 8");
        }
        Sdf.AnalyticalUsedPointNum = sdfConfig.value("AnalyticalUsedPointNum", 512);
        Sdf.MultiViewDepthResolution = sdfConfig.value("MultiViewDepthResolution", 128);
        Sdf.MultiViewUsedCameraNum = sdfConfig.value("MultiViewUsedCameraNum", 10);
        Sdf.SdfMode = sdfConfig.value("SdfMode", 1);
        Sdf.MaxCameraNum = sdfConfig.value("MaxCameraNum", 30);
        Sdf.MeshToSdfMode = sdfConfig.value("MeshToSdfMode", 1);
        Sdf.MeshToSdfIteration = sdfConfig.value("MeshToSdfIteration", 10);
        Sdf.MeshToSdfDistanceMode = sdfConfig.value("MeshToSdfDistanceMode", 1);
        Sdf.MeshToSdfQuality = sdfConfig.value("MeshToSdfQuality", 1);
        Sdf.SampledLevel = sdfConfig.value("SampledLevel", 1);
        Sdf.SdfAoUseSdfKind = static_cast<SdfKind>(sdfConfig.value("UseSdfKind", 2));
        Sdf.UseRandomSelection = sdfConfig.value("UseRandomSelection", 1);
    }
    spdlog::info("config loadded");
}