module;

#include "CubqlBvhGpu.h"
#include <cstdint>

export module CubqlBvh;

import std;
import VkglTFModel;
import GlmMod;

export struct CubqlBvhTriangleHost
{
    float a[3];
    float b[3];
    float c[3];
};

export class CubqlBvhSdf
{
public:
    CubqlBvhSdf();
    ~CubqlBvhSdf();

    CubqlBvhSdf(const CubqlBvhSdf &) = delete;
    CubqlBvhSdf &operator=(const CubqlBvhSdf &) = delete;

    bool Create();
    void Destroy();
    void SetModel(const vkglTF::Model *model);
    std::uint32_t TriangleCount() const;
    bool Build(float &outBuildMs);
    bool FillVolume(float worldSize, std::uint32_t resolution, std::vector<float> &outVolume, float &outFillMs);

private:
    CubqlBvhContext *ctx_{nullptr};
    std::vector<CubqlBvhTriangleHost> hostTris_;
};
