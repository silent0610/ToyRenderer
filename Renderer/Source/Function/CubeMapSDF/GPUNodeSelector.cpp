module;
#include "vulkan/vulkan.h"
#include <iostream>
#include <fstream>
#include <chrono>
module GPUNodeSelectorMod;

import ToolMod;
import Logger;

// 前向声明
class Renderer;

GPUNodeSelector::GPUNodeSelector(OldVulkanDevice *device)
    : m_device(device)
{
}

GPUNodeSelector::~GPUNodeSelector()
{
    Cleanup();
}

void GPUNodeSelector::Initialize()
{
    CreateBuffers();
    CreatePipeline();
    CreateDescriptorSets();
}

void GPUNodeSelector::CreateBuffers()
{
    // 1. 配置参数buffer - 使用内部结构体以匹配HLSL布局
    VkDeviceSize configSize = 64; // 固定64字节以匹配HLSL中的SelectionConfig结构

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

    // 2. 结果buffer（SelectedNode数组）
    VkDeviceSize resultSize = m_maxNodes * sizeof(SelectedNode);

    bufferInfo.size = resultSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (vkCreateBuffer(m_device->logicalDevice, &bufferInfo, nullptr, &m_resultBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create result buffer");
    }

    vkGetBufferMemoryRequirements(m_device->logicalDevice, m_resultBuffer, &memReq);
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_device->GetMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device->logicalDevice, &allocInfo, nullptr, &m_resultMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate result buffer memory");
    }

    vkBindBufferMemory(m_device->logicalDevice, m_resultBuffer, m_resultMemory, 0);

    // 3. 原子计数器buffer
    bufferInfo.size = sizeof(uint32_t);
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (vkCreateBuffer(m_device->logicalDevice, &bufferInfo, nullptr, &m_counterBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create counter buffer");
    }

    vkGetBufferMemoryRequirements(m_device->logicalDevice, m_counterBuffer, &memReq);
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_device->GetMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device->logicalDevice, &allocInfo, nullptr, &m_counterMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate counter buffer memory");
    }

    vkBindBufferMemory(m_device->logicalDevice, m_counterBuffer, m_counterMemory, 0);

    // 4. 统计信息buffer
    bufferInfo.size = sizeof(SelectionStats);
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (vkCreateBuffer(m_device->logicalDevice, &bufferInfo, nullptr, &m_statsBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create stats buffer");
    }

    vkGetBufferMemoryRequirements(m_device->logicalDevice, m_statsBuffer, &memReq);
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_device->GetMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device->logicalDevice, &allocInfo, nullptr, &m_statsMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate stats buffer memory");
    }

    vkBindBufferMemory(m_device->logicalDevice, m_statsBuffer, m_statsMemory, 0);
}

