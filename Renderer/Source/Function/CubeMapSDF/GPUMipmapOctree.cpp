module;
#include "vulkan/vulkan.h"
#include <assert.h>
#include <cstring>

module GPUMipmapOctreeMod;

import std;
import ToolMod;
import Logger;
GPUMipmapOctree::GPUMipmapOctree(OldVulkanDevice* device, uint32_t mode, uint32_t baseSize, bool useRandom)
    : m_device(device), m_baseSize(baseSize), m_maxLevel(0), m_sampler(VK_NULL_HANDLE), m_uniformBuffer(VK_NULL_HANDLE),
      m_uniformBufferMemory(VK_NULL_HANDLE), m_uniformBufferMapped(nullptr), m_buildPipeline(VK_NULL_HANDLE), m_pipelineLayout(VK_NULL_HANDLE),
      m_descriptorSetLayout(VK_NULL_HANDLE), m_descriptorPool(VK_NULL_HANDLE)
{
    // Calculate number of mip levels
    uint32_t size = baseSize;
    while (size > 4) // 认为小于4, 即2x2x2没有意义了,不再细分
    {
        m_maxLevel++;
        size /= 2;
    }

    if (useRandom)
    {
        m_maxLevel = 0;
        
        // Create sampler
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(m_device->logicalDevice, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create texture sampler!");
        }
        return;
    }
    CreateMipLevels();           // This creates textures and sampler
    CreateUniformBuffer();       // Create uniform buffer for LevelInfo
    CreateDescriptorSets();      // Create descriptor set layout first
    CreateComputePipeline(mode); // Then create pipeline using the layout
}

GPUMipmapOctree::~GPUMipmapOctree()
{
    // Cleanup pipeline
    if (m_buildPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device->logicalDevice, m_buildPipeline, nullptr);
    }

    if (m_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device->logicalDevice, m_pipelineLayout, nullptr);
    }

    // Cleanup descriptor resources
    if (m_descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device->logicalDevice, m_descriptorSetLayout, nullptr);
    }

    if (m_descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device->logicalDevice, m_descriptorPool, nullptr);
    }

    // Cleanup sampler
    if (m_sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(m_device->logicalDevice, m_sampler, nullptr);
    }

    // Cleanup uniform buffer
    if (m_uniformBuffer != VK_NULL_HANDLE)
    {
        if (m_uniformBufferMapped)
        {
            vkUnmapMemory(m_device->logicalDevice, m_uniformBufferMemory);
        }
        vkDestroyBuffer(m_device->logicalDevice, m_uniformBuffer, nullptr);
        vkFreeMemory(m_device->logicalDevice, m_uniformBufferMemory, nullptr);
    }

    // Cleanup mip level textures
    for (auto& texture : m_mipLevels)
    {
        texture.Destroy();
    }
}

void GPUMipmapOctree::CreateMipLevels()
{
    m_mipLevels.resize(m_maxLevel);

    for (uint32_t level = 0; level < m_maxLevel; level++)
    {
        uint32_t mipSize = CalculateMipSize(level);

        // Create 3D texture for this mip level
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        imageInfo.format = VK_FORMAT_R32G32_UINT; // Single channel 32-bit unsigned int to match HLSL RWTexture3D<uint>
        imageInfo.extent = {mipSize, mipSize, mipSize};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(m_device->logicalDevice, &imageInfo, nullptr, &m_mipLevels[level].image) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create mip level texture!");
        }

        // Allocate memory
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device->logicalDevice, m_mipLevels[level].image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->GetMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_device->logicalDevice, &allocInfo, nullptr, &m_mipLevels[level].deviceMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate mip level texture memory!");
        }

        vkBindImageMemory(m_device->logicalDevice, m_mipLevels[level].image, m_mipLevels[level].deviceMemory, 0);

        // Create image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_mipLevels[level].image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        viewInfo.format = VK_FORMAT_R32G32_UINT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device->logicalDevice, &viewInfo, nullptr, &m_mipLevels[level].view) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create mip level image view!");
        }
    }

    // Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(m_device->logicalDevice, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create texture sampler!");
    }

    // Initialize all mip level textures to zero
    ClearAllMipLevels();
}

