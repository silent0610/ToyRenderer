module;
#include "vulkan/vulkan.h"
#include <vector>
#include <memory>
#include <functional>
export module GPUNodeSelectorMod;

import std;
import DeviceMod;
import GPUMipmapOctreeMod;
import GlmMod;

// GPU节点选择的配置结构
export struct GPUSelectionConfig {
    // 基础过滤条件
    uint32_t minLevel = 0;
    uint32_t maxLevel = 6;
    uint32_t maxNodes = 1000;
    float minNodeSize = 0.1f;
    
    // 选择策略
    enum SelectionStrategy {
        ALL_SOLID = 0,          // 所有SOLID节点
        BOUNDARY_ONLY = 1,      // 只有边界节点
        SIZE_BASED = 2,         // 基于大小的选择
        COMPLEXITY_BASED = 3,   // 基于几何复杂度
        CUSTOM = 4              // 自定义策略
    } strategy = BOUNDARY_ONLY;
    
    // 扩展参数（用于复杂策略）
    struct {
        float complexityThreshold = 0.5f;  // 复杂度阈值
        float sizeWeight = 1.0f;           // 大小权重
        float depthWeight = 1.0f;          // 深度权重
        float coverageWeight = 1.0f;       // 覆盖范围权重
        uint32_t customFlags = 0;          // 自定义标志位
    } advanced;
};

// GPU选择的输出节点数据
export struct SelectedNode {
    glm::vec3 center;       // 节点中心
    float size;             // 节点大小
    glm::uvec3 position;    // 在八叉树中的位置
    uint32_t level;         // 层级
    uint32_t nodeId;        // 唯一标识
    float score;            // 选择评分
    uint32_t metadata;      // 扩展元数据
    uint32_t reserved;      // 预留字段
};

// 选择统计信息
export struct SelectionStats {
    uint32_t totalCandidates = 0;  // 候选节点总数
    uint32_t selectedCount = 0;    // 实际选中数量
    uint32_t boundaryNodes = 0;    // 边界节点数
    uint32_t interiorNodes = 0;    // 内部节点数
    float executionTime = 0.0f;    // GPU执行时间
    uint32_t memoryUsage = 0;      // 内存使用量
};

// GPU节点选择器类
export class GPUNodeSelector {
public:
    GPUNodeSelector(OldVulkanDevice* device);
    ~GPUNodeSelector();
    
    // 主要接口
    void Initialize();
    std::vector<SelectedNode> SelectNodes(const GPUMipmapOctree& octree, const GPUSelectionConfig& config);
    SelectionStats GetLastSelectionStats() const { return m_lastStats; }
    
    // GPU-to-GPU接口 - 彻底无CPU回读
    VkBuffer GetResultBuffer() const { return m_resultBuffer; }
    VkBuffer GetCounterBuffer() const { return m_counterBuffer; }  // 直接返回GPU计数器buffer
    uint32_t GetMaxNodeCapacity() const { return m_maxNodes; }     // 返回buffer容量，不是实际数量
    void SelectNodesGPUOnly(const GPUMipmapOctree& octree, const GPUSelectionConfig& config);
    
    // 新增：只录制命令，不提交和等待 - 用于单一command buffer优化
    void RecordSelection(VkCommandBuffer commandBuffer, const GPUMipmapOctree& octree, const GPUSelectionConfig& config);
    
    // 设置Renderer指针（用于调试标签）
    void SetRendererPointer(class Renderer* renderer);
    
    // 扩展接口
    void SetCustomShader(const std::string& shaderPath);
    void RegisterSelectionCallback(std::function<bool(const SelectedNode&)> callback);
    
    // 性能优化
    void PreallocateBuffers(uint32_t maxNodes);
    void EnableAsyncExecution(bool enable) { m_asyncExecution = enable; }
    
    // 调试功能
    void DumpSelectionResults(const std::string& filename);
    void EnableDebugMode(bool enable) { m_debugMode = enable; }

private:
    // 核心组件
    OldVulkanDevice* m_device;
    
    // Vulkan资源
    VkPipeline m_selectionPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
    
    // GPU缓冲区
    VkBuffer m_configBuffer = VK_NULL_HANDLE;      // 配置参数
    VkDeviceMemory m_configMemory = VK_NULL_HANDLE;
    void* m_configMapped = nullptr;
    
    VkBuffer m_resultBuffer = VK_NULL_HANDLE;      // 选择结果
    VkDeviceMemory m_resultMemory = VK_NULL_HANDLE;
    
    VkBuffer m_counterBuffer = VK_NULL_HANDLE;     // 原子计数器
    VkDeviceMemory m_counterMemory = VK_NULL_HANDLE;
    
    VkBuffer m_statsBuffer = VK_NULL_HANDLE;       // 统计信息
    VkDeviceMemory m_statsMemory = VK_NULL_HANDLE;
    
    // 状态管理
    SelectionStats m_lastStats;
    uint32_t m_maxNodes = 1000;
    uint32_t m_lastSelectedCount = 0;  // 记录上次选择的节点数量
    bool m_asyncExecution = false;
    
    // Renderer指针（用于调试标签）
    class Renderer* m_renderer = nullptr;
    bool m_debugMode = false;
    
    // 内部实现
    void CreatePipeline();
    void CreateBuffers();
    void CreateDescriptorSets();
    void UpdateDescriptorSets(const GPUMipmapOctree& octree);
    void ExecuteSelection(const GPUSelectionConfig& config);
    std::vector<SelectedNode> ReadResults();
    void Cleanup();
    
    // 着色器管理
    VkShaderModule LoadShaderModule(const std::string& filename);
    void RecompileShaders();
    
    // 性能分析
    void RecordTimestamp(VkCommandBuffer cmd, const std::string& label);
    float CalculateExecutionTime();
};

// 预定义的选择策略
export namespace SelectionStrategies {
    GPUSelectionConfig CreateBoundarySelector(uint32_t maxNodes = 500);
    GPUSelectionConfig CreateSizeBasedSelector(float minSize, float maxSize, uint32_t maxNodes = 300);
    GPUSelectionConfig CreateComplexitySelector(float threshold = 0.5f, uint32_t maxNodes = 200);
    GPUSelectionConfig CreateCustomSelector(const std::string& shaderPath, uint32_t maxNodes = 1000);
}