void GPUNodeSelector::CreatePipeline()
{
    // 1. 创建descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    // Binding 0: 配置参数uniform buffer
    VkDescriptorSetLayoutBinding configBinding{};
    configBinding.binding = 0;
    configBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    configBinding.descriptorCount = 1;
    configBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(configBinding);

    // Binding 1: 八叉树纹理数组
    VkDescriptorSetLayoutBinding octreeBinding{};
    octreeBinding.binding = 1;
    octreeBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    octreeBinding.descriptorCount = 6; // 6个mip levels
    octreeBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(octreeBinding);

    // Binding 2: 结果buffer
    VkDescriptorSetLayoutBinding resultBinding{};
    resultBinding.binding = 2;
    resultBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resultBinding.descriptorCount = 1;
    resultBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(resultBinding);

    // Binding 3: 原子计数器buffer
    VkDescriptorSetLayoutBinding counterBinding{};
    counterBinding.binding = 3;
    counterBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    counterBinding.descriptorCount = 1;
    counterBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(counterBinding);

    // Binding 4: 统计信息buffer
    VkDescriptorSetLayoutBinding statsBinding{};
    statsBinding.binding = 4;
    statsBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    statsBinding.descriptorCount = 1;
    statsBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(statsBinding);

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_device->logicalDevice, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    // 2. 创建pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkCreatePipelineLayout(m_device->logicalDevice, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    // 3. 加载compute shader
    VkShaderModule shaderModule = LoadShaderModule("NodeSelection.Comp.spv");

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    // 4. 创建compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.stage = shaderStageInfo;

    if (vkCreateComputePipelines(m_device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_selectionPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create compute pipeline");
    }

    vkDestroyShaderModule(m_device->logicalDevice, shaderModule, nullptr);
}

void GPUNodeSelector::CreateDescriptorSets()
{
    // 创建descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1});
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 6});  // 6个mip levels
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3}); // result + counter + stats

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(m_device->logicalDevice, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    // 分配descriptor set
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

void GPUNodeSelector::UpdateDescriptorSets(const GPUMipmapOctree &octree)
{
    std::vector<VkWriteDescriptorSet> descriptorWrites;

    // Binding 0: 配置buffer
    VkDescriptorBufferInfo configBufferInfo{};
    configBufferInfo.buffer = m_configBuffer;
    configBufferInfo.offset = 0;
    configBufferInfo.range = 64; // 匹配HLSL结构大小

    VkWriteDescriptorSet configWrite{};
    configWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    configWrite.dstSet = m_descriptorSets[0];
    configWrite.dstBinding = 0;
    configWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    configWrite.descriptorCount = 1;
    configWrite.pBufferInfo = &configBufferInfo;
    descriptorWrites.push_back(configWrite);

    // Binding 1: 八叉树纹理数组 - 绑定所有需要的mip levels
    std::vector<VkDescriptorImageInfo> octreeImageInfos;
    for (uint32_t level = 0; level <= 5; level++)
    { // 绑定所有6个level
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = octree.GetMipLevelView(level);
        imageInfo.sampler = VK_NULL_HANDLE;
        octreeImageInfos.push_back(imageInfo);
    }

    VkWriteDescriptorSet octreeWrite{};
    octreeWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    octreeWrite.dstSet = m_descriptorSets[0];
    octreeWrite.dstBinding = 1;
    octreeWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    octreeWrite.descriptorCount = 6; // 6个mip levels
    octreeWrite.pImageInfo = octreeImageInfos.data();
    descriptorWrites.push_back(octreeWrite);

    // Binding 2: 结果buffer
    VkDescriptorBufferInfo resultBufferInfo{};
    resultBufferInfo.buffer = m_resultBuffer;
    resultBufferInfo.offset = 0;
    resultBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet resultWrite{};
    resultWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    resultWrite.dstSet = m_descriptorSets[0];
    resultWrite.dstBinding = 2;
    resultWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resultWrite.descriptorCount = 1;
    resultWrite.pBufferInfo = &resultBufferInfo;
    descriptorWrites.push_back(resultWrite);

    // Binding 3: 计数器buffer
    VkDescriptorBufferInfo counterBufferInfo{};
    counterBufferInfo.buffer = m_counterBuffer;
    counterBufferInfo.offset = 0;
    counterBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet counterWrite{};
    counterWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    counterWrite.dstSet = m_descriptorSets[0];
    counterWrite.dstBinding = 3;
    counterWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    counterWrite.descriptorCount = 1;
    counterWrite.pBufferInfo = &counterBufferInfo;
    descriptorWrites.push_back(counterWrite);

    // Binding 4: 统计buffer
    VkDescriptorBufferInfo statsBufferInfo{};
    statsBufferInfo.buffer = m_statsBuffer;
    statsBufferInfo.offset = 0;
    statsBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet statsWrite{};
    statsWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    statsWrite.dstSet = m_descriptorSets[0];
    statsWrite.dstBinding = 4;
    statsWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    statsWrite.descriptorCount = 1;
    statsWrite.pBufferInfo = &statsBufferInfo;
    descriptorWrites.push_back(statsWrite);

    vkUpdateDescriptorSets(m_device->logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

std::vector<SelectedNode> GPUNodeSelector::SelectNodes(const GPUMipmapOctree &octree, const GPUSelectionConfig &config)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // 创建与HLSL匹配的配置结构
    struct alignas(16) ShaderConfig
    {
        uint32_t minLevel;
        uint32_t maxLevel;
        uint32_t maxNodes;
        float minNodeSize;

        uint32_t strategy;
        uint32_t reserved1, reserved2, reserved3;

        // 高级参数
        float complexityThreshold;
        float sizeWeight;
        float depthWeight;
        float coverageWeight;
        uint32_t customFlags;
        uint32_t reserved4, reserved5, reserved6;
    };

    ShaderConfig shaderConfig{};
    shaderConfig.minLevel = config.minLevel;
    shaderConfig.maxLevel = config.maxLevel;
    shaderConfig.maxNodes = config.maxNodes;
    shaderConfig.minNodeSize = config.minNodeSize;
    shaderConfig.strategy = static_cast<uint32_t>(config.strategy);
    shaderConfig.complexityThreshold = config.advanced.complexityThreshold;
    shaderConfig.sizeWeight = config.advanced.sizeWeight;
    shaderConfig.depthWeight = config.advanced.depthWeight;
    shaderConfig.coverageWeight = config.advanced.coverageWeight;
    shaderConfig.customFlags = config.advanced.customFlags;

    // 更新配置参数
    memcpy(m_configMapped, &shaderConfig, sizeof(ShaderConfig));

    // 更新descriptor sets
    UpdateDescriptorSets(octree);

    // 执行GPU选择
    ExecuteSelection(config);

    // 读取结果
    std::vector<SelectedNode> results = ReadResults();

    // 计算执行时间
    auto endTime = std::chrono::high_resolution_clock::now();
    m_lastStats.executionTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    return results;
}

void GPUNodeSelector::ExecuteSelection(const GPUSelectionConfig &config)
{
    VkCommandBuffer cmdBuffer = m_device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // TODO: 添加调试标签 (需要完整Renderer声明)
    // if (m_renderer) {
    //     m_renderer->BeginDebugLabel(cmdBuffer, "GPU Node Selection", 0.0f, 1.0f, 0.0f, 1.0f);
    // }

    // 清零计数器和统计buffer
    vkCmdFillBuffer(cmdBuffer, m_counterBuffer, 0, sizeof(uint32_t), 0);
    vkCmdFillBuffer(cmdBuffer, m_statsBuffer, 0, sizeof(SelectionStats), 0);

    // 内存屏障
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    // 绑定compute pipeline
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_selectionPipeline);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSets[0], 0, nullptr);

    // 计算总的节点数量 - 所有层级的节点总和
    uint32_t totalNodes = 0;
    for (uint32_t level = config.minLevel; level <= config.maxLevel; level++)
    {
        uint32_t levelSize = 64 >> (level + 1);
        totalNodes += levelSize * levelSize * levelSize;
    }

    // 每个线程处理一个节点
    uint32_t workGroups = (totalNodes + 63) / 64; // 64 threads per workgroup

    // 执行compute shader
    vkCmdDispatch(cmdBuffer, workGroups, 1, 1);

    // RenderDoc Debug: GPU Node Selection结束
    // TODO: 结束调试标签 (需要完整Renderer声明)
    // if (m_renderer) {
    //     m_renderer->EndDebugLabel(cmdBuffer);
    // }

    vkEndCommandBuffer(cmdBuffer);

    // 提交并等待完成
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

    // 清理
    vkDestroyFence(m_device->logicalDevice, fence, nullptr);
    vkFreeCommandBuffers(m_device->logicalDevice, m_device->commandPool, 1, &cmdBuffer);
}

