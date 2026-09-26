module;

#include "CubqlBvhGpu.h"
#include <cstdint>

module CubqlBvh;

CubqlBvhSdf::CubqlBvhSdf() = default;

CubqlBvhSdf::~CubqlBvhSdf()
{
    Destroy();
}

bool CubqlBvhSdf::Create()
{
    Destroy();
    ctx_ = CubqlBvh_Create();
    return ctx_ != nullptr;
}

void CubqlBvhSdf::Destroy()
{
    if (ctx_)
    {
        CubqlBvh_Destroy(ctx_);
        ctx_ = nullptr;
    }
    hostTris_.clear();
}

void CubqlBvhSdf::SetModel(const vkglTF::Model *model)
{
    hostTris_.clear();
    if (!model)
        return;

    const glm::mat4 modelToStandard = model->GetModelToStandardTransform();
    const auto &vertexBuffer = model->vertexBuffer;
    const auto &indexBuffer = model->indexBuffer;
    hostTris_.reserve(indexBuffer.size() / 3);

    for (size_t i = 0; i + 2 < indexBuffer.size(); i += 3)
    {
        const uint32_t idx0 = indexBuffer[i + 0];
        const uint32_t idx1 = indexBuffer[i + 1];
        const uint32_t idx2 = indexBuffer[i + 2];
        if (idx0 >= vertexBuffer.size() || idx1 >= vertexBuffer.size() || idx2 >= vertexBuffer.size())
            continue;

        const glm::vec3 v0 = glm::vec3(modelToStandard * glm::vec4(vertexBuffer[idx0].pos, 1.0f));
        const glm::vec3 v1 = glm::vec3(modelToStandard * glm::vec4(vertexBuffer[idx1].pos, 1.0f));
        const glm::vec3 v2 = glm::vec3(modelToStandard * glm::vec4(vertexBuffer[idx2].pos, 1.0f));

        CubqlBvhTriangleHost tri{};
        tri.a[0] = v0.x;
        tri.a[1] = v0.y;
        tri.a[2] = v0.z;
        tri.b[0] = v1.x;
        tri.b[1] = v1.y;
        tri.b[2] = v1.z;
        tri.c[0] = v2.x;
        tri.c[1] = v2.y;
        tri.c[2] = v2.z;
        hostTris_.push_back(tri);
    }
}

std::uint32_t CubqlBvhSdf::TriangleCount() const
{
    return static_cast<std::uint32_t>(hostTris_.size());
}

bool CubqlBvhSdf::Build(float &outBuildMs)
{
    outBuildMs = 0.f;
    if (!ctx_ || hostTris_.empty())
        return false;
    return CubqlBvh_Build(ctx_, reinterpret_cast<const CubqlBvhTriangle *>(hostTris_.data()),
                          static_cast<int>(hostTris_.size()), &outBuildMs) == 0;
}

bool CubqlBvhSdf::FillVolume(float worldSize, std::uint32_t resolution, std::vector<float> &outVolume, float &outFillMs)
{
    outFillMs = 0.f;
    if (!ctx_ || resolution < 2)
        return false;
    const size_t cells = size_t(resolution) * resolution * resolution;
    outVolume.resize(cells);
    const float half = 0.5f * worldSize;
    const float cell = worldSize / float(resolution);
    return CubqlBvh_FillVolume(ctx_, -half, -half, -half, cell, static_cast<int>(resolution), outVolume.data(),
                               &outFillMs) == 0;
}