void GPUMipmapOctree::CreateUniformBuffer()
{

    if (m_maxLevel == 0)
    {
        return;
    }
    // Align each LevelInfo to 64-byte boundary for uniform buffer offset alignment
    const VkDeviceSize alignment = 64; // minUniformBufferOffsetAlignment
    const VkDeviceSize alignedLevelInfoSize = ((sizeof(LevelInfo) + alignment - 1) / alignment) * alignment;
    VkDeviceSize bufferSize = alignedLevelInfoSize * m_maxLevel; // Space for all levels with proper alignment

    // Create uniform buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device->logicalDevice, &bufferInfo, nullptr, &m_uniformBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create uniform buffer!");
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device->logicalDevice, m_uniformBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        m_device->GetMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_device->logicalDevice, &allocInfo, nullptr, &m_uniformBufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate uniform buffer memory!");
    }

    vkBindBufferMemory(m_device->logicalDevice, m_uniformBuffer, m_uniformBufferMemory, 0);

    // Map the memory persistently
    if (vkMapMemory(m_device->logicalDevice, m_uniformBufferMemory, 0, bufferSize, 0, &m_uniformBufferMapped) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to map uniform buffer memory!");
    }
}

void GPUMipmapOctree::CreateComputePipeline(uint32_t mode)
{
    std::string shaderPath = Tool::GetShadersPath();
    // Load compute shader
    if (mode == 1)
    {
        shaderPath = shaderPath + "MipmapOctree/BuildMipmapOctreeComplexity.Comp.spv";
    }
    else if (mode == 0)
    {
        shaderPath = shaderPath + "MipmapOctree/BuildMipmapOctree.Comp.spv";
    }

    // Read shader file
    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        std::string err = "Failed to open shader file: " + shaderPath;
        throw std::runtime_error(err);
        return;
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> shaderCode(fileSize);
    file.seekg(0);
    file.read(shaderCode.data(), fileSize);
    file.close();

    // Create shader module
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device->logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module!");
    }

    // Create pipeline layout (no push constants needed - using uniform buffer)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(m_device->logicalDevice, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    // Create compute pipeline
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.stage = shaderStageInfo;

    if (vkCreateComputePipelines(m_device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_buildPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create compute pipeline!");
    }

    // Cleanup shader module
    vkDestroyShaderModule(m_device->logicalDevice, shaderModule, nullptr);
}

void GPUMipmapOctree::CreateDescriptorSets()
{

    // Create descriptor set layout - Match shader bindings exactly
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    // Binding 0: LevelInfo uniform buffer (cbuffer register(b0))
    VkDescriptorSetLayoutBinding uniformBinding{};
    uniformBinding.binding = 0;
    uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBinding.descriptorCount = 1;
    uniformBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(uniformBinding);

    // Binding 1: Input texture (Texture3D register(t0))
    VkDescriptorSetLayoutBinding inputBinding{};
    inputBinding.binding = 1;
    inputBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    inputBinding.descriptorCount = 1;
    inputBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(inputBinding);

    // Binding 2: Output texture (RWTexture3D register(u1))
    VkDescriptorSetLayoutBinding outputBinding{};
    outputBinding.binding = 2;
    outputBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputBinding.descriptorCount = 1;
    outputBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(outputBinding);

    // Binding 3: Sampler (SamplerState register(s2))
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 3;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(samplerBinding);

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_device->logicalDevice, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    // Create descriptor pool
    // Each descriptor set needs: 1 UNIFORM_BUFFER + 1 SAMPLED_IMAGE + 1 STORAGE_IMAGE + 1 SAMPLER
    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_maxLevel}); // One per set
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_maxLevel});  // One per set
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_maxLevel});  // One per set
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, m_maxLevel});        // One per set

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = m_maxLevel; // One descriptor set per mip level

    if (vkCreateDescriptorPool(m_device->logicalDevice, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor pool!");
    }

    // Allocate descriptor sets (one per mip level)
    m_descriptorSets.resize(m_maxLevel);
    std::vector<VkDescriptorSetLayout> layouts(m_maxLevel, m_descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = m_maxLevel;
    allocInfo.pSetLayouts = layouts.data();

    VkResult result = vkAllocateDescriptorSets(m_device->logicalDevice, &allocInfo, m_descriptorSets.data());
    // std::cout << "  vkAllocateDescriptorSets result: " << result << std::endl;
    if (result != VK_SUCCESS)
    {
        std::cout << "  ERROR: Failed to allocate descriptor sets! Result: " << result << std::endl;
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }
}

void GPUMipmapOctree::BuildFromVoxelTexture(VkCommandBuffer commandBuffer, Texture* voxelTexture)
{
    if (m_maxLevel == 0)
    {
        return;
    }
    if (!m_buildPipeline)
    {
        std::cout << "Warning: Build pipeline not available, skipping octree construction" << std::endl;
        return;
    }

    
    // Update descriptor sets with input voxel texture
    UpdateDescriptorSets(voxelTexture);

    // Input texture already in VK_IMAGE_LAYOUT_GENERAL, no transition needed

    // Transition all mip level textures to general layout
    for (uint32_t level = 0; level < m_maxLevel; level++)
    {
        VkImageMemoryBarrier mipBarrier{};
        mipBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        mipBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        mipBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        mipBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        mipBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        mipBarrier.image = m_mipLevels[level].image;
        mipBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        mipBarrier.subresourceRange.baseMipLevel = 0;
        mipBarrier.subresourceRange.levelCount = 1;
        mipBarrier.subresourceRange.baseArrayLayer = 0;
        mipBarrier.subresourceRange.layerCount = 1;
        mipBarrier.srcAccessMask = 0;
        mipBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &mipBarrier);
    }

    // Bind compute pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_buildPipeline);

    for (uint32_t level = 0; level < m_maxLevel; level++)
    {
        BuildMipLevel(commandBuffer, level);

        // Insert barrier after every level to ensure writes complete before next level reads
        if (level < m_maxLevel - 1)
        {
            InsertMemoryBarrier(commandBuffer, level);
        }
    }
}

