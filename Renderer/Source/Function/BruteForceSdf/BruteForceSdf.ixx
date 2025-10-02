module;

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
        glm::vec3 origin{-2.5f, -2.5f, -2.5f};
        float cellSize{0.078125f}; // 5.0f / 64.0f
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
    // 获取体素的世界坐标
    glm::vec3 GetVoxelWorldPosition(int x, int y, int z) const;

    // 获取体素索引
    size_t GetVoxelIndex(int x, int y, int z) const;

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
    SdfParameters params_;
    glm::mat4 modelToStandard_{1.0f};
    const vkglTF::Model *model_{nullptr};
};