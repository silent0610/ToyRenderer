module;
#include "nlohmann/json.hpp"
#include "glm/glm.hpp"
#include <fstream>
#include "spdlog/spdlog.h"
module ConfigMod;
import Logger;
using Json = nlohmann::json;
Config::Config(const std::string& configPath)
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
        Sdf.OctreeResolution = sdfConfig.value("OctreeResolution", sdfConfig.value("VoxelResolution", 32u));
        Sdf.MaxSelectionResolution = sdfConfig.value("MaxSelectionResolution", Sdf.OctreeResolution);
        Sdf.MinSelectionResolution = sdfConfig.value("MinSelectionResolution", 4u);
        const auto powerOfTwo = [](uint32_t value) { return value >= 2 && (value & (value - 1)) == 0; };
        if (!powerOfTwo(Sdf.OctreeResolution) || !powerOfTwo(Sdf.MinSelectionResolution) || !powerOfTwo(Sdf.MaxSelectionResolution) ||
            Sdf.MinSelectionResolution > Sdf.MaxSelectionResolution || Sdf.MaxSelectionResolution > Sdf.OctreeResolution)
        {
            Log::Error("octree and selection resolutions must be powers of two, and min <= max <= octree");
            throw std::runtime_error("invalid selection resolution");
        }
        Sdf.VoxelResolution = Sdf.OctreeResolution;
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
        Sdf.EnableCountSort = sdfConfig.value("EnableCountSort", 1);
        Sdf.EnableCameraOverlay = sdfConfig.value("EnableCameraOverlay", 1);
        Sdf.EnableMultiviewIsoSurface = sdfConfig.value("EnableMultiviewIsoSurface", 0);
        Sdf.EnableSelectionScoreOctreePass = sdfConfig.value("EnableSelectionScoreOctreePass", 0);
        Sdf.EnableHierarchicalParentQuota = sdfConfig.value("EnableHierarchicalParentQuota", 0);
        Sdf.MaxChildrenPerParent = sdfConfig.value("MaxChildrenPerParent", 1);
        Sdf.EnableStableCameraSelection = sdfConfig.value("EnableStableCameraSelection", 1);
        if (Sdf.MaxChildrenPerParent < 1)
        {
            Sdf.MaxChildrenPerParent = 1;
        }
        if (Sdf.MaxChildrenPerParent > 8)
        {
            Sdf.MaxChildrenPerParent = 8;
        }
        Sdf.SdfResolution = sdfConfig.value("SdfResolution", 64);
    }
    if (config.contains("DynamicGeometry"))
    {
        const auto &dynamicConfig{config["DynamicGeometry"]};
        Dynamic.enable = dynamicConfig.value("enable", false);
        Dynamic.animationIndex = dynamicConfig.value("animationIndex", 0u);
        Dynamic.speed = dynamicConfig.value("speed", 1.0f);
        Dynamic.loop = dynamicConfig.value("loop", true);
    }
    spdlog::info("config loadded");
}
