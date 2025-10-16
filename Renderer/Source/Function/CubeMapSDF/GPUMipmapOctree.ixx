module;

#include "vulkan/vulkan.h"
#include <stdint.h>
export module GPUMipmapOctreeMod;

import std;
import DeviceMod;
import BufferMod;
import TextureMod;
import GlmMod;

// Node state enumeration

// Level info structure for uniform buffer (matches shader cbuffer)
export struct LevelInfo
{
    uint32_t current_level; // Current mip level being built
    uint32_t input_size;    // Input texture size
    uint32_t output_size;   // Output texture size
    uint32_t base_size;     // Base texture size (64)
    glm::vec4 pad1;
    glm::vec4 pad2;
    glm::vec4 pad3;
};

// Octree node information for CPU queries


// Configuration for node selection
export struct NodeSelectionConfig
{
    uint32_t minLevel = 2;           // Minimum mip level to extract
    uint32_t maxLevel = 5;           // Maximum mip level to extract
    uint32_t maxNodesPerLevel = 100; // Max nodes per level
    bool includeEmptyNodes = false;  // Include MIXED nodes
    float minNodeSize = 0.1f;        // Minimum node size filter
};

// GPU Mipmap-based Octree
export class GPUMipmapOctree
{
public:
    GPUMipmapOctree(OldVulkanDevice* device, uint32_t mode, uint32_t baseSize = 64, bool useRandom=false);
    ~GPUMipmapOctree();
    void SetVoxelTexture(VkImageView view);
    // Build mipmap octree from 3D binary texture
    void BuildFromVoxelTexture(VkCommandBuffer commandBuffer, Texture *voxelTexture);

    // Build mipmap octree with semaphore optimization (parallel levels)
    void BuildFromVoxelTextureOptimized(VkCommandBuffer commandBuffer, Texture *voxelTexture, VkQueue computeQueue, VkCommandPool computeCommandPool);

    // Debug and validation functions
    void ValidateResults();
    void PrintOctreeStatistics();
    std::vector<uint32_t> ReadMipLevel(uint32_t level);

    // Getters
    uint32_t GetBaseSize() const { return m_baseSize; }
    VkImageView GetMipLevelView(uint32_t level) const;
    VkSampler GetSampler() const { return m_sampler; }
    /// <summary>
    /// 获取4x4x4等级的level
    /// </summary>
    /// <returns></returns>
    uint32_t GetMaxLevel();

private:
    VkImageView voxelTextureView_{};
    // Device reference
    OldVulkanDevice *m_device{};

    // Configuration
    uint32_t m_baseSize;                 // 基础分辨率, 即m_config->Sdf.Resolution
    uint32_t m_maxLevel;                 // 保存最大可以的mip等级
    VkDeviceSize m_alignedLevelInfoSize; // Aligned size of LevelInfo for uniform buffer

    // GPU resources
    std::vector<Texture> m_mipLevels; // Mipmap pyramid textures
    VkSampler m_sampler;              // Sampler for texture reads

    // Uniform buffer for level info
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformBufferMemory;
    void *m_uniformBufferMapped;

    // Compute pipeline
    VkPipeline m_buildPipeline;
    VkPipelineLayout m_pipelineLayout;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorPool m_descriptorPool;
    std::vector<VkDescriptorSet> m_descriptorSets; // One per level

    // Internal methods
    void CreateMipLevels();
    void CreateUniformBuffer();
    void CreateComputePipeline(uint32_t mode);
    void CreateDescriptorSets();
    void UpdateDescriptorSets(Texture *inputTexture);

    // GPU building steps
    void BuildMipLevel(VkCommandBuffer commandBuffer, uint32_t level);
    void BuildMipLevelsBatch(VkCommandBuffer commandBuffer);               // OPTIMIZED: Batch dispatch version
    void BuildMipLevelFast(VkCommandBuffer commandBuffer, uint32_t level); // Fast version without barriers
    void InsertMemoryBarrier(VkCommandBuffer commandBuffer, uint32_t level);
    void InsertBatchBarrier(VkCommandBuffer commandBuffer, uint32_t startLevel, uint32_t endLevel);

    // Helper methods
    uint32_t CalculateMipSize(uint32_t level) const;
    glm::vec3 CalculateNodeCenter(glm::uvec3 position, uint32_t level) const;
    float CalculateNodeSize(uint32_t level) const;

    // Data readback helpers
    void CreateStagingBuffer(VkDeviceSize size, VkBuffer &buffer, VkDeviceMemory &memory);
    void CopyImageToBuffer(uint32_t level, VkBuffer stagingBuffer, uint32_t size);
    void WaitForTransferComplete();
    void ClearAllMipLevels();
    
};