uint32_t GPUMipmapOctree::CalculateMipSize(uint32_t level) const
{
    // Level 0 即 base 的下一个等级, base64, 则level0为32
    return m_baseSize >> (level + 1);
}

glm::vec3 GPUMipmapOctree::CalculateNodeCenter(glm::uvec3 position, uint32_t level) const
{
    float nodeSize = CalculateNodeSize(level);
    return glm::vec3(position) * nodeSize + glm::vec3(nodeSize * 0.5f);
}

float GPUMipmapOctree::CalculateNodeSize(uint32_t level) const
{
    return 1.0f * (1 << level); // Assuming base voxel size = 1.0
}

void GPUMipmapOctree::UpdateDescriptorSets(Texture* inputTexture)
{

    for (uint32_t level = 0; level < m_maxLevel; level++)
    {
        std::vector<VkWriteDescriptorSet> descriptorWrites;

        // Binding 0: LevelInfo uniform buffer (each level gets its own offset)
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffer;
        bufferInfo.offset = level * sizeof(LevelInfo); // Different offset for each level (64-byte aligned)
        bufferInfo.range = sizeof(LevelInfo);

        VkWriteDescriptorSet uniformWrite{};
        uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniformWrite.dstSet = m_descriptorSets[level];
        uniformWrite.dstBinding = 0;
        uniformWrite.dstArrayElement = 0;
        uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformWrite.descriptorCount = 1;
        uniformWrite.pBufferInfo = &bufferInfo;
        descriptorWrites.push_back(uniformWrite);

        // Binding 1: Input texture (original voxel texture for level 0, previous mip level for others)
        VkDescriptorImageInfo inputImageInfo{};
        inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        inputImageInfo.imageView = (level == 0) ? inputTexture->view : m_mipLevels[level - 1].view;
        inputImageInfo.sampler = VK_NULL_HANDLE;

        VkWriteDescriptorSet inputWrite{};
        inputWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inputWrite.dstSet = m_descriptorSets[level];
        inputWrite.dstBinding = 1;
        inputWrite.dstArrayElement = 0;
        inputWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        inputWrite.descriptorCount = 1;
        inputWrite.pImageInfo = &inputImageInfo;
        descriptorWrites.push_back(inputWrite);

        // Binding 2: Output texture (current mip level)
        VkDescriptorImageInfo outputImageInfo{};
        outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        outputImageInfo.imageView = m_mipLevels[level].view;
        outputImageInfo.sampler = VK_NULL_HANDLE;

        VkWriteDescriptorSet outputWrite{};
        outputWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        outputWrite.dstSet = m_descriptorSets[level];
        outputWrite.dstBinding = 2;
        outputWrite.dstArrayElement = 0;
        outputWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputWrite.descriptorCount = 1;
        outputWrite.pImageInfo = &outputImageInfo;
        descriptorWrites.push_back(outputWrite);

        // Binding 3: Sampler
        VkDescriptorImageInfo samplerInfo{};
        samplerInfo.sampler = m_sampler;

        VkWriteDescriptorSet samplerWrite{};
        samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        samplerWrite.dstSet = m_descriptorSets[level];
        samplerWrite.dstBinding = 3;
        samplerWrite.dstArrayElement = 0;
        samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        samplerWrite.descriptorCount = 1;
        samplerWrite.pImageInfo = &samplerInfo;
        descriptorWrites.push_back(samplerWrite);

        vkUpdateDescriptorSets(m_device->logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void GPUMipmapOctree::BuildMipLevel(VkCommandBuffer commandBuffer, uint32_t level)
{
    // Correct octree level mapping:
    // Level 0: 64³ → 32³, Level 1: 32³ → 16³, etc.
    uint32_t inputSize = (level == 0) ? m_baseSize : CalculateMipSize(level - 1);
    uint32_t outputSize = CalculateMipSize(level);

    // All textures remain in VK_IMAGE_LAYOUT_GENERAL, no layout transitions needed

    // Update uniform buffer with level info
    LevelInfo levelInfo = {
        level,      // current_level
        inputSize,  // input_size
        outputSize, // output_size
        m_baseSize  // base_size
    };

    // std::cout << "Level " << level << " uniform buffer: current_level=" << levelInfo.current_level
    //           << " input_size=" << levelInfo.input_size
    //           << " output_size=" << levelInfo.output_size
    //           << " base_size=" << levelInfo.base_size << std::endl;

    // Write to the correct offset for this level
    char* bufferPtr = static_cast<char*>(m_uniformBufferMapped);
    memcpy(bufferPtr + (level * sizeof(LevelInfo)), &levelInfo, sizeof(LevelInfo));

    // Bind descriptor set for this level
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSets[level], 0, nullptr);

    // Dispatch compute shader - ensure minimum 1 group per dimension
    uint32_t groupsX = std::max(1u, (outputSize + 7) / 8);
    uint32_t groupsY = std::max(1u, (outputSize + 7) / 8);
    uint32_t groupsZ = std::max(1u, (outputSize + 7) / 8);

    vkCmdDispatch(commandBuffer, groupsX, groupsY, groupsZ);

    //              (groupsX * groupsY * groupsZ) << " work groups" << std::endl;
}

void GPUMipmapOctree::InsertMemoryBarrier(VkCommandBuffer commandBuffer, uint32_t level)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL; // Keep same layout
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_mipLevels[level].image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}



// Batch barrier for multiple levels
void GPUMipmapOctree::InsertBatchBarrier(VkCommandBuffer commandBuffer, uint32_t startLevel, uint32_t endLevel)
{
    std::vector<VkImageMemoryBarrier> barriers;

    for (uint32_t level = startLevel; level <= endLevel; level++)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_mipLevels[level].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        barriers.push_back(barrier);
    }

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                         static_cast<uint32_t>(barriers.size()), barriers.data());
}

