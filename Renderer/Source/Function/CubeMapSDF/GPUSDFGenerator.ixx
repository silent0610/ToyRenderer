module;
#include "vulkan/vulkan.h"
#include <vector>
#include <memory>
export module GPUSDFGeneratorMod;

import std;
import DeviceMod;
import GPUNodeSelectorMod;
import GlmMod;

// SDF生成配置 - 简化版本
export struct SDFGenerationConfig {
    uint32_t outputResolution = 256;        // SDF纹理分辨率 (256³)
    float worldScale = 1.0f;                // 世界坐标缩放
};

// SDF生成统计信息
export struct SDFGenerationStats {
    uint32_t processedVoxels = 0;           // 处理的体素数
    float executionTime = 0.0f;             // GPU执行时间
    uint32_t memoryUsage = 0;               // 内存使用量
    uint32_t nodeCount = 0;                 // 处理的节点数
};

// GPU SDF生成器类
export class GPUSDFGenerator {
public:
    GPUSDFGenerator(OldVulkanDevice* device);
    ~GPUSDFGenerator();
    
    // 初始化
    void Initialize();
    
    // 主要接口 - 彻底GPU管线，零CPU回读
    void GenerateSDFFromSelectedNodes(
        const VkBuffer& selectedNodesBuffer,    // GPU节点选择的输出buffer
        const VkBuffer& counterBuffer,          // GPU计数器buffer (真实节点数量)
        uint32_t maxNodeCapacity,               // buffer最大容量 (用于bounds检查)
        const SDFGenerationConfig& config
    );
    
    // 新增：只录制命令，不提交和等待 - 用于单一command buffer优化
    void RecordGeneration(
        VkCommandBuffer commandBuffer,
        const VkBuffer& selectedNodesBuffer,
        const VkBuffer& counterBuffer,
        uint32_t maxNodeCapacity,
        const SDFGenerationConfig& config
    );
    
    // 获取生成的SDF纹理 (供渲染管线直接使用)
    VkImageView GetSDFTexture() const { return m_sdfTextureView; }
    VkImage GetSDFImage() const { return m_sdfTexture; }
    VkSampler GetSDFSampler() const { return m_sdfSampler; }
    
    // 与渲染管线集成
    void BindToRenderPass(VkDescriptorSet descriptorSet, uint32_t binding);
    
    // 性能统计 (异步，可选)
    SDFGenerationStats GetLastStats() const { return m_lastStats; }
    
    // 调试功能 (开发时使用)
    void SaveSDFToFile(const std::string& filename);
    void EnableDebugVisualization(bool enable) { m_debugMode = enable; }
    
    // 设置Renderer指针（用于调试标签）
    void SetRendererPointer(class Renderer* renderer);

private:
    // 核心组件
    OldVulkanDevice* m_device;
    
    // Vulkan资源
    VkPipeline m_sdfGenPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
    
    // SDF输出纹理
    VkImage m_sdfTexture = VK_NULL_HANDLE;
    VkImageView m_sdfTextureView = VK_NULL_HANDLE;
    VkDeviceMemory m_sdfTextureMemory = VK_NULL_HANDLE;
    VkSampler m_sdfSampler = VK_NULL_HANDLE;
    
    // GPU缓冲区
    VkBuffer m_configBuffer = VK_NULL_HANDLE;          // 配置参数
    VkDeviceMemory m_configMemory = VK_NULL_HANDLE;
    void* m_configMapped = nullptr;
    
    VkBuffer m_statsBuffer = VK_NULL_HANDLE;           // 统计信息
    VkDeviceMemory m_statsMemory = VK_NULL_HANDLE;
    
    // 状态管理
    SDFGenerationStats m_lastStats;
    uint32_t m_currentResolution = 256;
    bool m_debugMode = false;
    
    // Renderer指针（用于调试标签）
    class Renderer* m_renderer = nullptr;
    
    // 内部实现
    void CreateSDFTexture(uint32_t resolution);
    void CreatePipeline();
    void CreateBuffers();
    void CreateDescriptorSets();
    void UpdateDescriptorSets(const VkBuffer& selectedNodesBuffer, const VkBuffer& counterBuffer);
    void ExecuteSDFGeneration(const SDFGenerationConfig& config);
    void Cleanup();
    
    // 着色器管理
    VkShaderModule LoadShaderModule(const std::string& filename);
    
    // 图像布局转换
    void TransitionImageLayout(VkImage image, VkFormat format, 
                              VkImageLayout oldLayout, VkImageLayout newLayout);
};

// 预定义的SDF生成策略 - 简化版本
export namespace SDFStrategies {
    SDFGenerationConfig CreateHighRes() { return {512, 1.0f}; }       // 512³高分辨率
    SDFGenerationConfig CreateStandard() { return {256, 1.0f}; }      // 256³标准
    SDFGenerationConfig CreateFast() { return {128, 1.0f}; }          // 128³快速
    SDFGenerationConfig CreateRealtime() { return {64, 1.0f}; }       // 64³实时
}