module;
#include "vulkan/vulkan.h"
#include <iostream>
#include <fstream>
#include <chrono>
module GPUSDFGeneratorMod;

import std;
import ToolMod;
import Logger;

// 前向声明
class Renderer;

// 与HLSL匹配的配置结构
struct SDFConfigBuffer
{
    uint32_t outputResolution;
    uint32_t nodeCount;
    float worldScale;
    float reserved;
};

GPUSDFGenerator::GPUSDFGenerator(OldVulkanDevice *device)
    : m_device(device)
{
}

GPUSDFGenerator::~GPUSDFGenerator()
{
    Cleanup();
}

void GPUSDFGenerator::Initialize()
{

    CreateBuffers();
    CreateSDFTexture(128); // 更现实的128³分辨率 (减少8倍计算量)
    CreatePipeline();
    CreateDescriptorSets();

}

void GPUSDFGenerator::CreateSDFTexture(uint32_t resolution)
{
    m_currentResolution = resolution;

    // 清理旧纹理
    if (m_sdfTexture != VK_NULL_HANDLE)
    {
        vkDestroyImageView(m_device->logicalDevice, m_sdfTextureView, nullptr);
        vkDestroyImage(m_device->logicalDevice, m_sdfTexture, nullptr);
        vkFreeMemory(m_device->logicalDevice, m_sdfTextureMemory, nullptr);
        vkDestroySampler(m_device->logicalDevice, m_sdfSampler, nullptr);
    }

    // 创建3D纹理
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_3D;
    imageInfo.format = VK_FORMAT_R32_SFLOAT; // 32位浮点SDF值
    imageInfo.extent = {resolution, resolution, resolution};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(m_device->logicalDevice, &imageInfo, nullptr, &m_sdfTexture) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SDF texture");
    }

    // 分配内存
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device->logicalDevice, m_sdfTexture, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_device->GetMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device->logicalDevice, &allocInfo, nullptr, &m_sdfTextureMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate SDF texture memory");
    }

    vkBindImageMemory(m_device->logicalDevice, m_sdfTexture, m_sdfTextureMemory, 0);

    // 创建Image View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_sdfTexture;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    viewInfo.format = VK_FORMAT_R32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device->logicalDevice, &viewInfo, nullptr, &m_sdfTextureView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SDF texture view");
    }

    // 创建采样器
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(m_device->logicalDevice, &samplerInfo, nullptr, &m_sdfSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SDF sampler");
    }

    // 立即转换纹理布局到GENERAL，以便compute shader使用
    TransitionImageLayout(m_sdfTexture, VK_FORMAT_R32_SFLOAT,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    std::cout << "Created SDF texture: " << resolution << "³ (" << (memRequirements.size / 1024 / 1024) << " MB)" << std::endl;
}

void GPUSDFGenerator::CreateBuffers()
{
    // 配置buffer
    VkDeviceSize configSize = sizeof(SDFConfigBuffer);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = configSize;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device->logicalDevice, &bufferInfo, nullptr, &m_configBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create config buffer");
    }

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(m_device->logicalDevice, m_configBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_device->GetMemoryType(memReq.memoryTypeBits,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_device->logicalDevice, &allocInfo, nullptr, &m_configMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate config buffer memory");
    }

    vkBindBufferMemory(m_device->logicalDevice, m_configBuffer, m_configMemory, 0);
    vkMapMemory(m_device->logicalDevice, m_configMemory, 0, configSize, 0, &m_configMapped);
}