// Debug and validation functions
std::vector<uint32_t> GPUMipmapOctree::ReadMipLevel(uint32_t level)
{
    if (level >= m_maxLevel)
    {
        throw std::runtime_error("Invalid mip level");
    }

    uint32_t mipSize = CalculateMipSize(level);
    uint32_t voxelCount = mipSize * mipSize * mipSize;
    VkDeviceSize bufferSize = voxelCount * sizeof(uint8_t); // R8_UINT = 1 byte per voxel

    std::vector<uint32_t> result(voxelCount, 0); // Default to 0 on error

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence transferFence = VK_NULL_HANDLE;

    try
    {
        // Create staging buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device->logicalDevice, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create staging buffer");
        }

        // Allocate memory for staging buffer
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->logicalDevice, stagingBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex =
            m_device->GetMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_device->logicalDevice, &allocInfo, nullptr, &stagingBufferMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate staging buffer memory");
        }

        vkBindBufferMemory(m_device->logicalDevice, stagingBuffer, stagingBufferMemory, 0);

        // Create command buffer
        commandBuffer = m_device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // Transition image layout to transfer source
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_mipLevels[level].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &barrier);

        // Copy image to buffer
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {mipSize, mipSize, mipSize};

        vkCmdCopyImageToBuffer(commandBuffer, m_mipLevels[level].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

        // Transition back to general layout
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &barrier);

        vkEndCommandBuffer(commandBuffer);

        // Create fence for synchronization
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        if (vkCreateFence(m_device->logicalDevice, &fenceInfo, nullptr, &transferFence) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create transfer fence");
        }

        // Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        // Get graphics queue from device
        VkQueue graphicsQueue;
        vkGetDeviceQueue(m_device->logicalDevice, m_device->queueFamilyIndices.graphics, 0, &graphicsQueue);

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, transferFence) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to submit transfer command buffer");
        }

        // Wait for transfer to complete
        if (vkWaitForFences(m_device->logicalDevice, 1, &transferFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for transfer fence");
        }

        // Map memory and copy data
        void* mappedData;
        if (vkMapMemory(m_device->logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &mappedData) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to map staging buffer memory");
        }

        // Convert from uint8_t to uint32_t correctly
        uint8_t* uint8Data = static_cast<uint8_t*>(mappedData);
        for (uint32_t i = 0; i < voxelCount; i++)
        {
            result[i] = static_cast<uint32_t>(uint8Data[i]);
        }

        vkUnmapMemory(m_device->logicalDevice, stagingBufferMemory);
    }
    catch (const std::exception& e)
    {
        std::cout << "ReadMipLevel error: " << e.what() << std::endl;
        // Return vector filled with zeros on error
    }

    // Cleanup resources
    if (transferFence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_device->logicalDevice, transferFence, nullptr);
    }
    if (commandBuffer != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(m_device->logicalDevice, m_device->commandPool, 1, &commandBuffer);
    }
    if (stagingBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(m_device->logicalDevice, stagingBuffer, nullptr);
    }
    if (stagingBufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(m_device->logicalDevice, stagingBufferMemory, nullptr);
    }

    return result;
}



