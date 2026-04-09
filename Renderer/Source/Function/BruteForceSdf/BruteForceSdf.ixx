module;
#include <cstdint>

export module BruteForceSdf;

import std;
import Core;
import Logger;
import VkglTFModel;
import GlmMod;

export class BruteForceSdf
{
public:
    // SDF参数结构
    struct SdfParameters
    {
        glm::ivec3 voxelResolution{64, 64, 64};
        glm::vec3 origin{-1.0f, -1.0f, -1.0f}; // 指体素网格的起始位置的世界空间坐标, 如64的网格, 其网格坐标为[0,64]. 其中起始点为[0,0,0], 如果世界尺寸为2.0f,则为[-1,-1,-1]
        float cellSize{2.0f/64.0f}; // 5.0f / 64.0f
        bool signedDistance{false}; // 是否计算有符号距离
    };

public:
    BruteForceSdf() = default;
    ~BruteForceSdf() = default;

    // 初始化
    void Initialize(const SdfParameters &params);

    // 设置GLTF模型数据
    void SetModel(const vkglTF::Model *model);

    // 暴力生成SDF
    void GenerateGroundTruth(std::vector<float>&sdfData);

    // 保存结果到文件
    void SaveToFile(const std::vector<float> &data, const std::string &filename);

    // 从文件加载结果
    std::vector<float> LoadFromFile(const std::string &filename);

    // 对比两个SDF结果
    struct ComparisonResult
    {
        float maxError{0.0f};
        float avgError{0.0f};
        float rmseError{0.0f};
        size_t totalVoxels{0};
        size_t errorVoxels{0}; // 误差超过阈值的体素数
    };

    ComparisonResult CompareResults(const std::vector<float> &groundTruth,
                                    const std::vector<float> &testData,
                                    float errorThreshold = 0.01f);

private:
    struct TriangleData
    {
        glm::vec3 v0{0.0f};
        glm::vec3 v1{0.0f};
        glm::vec3 v2{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec3 boundsMin{0.0f};
        glm::vec3 boundsMax{0.0f};
    };

    struct BvhNode
    {
        glm::vec3 boundsMin{0.0f};
        glm::vec3 boundsMax{0.0f};
        uint32_t leftFirst{0};   // leaf: triangle start index, internal: left child index
        uint32_t rightChild{0};  // internal only
        uint32_t triangleCount{0}; // leaf only, internal node uses 0
    };

    // 获取体素的世界坐标
    glm::vec3 GetVoxelWorldPosition(int x, int y, int z) const;

    // 获取体素索引
    size_t GetVoxelIndex(int x, int y, int z) const;

    // 构建三角形AABB与BVH
    void BuildAccelerationStructure();
    uint32_t BuildBvhRecursive(uint32_t first, uint32_t count);

    // 点到AABB的最小平方距离
    float DistanceSquaredToAabb(const glm::vec3 &point, const glm::vec3 &boundsMin, const glm::vec3 &boundsMax) const;

    // 射线与AABB/三角形相交测试
    bool IntersectRayAabb(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirInv, bool rayDirNegX, bool rayDirNegY, bool rayDirNegZ,
                          const glm::vec3 &boundsMin, const glm::vec3 &boundsMax) const;
    bool IntersectRayTriangle(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir,
                              const TriangleData &triangle, float &t) const;

    // 计算点到模型的最短距离
    float FindClosestDistance(const glm::vec3 &point) const;

    // 计算点到三角形的距离
    float SignedDistancePointToTriangle(const glm::vec3 &point,
                                        const glm::vec3 &v0,
                                        const glm::vec3 &v1,
                                        const glm::vec3 &v2,
                                        const glm::vec3 &normal) const;

    float DistancePointToEdge(const glm::vec3 &point,
                              const glm::vec3 &edgeStart,
                              const glm::vec3 &edgeEnd) const;

    // 判断点是否在网格内部
    bool IsPointInsideMesh(const glm::vec3 &point) const;

private:
    SdfParameters params_{};
    glm::mat4 modelToStandard_{1.0f};
    const vkglTF::Model *model_{nullptr};
    std::vector<TriangleData> triangles_{};
    std::vector<uint32_t> triangleIndices_{};
    std::vector<BvhNode> bvhNodes_{};
    uint32_t rootNode_{0};
    bool accelerationReady_{false};
};