void GPUSDFGenerator::CreatePipeline()
{
    // 加载compute shader
    VkShaderModule computeShader = LoadShaderModule("SDFGeneration.Comp.spv");

    // Descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> bindings(4);

    // b0: config
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // t1: selected nodes
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // u2: counter buffer
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // u3: SDF texture
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_device->logicalDevice, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkCreatePipelineLayout(m_device->logicalDevice, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    // Compute pipeline
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = computeShader;
    shaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = m_pipelineLayout;

    if (vkCreateComputePipelines(m_device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_sdfGenPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create compute pipeline");
    }

    vkDestroyShaderModule(m_device->logicalDevice, computeShader, nullptr);
}

void GPUSDFGenerator::CreateDescriptorSets()
{
    // Descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes(3);
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 2; // nodes buffer + counter buffer
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[2].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(m_device->logicalDevice, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    m_descriptorSets.resize(1);
    if (vkAllocateDescriptorSets(m_device->logicalDevice, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }
}

VkShaderModule GPUSDFGenerator::LoadShaderModule(const std::string &filename)
{
    std::string fullPath = Tool::GetShadersPath() + "/" + filename;
    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open shader file: " + fullPath);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(buffer.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device->logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

void GPUSDFGenerator::GenerateSDFFromSelectedNodes(
    const VkBuffer &selectedNodesBuffer,
    const VkBuffer &counterBuffer,
    uint32_t maxNodeCapacity,
    const SDFGenerationConfig &config)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // 更新配置 - 使用buffer容量而非具体数量
    SDFConfigBuffer configBuffer{};
    configBuffer.outputResolution = config.outputResolution;
    configBuffer.nodeCount = maxNodeCapacity; // 用于bounds检查
    configBuffer.worldScale = config.worldScale;

    memcpy(m_configMapped, &configBuffer, sizeof(SDFConfigBuffer));

    // 更新descriptor sets - 包含counter buffer
    UpdateDescriptorSets(selectedNodesBuffer, counterBuffer);

    // 执行GPU SDF生成
    ExecuteSDFGeneration(config);

    auto endTime = std::chrono::high_resolution_clock::now();
    m_lastStats.executionTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    m_lastStats.nodeCount = 0; // GPU-only模式：不统计具体数量
    m_lastStats.processedVoxels = config.outputResolution * config.outputResolution * config.outputResolution;

    std::cout << "GPU-only SDF Generation: max " << maxNodeCapacity << " nodes capacity → "
              << config.outputResolution << "³ texture in "
              << m_lastStats.executionTime << "ms" << std::endl;
}

void GPUSDFGenerator::UpdateDescriptorSets(const VkBuffer &selectedNodesBuffer, const VkBuffer &counterBuffer)
{
    std::vector<VkWriteDescriptorSet> descriptorWrites(4); // 增加到4个binding

    // Config buffer
    VkDescriptorBufferInfo configBufferInfo{};
    configBufferInfo.buffer = m_configBuffer;
    configBufferInfo.offset = 0;
    configBufferInfo.range = sizeof(SDFConfigBuffer);

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = m_descriptorSets[0];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &configBufferInfo;

    // Selected nodes buffer
    VkDescriptorBufferInfo nodesBufferInfo{};
    nodesBufferInfo.buffer = selectedNodesBuffer;
    nodesBufferInfo.offset = 0;
    nodesBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = m_descriptorSets[0];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &nodesBufferInfo;

    // Counter buffer (binding 2)
    VkDescriptorBufferInfo counterBufferInfo{};
    counterBufferInfo.buffer = counterBuffer;
    counterBufferInfo.offset = 0;
    counterBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = m_descriptorSets[0];
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &counterBufferInfo;

    // SDF texture (binding 3)
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = m_sdfTextureView;
    imageInfo.sampler = VK_NULL_HANDLE;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = m_descriptorSets[0];
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_device->logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void GPUSDFGenerator::SetRendererPointer(class Renderer *renderer)
{
    m_renderer = renderer;
}

void GPUSDFGenerator::ExecuteSDFGeneration(const SDFGenerationConfig &config)
{
    VkCommandBuffer cmdBuffer = m_device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // TODO: 添加调试标签 (需要完整Renderer声明)
    // if (m_renderer) {
    //     m_renderer->BeginDebugLabel(cmdBuffer, "GPU SDF Generation", 0.0f, 0.0f, 1.0f, 1.0f);
    // }

    // 绑定compute pipeline
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_sdfGenPipeline);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout,
                            0, 1, m_descriptorSets.data(), 0, nullptr);

    // Dispatch - 8³ threads per group
    uint32_t groupsPerDim = (config.outputResolution + 7) / 8;
    vkCmdDispatch(cmdBuffer, groupsPerDim, groupsPerDim, groupsPerDim);

    // TODO: 结束调试标签 (需要完整Renderer声明)
    // if (m_renderer) {
    //     m_renderer->EndDebugLabel(cmdBuffer);
    // }

    vkEndCommandBuffer(cmdBuffer);

    // 提交并等待
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkFence fence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(m_device->logicalDevice, &fenceInfo, nullptr, &fence);

    VkQueue queue;
    vkGetDeviceQueue(m_device->logicalDevice, m_device->queueFamilyIndices.graphics, 0, &queue);
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(m_device->logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX);

    // // 清理
    // vkDestroyFence(m_device->logicalDevice, fence, nullptr);
    // vkFreeCommandBuffers(m_device->logicalDevice, m_device->commandPool, 1, &cmdBuffer);
}

void GPUSDFGenerator::Cleanup()
{
    if (!m_device || m_device->logicalDevice == VK_NULL_HANDLE)
        return;

    if (m_configMapped)
    {
        vkUnmapMemory(m_device->logicalDevice, m_configMemory);
        m_configMapped = nullptr;
    }

    if (m_sdfGenPipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(m_device->logicalDevice, m_sdfGenPipeline, nullptr);
    if (m_pipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(m_device->logicalDevice, m_pipelineLayout, nullptr);
    if (m_descriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(m_device->logicalDevice, m_descriptorSetLayout, nullptr);
    if (m_descriptorPool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(m_device->logicalDevice, m_descriptorPool, nullptr);

    if (m_sdfTextureView != VK_NULL_HANDLE)
        vkDestroyImageView(m_device->logicalDevice, m_sdfTextureView, nullptr);
    if (m_sdfTexture != VK_NULL_HANDLE)
        vkDestroyImage(m_device->logicalDevice, m_sdfTexture, nullptr);
    if (m_sdfTextureMemory != VK_NULL_HANDLE)
        vkFreeMemory(m_device->logicalDevice, m_sdfTextureMemory, nullptr);
    if (m_sdfSampler != VK_NULL_HANDLE)
        vkDestroySampler(m_device->logicalDevice, m_sdfSampler, nullptr);

    if (m_configBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_device->logicalDevice, m_configBuffer, nullptr);
    if (m_configMemory != VK_NULL_HANDLE)
        vkFreeMemory(m_device->logicalDevice, m_configMemory, nullptr);
}

void GPUSDFGenerator::TransitionImageLayout(VkImage image, VkFormat format,
                                            VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = m_device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else
    {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFence fence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(m_device->logicalDevice, &fenceInfo, nullptr, &fence);

    VkQueue queue;
    vkGetDeviceQueue(m_device->logicalDevice, m_device->queueFamilyIndices.graphics, 0, &queue);
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(m_device->logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(m_device->logicalDevice, fence, nullptr);
    vkFreeCommandBuffers(m_device->logicalDevice, m_device->commandPool, 1, &commandBuffer);
}

void GPUSDFGenerator::RecordGeneration(
    VkCommandBuffer commandBuffer,
    const VkBuffer &selectedNodesBuffer,
    const VkBuffer &counterBuffer,
    uint32_t maxNodeCapacity,
    const SDFGenerationConfig &config)
{
    // 更新配置 - 使用buffer容量而非具体数量（和GenerateSDFFromSelectedNodes相同）
    SDFConfigBuffer configBuffer{};
    configBuffer.outputResolution = config.outputResolution;
    configBuffer.nodeCount = maxNodeCapacity; // 用于bounds检查
    configBuffer.worldScale = config.worldScale;

    memcpy(m_configMapped, &configBuffer, sizeof(SDFConfigBuffer));

    // 更新descriptor sets - 包含counter buffer
    UpdateDescriptorSets(selectedNodesBuffer, counterBuffer);

    // === 只录制命令到传入的commandBuffer，不创建新的，不提交 ===
    
    // 绑定compute pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_sdfGenPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout,
                            0, 1, m_descriptorSets.data(), 0, nullptr);

    // Dispatch - 8³ threads per group
    uint32_t groupsPerDim = (config.outputResolution + 7) / 8;
    vkCmdDispatch(commandBuffer, groupsPerDim, groupsPerDim, groupsPerDim);

    // 注意：不执行vkEndCommandBuffer, vkQueueSubmit, vkWaitForFences
    // 这些由调用方在单一command buffer中统一处理
}