std::vector<SelectedNode> GPUNodeSelector::ReadResults()
{
    // 创建staging buffer读取结果
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VkDeviceSize bufferSize = m_maxNodes * sizeof(SelectedNode);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(m_device->logicalDevice, &bufferInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(m_device->logicalDevice, stagingBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_device->GetMemoryType(memReq.memoryTypeBits,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(m_device->logicalDevice, &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(m_device->logicalDevice, stagingBuffer, stagingMemory, 0);

    // 拷贝数据
    VkCommandBuffer cmdBuffer = m_device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(cmdBuffer, m_resultBuffer, stagingBuffer, 1, &copyRegion);

    vkEndCommandBuffer(cmdBuffer);

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

    // 读取数据
    void *mappedData;
    vkMapMemory(m_device->logicalDevice, stagingMemory, 0, bufferSize, 0, &mappedData);

    // 读取实际的节点数量 - 从计数器buffer
    VkBuffer counterStagingBuffer;
    VkDeviceMemory counterStagingMemory;
    VkDeviceSize counterSize = sizeof(uint32_t);

    m_device->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           counterSize, &counterStagingBuffer, &counterStagingMemory);

    VkCommandBuffer counterCmdBuffer = m_device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferCopy counterCopyRegion{};
    counterCopyRegion.size = counterSize;
    vkCmdCopyBuffer(counterCmdBuffer, m_counterBuffer, counterStagingBuffer, 1, &counterCopyRegion);

    vkEndCommandBuffer(counterCmdBuffer);

    VkSubmitInfo counterSubmitInfo{};
    counterSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    counterSubmitInfo.commandBufferCount = 1;
    counterSubmitInfo.pCommandBuffers = &counterCmdBuffer;

    VkFence counterFence;
    VkFenceCreateInfo counterFenceInfo{};
    counterFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(m_device->logicalDevice, &counterFenceInfo, nullptr, &counterFence);

    VkQueue counterQueue;
    vkGetDeviceQueue(m_device->logicalDevice, m_device->queueFamilyIndices.graphics, 0, &counterQueue);
    vkQueueSubmit(counterQueue, 1, &counterSubmitInfo, counterFence);
    vkWaitForFences(m_device->logicalDevice, 1, &counterFence, VK_TRUE, UINT64_MAX);

    void *counterMappedData;
    vkMapMemory(m_device->logicalDevice, counterStagingMemory, 0, counterSize, 0, &counterMappedData);
    uint32_t actualCount = *static_cast<uint32_t *>(counterMappedData);
    vkUnmapMemory(m_device->logicalDevice, counterStagingMemory);

    // 清理计数器staging buffer
    vkDestroyFence(m_device->logicalDevice, counterFence, nullptr);
    vkFreeCommandBuffers(m_device->logicalDevice, m_device->commandPool, 1, &counterCmdBuffer);
    vkDestroyBuffer(m_device->logicalDevice, counterStagingBuffer, nullptr);
    vkFreeMemory(m_device->logicalDevice, counterStagingMemory, nullptr);

    // 限制实际读取数量
    actualCount = std::min(actualCount, m_maxNodes);

    std::vector<SelectedNode> results;
    results.resize(actualCount);
    memcpy(results.data(), mappedData, actualCount * sizeof(SelectedNode));

    vkUnmapMemory(m_device->logicalDevice, stagingMemory);

    // 清理
    vkDestroyFence(m_device->logicalDevice, fence, nullptr);
    vkFreeCommandBuffers(m_device->logicalDevice, m_device->commandPool, 1, &cmdBuffer);
    vkDestroyBuffer(m_device->logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(m_device->logicalDevice, stagingMemory, nullptr);

    return results;
}

VkShaderModule GPUNodeSelector::LoadShaderModule(const std::string &filename)
{
    std::string fullPath = Tool::GetShadersPath() + "/" + filename;

    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open shader file: " + fullPath);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
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

void GPUNodeSelector::Cleanup()
{
    if (m_device && m_device->logicalDevice)
    {
        if (m_selectionPipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_device->logicalDevice, m_selectionPipeline, nullptr);
        }
        if (m_pipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(m_device->logicalDevice, m_pipelineLayout, nullptr);
        }
        if (m_descriptorSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(m_device->logicalDevice, m_descriptorSetLayout, nullptr);
        }
        if (m_descriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_device->logicalDevice, m_descriptorPool, nullptr);
        }

        // 清理buffers
        if (m_configMapped)
        {
            vkUnmapMemory(m_device->logicalDevice, m_configMemory);
        }

        if (m_configBuffer != VK_NULL_HANDLE)
            vkDestroyBuffer(m_device->logicalDevice, m_configBuffer, nullptr);
        if (m_configMemory != VK_NULL_HANDLE)
            vkFreeMemory(m_device->logicalDevice, m_configMemory, nullptr);
        if (m_resultBuffer != VK_NULL_HANDLE)
            vkDestroyBuffer(m_device->logicalDevice, m_resultBuffer, nullptr);
        if (m_resultMemory != VK_NULL_HANDLE)
            vkFreeMemory(m_device->logicalDevice, m_resultMemory, nullptr);
        if (m_counterBuffer != VK_NULL_HANDLE)
            vkDestroyBuffer(m_device->logicalDevice, m_counterBuffer, nullptr);
        if (m_counterMemory != VK_NULL_HANDLE)
            vkFreeMemory(m_device->logicalDevice, m_counterMemory, nullptr);
        if (m_statsBuffer != VK_NULL_HANDLE)
            vkDestroyBuffer(m_device->logicalDevice, m_statsBuffer, nullptr);
        if (m_statsMemory != VK_NULL_HANDLE)
            vkFreeMemory(m_device->logicalDevice, m_statsMemory, nullptr);
    }
}

// 预定义策略实现
namespace SelectionStrategies
{
    GPUSelectionConfig CreateBoundarySelector(uint32_t maxNodes)
    {
        GPUSelectionConfig config;
        config.strategy = GPUSelectionConfig::BOUNDARY_ONLY;
        config.maxNodes = maxNodes;
        config.minLevel = 1;
        config.maxLevel = 4;
        return config;
    }

    GPUSelectionConfig CreateSizeBasedSelector(float minSize, float maxSize, uint32_t maxNodes)
    {
        GPUSelectionConfig config;
        config.strategy = GPUSelectionConfig::SIZE_BASED;
        config.maxNodes = maxNodes;
        config.minNodeSize = minSize;
        config.advanced.sizeWeight = 1.0f;
        return config;
    }

    GPUSelectionConfig CreateComplexitySelector(float threshold, uint32_t maxNodes)
    {
        GPUSelectionConfig config;
        config.strategy = GPUSelectionConfig::COMPLEXITY_BASED;
        config.maxNodes = maxNodes;
        config.advanced.complexityThreshold = threshold;
        return config;
    }

    GPUSelectionConfig CreateCustomSelector(const std::string &shaderPath, uint32_t maxNodes)
    {
        GPUSelectionConfig config;
        config.strategy = GPUSelectionConfig::CUSTOM;
        config.maxNodes = maxNodes;
        // 可以通过customFlags传递额外参数
        return config;
    }
}

void GPUNodeSelector::SetRendererPointer(class Renderer *renderer)
{
    m_renderer = renderer;
}

void GPUNodeSelector::SelectNodesGPUOnly(const GPUMipmapOctree &octree, const GPUSelectionConfig &config)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // 创建与HLSL匹配的配置结构
    struct alignas(16) ShaderConfig
    {
        uint32_t minLevel;
        uint32_t maxLevel;
        uint32_t maxNodes;
        float minNodeSize;

        uint32_t strategy;
        uint32_t reserved1, reserved2, reserved3;

        // 高级参数
        float complexityThreshold;
        float sizeWeight;
        float depthWeight;
        float coverageWeight;
        uint32_t customFlags;
        uint32_t reserved4, reserved5, reserved6;
    };

    ShaderConfig shaderConfig{};
    shaderConfig.minLevel = config.minLevel;
    shaderConfig.maxLevel = config.maxLevel;
    shaderConfig.maxNodes = config.maxNodes;
    shaderConfig.minNodeSize = config.minNodeSize;
    shaderConfig.strategy = static_cast<uint32_t>(config.strategy);
    shaderConfig.complexityThreshold = config.advanced.complexityThreshold;
    shaderConfig.sizeWeight = config.advanced.sizeWeight;
    shaderConfig.depthWeight = config.advanced.depthWeight;
    shaderConfig.coverageWeight = config.advanced.coverageWeight;
    shaderConfig.customFlags = config.advanced.customFlags;

    // 更新配置参数
    memcpy(m_configMapped, &shaderConfig, sizeof(ShaderConfig));

    // 更新descriptor sets
    UpdateDescriptorSets(octree);

    // 执行GPU选择
    ExecuteSelection(config);

    // 彻底GPU-only：不读取任何数据到CPU
    // SDF生成器将直接从GPU counter buffer读取节点数量

    // 计算执行时间
    auto endTime = std::chrono::high_resolution_clock::now();
    m_lastStats.executionTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    m_lastStats.selectedCount = 0; // GPU-only模式下不统计具体数量
}

void GPUNodeSelector::RecordSelection(VkCommandBuffer commandBuffer, const GPUMipmapOctree &octree, const GPUSelectionConfig &config)
{
    // 创建与HLSL匹配的配置结构（和SelectNodesGPUOnly相同）
    struct alignas(16) ShaderConfig
    {
        uint32_t minLevel;
        uint32_t maxLevel;
        uint32_t maxNodes;
        float minNodeSize;

        uint32_t strategy;
        uint32_t reserved1, reserved2, reserved3;

        // 高级参数
        float complexityThreshold;
        float sizeWeight;
        float depthWeight;
        float coverageWeight;
        uint32_t customFlags;
        uint32_t reserved4, reserved5, reserved6;
    };

    ShaderConfig shaderConfig{};
    shaderConfig.minLevel = config.minLevel;
    shaderConfig.maxLevel = config.maxLevel;
    shaderConfig.maxNodes = config.maxNodes;
    shaderConfig.minNodeSize = config.minNodeSize;
    shaderConfig.strategy = static_cast<uint32_t>(config.strategy);
    shaderConfig.complexityThreshold = config.advanced.complexityThreshold;
    shaderConfig.sizeWeight = config.advanced.sizeWeight;
    shaderConfig.depthWeight = config.advanced.depthWeight;
    shaderConfig.coverageWeight = config.advanced.coverageWeight;
    shaderConfig.customFlags = config.advanced.customFlags;

    // 更新配置参数
    memcpy(m_configMapped, &shaderConfig, sizeof(ShaderConfig));

    // 更新descriptor sets
    UpdateDescriptorSets(octree);

    // === 只录制命令到传入的commandBuffer，不创建新的，不提交 ===
    
    // 清零计数器和统计buffer
    vkCmdFillBuffer(commandBuffer, m_counterBuffer, 0, sizeof(uint32_t), 0);
    vkCmdFillBuffer(commandBuffer, m_statsBuffer, 0, sizeof(SelectionStats), 0);

    // 内存屏障
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    // 绑定compute pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_selectionPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSets[0], 0, nullptr);

    // 计算总的节点数量 - 所有层级的节点总和
    uint32_t totalNodes = 0;
    for (uint32_t level = config.minLevel; level <= config.maxLevel; level++)
    {
        uint32_t levelSize = 64 >> (level + 1);
        totalNodes += levelSize * levelSize * levelSize;
    }

    // 每个线程处理一个节点
    uint32_t workGroups = (totalNodes + 63) / 64; // 64 threads per workgroup

    // 执行compute shader
    vkCmdDispatch(commandBuffer, workGroups, 1, 1);

    // 注意：不执行vkEndCommandBuffer, vkQueueSubmit, vkWaitForFences
    // 这些由调用方在单一command buffer中统一处理
}