void GPUMipmapOctree::ClearAllMipLevels()
{

    VkCommandBuffer cmdBuffer = m_device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    for (uint32_t level = 0; level < m_maxLevel; level++)
    {
        // Transition image to transfer destination layout
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_mipLevels[level].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        // Clear image to zero
        VkClearColorValue clearColor = {0, 0, 0, 0}; // All zeros
        VkImageSubresourceRange range = {};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        vkCmdClearColorImage(cmdBuffer, m_mipLevels[level].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

        // Transition to general layout for compute shader access
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    vkEndCommandBuffer(cmdBuffer);

    // Submit and wait for completion
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkQueue graphicsQueue;
    vkGetDeviceQueue(m_device->logicalDevice, m_device->queueFamilyIndices.graphics, 0, &graphicsQueue);

    VkFence clearFence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(m_device->logicalDevice, &fenceInfo, nullptr, &clearFence);

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, clearFence);
    vkWaitForFences(m_device->logicalDevice, 1, &clearFence, VK_TRUE, UINT64_MAX);

    // Cleanup
    vkDestroyFence(m_device->logicalDevice, clearFence, nullptr);
    vkFreeCommandBuffers(m_device->logicalDevice, m_device->commandPool, 1, &cmdBuffer);
}

void GPUMipmapOctree::SetVoxelTexture(VkImageView view)
{
    voxelTextureView_ = view;
}
VkImageView GPUMipmapOctree::GetMipLevelView(uint32_t level) const
{
    if (level == 0)
    {
        return voxelTextureView_;
    }
    if (level > m_maxLevel)
    {
        throw std::runtime_error("Invalid mip level");
    }

    return m_mipLevels[level - 1].view;
}

uint32_t GPUMipmapOctree::GetMaxLevel()
{
    return m_maxLevel;
}