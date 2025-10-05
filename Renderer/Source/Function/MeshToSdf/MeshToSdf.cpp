module;
#include "vulkan/vulkan.h"
#include <cmath>
module MeshToSdf;
import Logger;

MeshToSdf::MeshToSdf() : device_{nullptr}, queue_(VK_NULL_HANDLE), currentMesh_(nullptr), sdfTexture_(nullptr)
{
    //sdfParam_ = {};
}
void MeshToSdf::Initialize(OldVulkanDevice* device, VkQueue queue, VkDescriptorPool descriptorPool, vkglTF::Model* mesh)
{
    Log::Info("Initializing MeshToSdf");
    device_ = device;
    queue_ = queue;
    currentMesh_ = mesh;
    worldToLocal_ = currentMesh_->GetModelToStandardTransform();
    // 纹理资源
    CreateSdfResource();

    CreateDescriptorSet(descriptorPool);

    //
    UpdateDescriptorSet();

    // pipeline
    CreateComputePipeline();

    Log::Info("MeshToSdf initialized");
    // 创建管线布局、描述符布局
    // 加载计算着色器，创建11个计算管线
    // 创建描述符池和集合
}

// 2. GenerateSdf 方法 - 执行SDF生成管线
void MeshToSdf::GenerateSdf(VkCommandBuffer cmd)
{
    std::array<VkDescriptorSet, 3> sets{descriptor_.NormalSet, descriptor_.PingSet, descriptor_.JumpPingSet};

    // 准备push constant数据
    MeshToSDFConstant constants{};
    constants.worldToLocal = worldToLocal_;
    constants.voxelResolution = glm::ivec4(sdfParam_.voxelResolution, sdfParam_.voxelResolution, sdfParam_.voxelResolution,
                                           sdfParam_.voxelResolution * sdfParam_.voxelResolution * sdfParam_.voxelResolution);

    // 计算基础参数, 填充push constant
    float cellSize = sdfParam_.size / static_cast<float>(sdfParam_.voxelResolution);
    float maxDistance = glm::length(glm::vec3(sdfParam_.size));
    float initialDistance = maxDistance * 1.01f;
    glm::vec3 origin = glm::vec3(-1.0f);

    constants.maxDistance = maxDistance;
    constants.initialDistance = initialDistance;
    constants.offset = sdfParam_.offset;
    constants.origin = glm::vec4(origin, 0.0f);
    constants.cellSize = cellSize;
    constants.numCellsX = sdfParam_.voxelResolution;
    constants.numCellsY = sdfParam_.voxelResolution;
    constants.numCellsZ = sdfParam_.voxelResolution;
    constants.indexFormat16bit = 0; // index 是否16位
    constants.vertexBufferStride = sizeof(vkglTF::Vertex);
    constants.vertexBufferPosOffset = offsetof(vkglTF::Vertex, pos);
    constants.jumpOffset = 0;
    constants.jumpOffsetInterleaved = glm::ivec4(0);
    constants.dispatchSizeX = (constants.voxelResolution.w + 63) / 64;

    // 内存屏障
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    device_->BeginDebugLabel(cmd, "1.Initializa");
    // 1.Initialize
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.initialize);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelinelayouts_.general, 0, sets.size(), sets.data(), 0, nullptr);
    vkCmdPushConstants(cmd, pipelinelayouts_.general, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MeshToSDFConstant), &constants);
    vkCmdDispatch(cmd, constants.dispatchSizeX, 1, 1);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0,
                         nullptr);
    device_->EndDebugLabel(cmd);

    // 2.SplatTriangle 即得到种子点
    uint32_t triangleCount = static_cast<uint32_t>(currentMesh_->indices.count) / 3;
    uint32_t threadGroupCountTriangles = (triangleCount + 63) / 64;
    VkPipeline splatPipeline{sdfParam_.distanceMode == DistanceMode::Signed && sdfParam_.floodMode == FloodMode::Linear ? pipelines_.splateSigned
                                                                                                                        : pipelines_.splatUnsigned};
    device_->BeginDebugLabel(cmd, "2.SplatTriangle");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, splatPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelinelayouts_.general, 0, sets.size(), sets.data(), 0, nullptr);
    vkCmdDispatch(cmd, threadGroupCountTriangles, 1, 1);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0,
                         nullptr);
    device_->EndDebugLabel(cmd);

    // 3. Finaliz  将uint转换为float的字节流
    device_->BeginDebugLabel(cmd, "3.Finalize");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.finalize);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelinelayouts_.general, 0, sets.size(), sets.data(), 0, nullptr);
    vkCmdDispatch(cmd, constants.dispatchSizeX, 1, 1);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0,
                         nullptr);
    device_->EndDebugLabel(cmd);

    // 4.JumpFlood or LinearFlood
    if (sdfParam_.floodMode == FloodMode::Linear) // linear
    {
        device_->BeginDebugLabel(cmd, "4.LinearFlood");
        VkPipeline floodPipeline{sdfParam_.FloodFillQuality == FloodFillQuality::Normal ? pipelines_.linearFlood : pipelines_.linearFloodUltra};
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, floodPipeline);

        for (int i = 0; i < sdfParam_.floodIterations; ++i)
        {
            // 交换缓冲区 Vkcommand
            int setIndex{i % 2};
            std::array<VkDescriptorSet, 3> currentSets{descriptor_.NormalSet, setIndex == 0 ? descriptor_.PingSet : descriptor_.PongSet,
                                                       setIndex == 0 ? descriptor_.JumpPingSet : descriptor_.JumpPongSet};
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelinelayouts_.general, 0, currentSets.size(), currentSets.data(), 0,
                                    nullptr);
            vkCmdDispatch(cmd, constants.dispatchSizeX, 1, 1);
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0,
                                 nullptr);
        }
        device_->EndDebugLabel(cmd);
    }
    else // jumpflood
    {
        device_->BeginDebugLabel(cmd, "4.JumpFlood");
        // 设置buffer
        // jumpfloodInit
        sets[1] = descriptor_.PingSet;
        sets[2] = descriptor_.JumpPongSet;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.jumpFloodInit);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelinelayouts_.general, 0, sets.size(), sets.data(), 0, nullptr);
        vkCmdDispatch(cmd, constants.dispatchSizeX, 1, 1);
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0,
                             nullptr);

        // 计算跳跃步数
        int maxDim{sdfParam_.voxelResolution};
        int jumpFloodStepCount{static_cast<int>(std::floor(std::log2(maxDim)) - 1)};
        bool bufferFlip{true};
        for (int i = 0; i < jumpFloodStepCount; ++i)
        {
            int jumpOffset{static_cast<int>(std::floor(std::pow(2, jumpFloodStepCount - 1 - i)) + 0.5f)};

            // normal
            if (sdfParam_.FloodFillQuality == FloodFillQuality::Normal)
            {
                for (int j = 0; j < 3; ++j)
                {
                    constants.jumpOffsetInterleaved = glm::ivec4(0);
                    constants.jumpOffsetInterleaved[j] = jumpOffset;
                    sets[2] = bufferFlip == true ? descriptor_.JumpPingSet : descriptor_.JumpPongSet;
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.jumpFloodStep);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelinelayouts_.general, 0, sets.size(), sets.data(), 0, nullptr);
                    vkCmdPushConstants(cmd, pipelinelayouts_.general, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MeshToSDFConstant), &constants);
                    vkCmdDispatch(cmd, constants.dispatchSizeX, 1, 1);
                    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0,
                                         nullptr, 0, nullptr);

                    bufferFlip = !bufferFlip;
                }
            }
            else // ultra
            {
                constants.jumpOffset = jumpOffset;
                sets[2] = bufferFlip == true ? descriptor_.JumpPingSet : descriptor_.JumpPongSet;
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.jumpFloodStepUltra);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelinelayouts_.general, 0, sets.size(), sets.data(), 0, nullptr);
                vkCmdPushConstants(cmd, pipelinelayouts_.general, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MeshToSDFConstant), &constants);
                vkCmdDispatch(cmd, constants.dispatchSizeX, 1, 1);
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0,
                                     nullptr, 0, nullptr);
                bufferFlip = !bufferFlip;
            }
        }
        // jumpFloodFinalize
        sets[1] = descriptor_.PongSet;
        sets[2] = bufferFlip == true ? descriptor_.JumpPingSet : descriptor_.JumpPongSet;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.jumpFloodFinalize);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelinelayouts_.general, 0, sets.size(), sets.data(), 0, nullptr);
        vkCmdDispatch(cmd, constants.dispatchSizeX, 1, 1);
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0,
                             nullptr);
        device_->EndDebugLabel(cmd);
    }

    // 5. bufferToTexture
    device_->BeginDebugLabel(cmd, "5.bufferToTexture");
    sets[1] = descriptor_.PingSet;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.bufferToTexture);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelinelayouts_.general, 0, sets.size(), sets.data(), 0, nullptr);
    constants.offset = (sdfParam_.distanceMode == DistanceMode::Signed && sdfParam_.floodMode != FloodMode::Jump) ? sdfParam_.offset : 0.0f;
    vkCmdPushConstants(cmd, pipelinelayouts_.general, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MeshToSDFConstant), &constants);
    vkCmdDispatch(cmd, constants.dispatchSizeX, 1, 1);
    device_->EndDebugLabel(cmd);
}
// 3. GetSdfTextureView 方法
VkImageView MeshToSdf::GetSdfTextureView() const
{
    return sdfTexture_ ? sdfTexture_->view : VK_NULL_HANDLE;
}

// 4. Cleanup 方法 - 清理所有Vulkan资源
void MeshToSdf::Cleanup()
{
    if (sdfTexture_)
    {
        sdfTexture_->Destroy();
        delete sdfTexture_;
        sdfTexture_ = nullptr;
    }
    // 销毁管线、布局、缓冲区、纹理等
}
Texture3D* MeshToSdf::GetSdfTexture() const
{
    return sdfTexture_;
}
void MeshToSdf::CreateSdfResource()
{
    // 创建3D纹理资源
    sdfTexture_ = new Texture3D{};
    sdfTexture_->Create(sdfParam_.voxelResolution, sdfParam_.voxelResolution, sdfParam_.voxelResolution, device_, queue_, VK_FORMAT_R32_SFLOAT,
                        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_LAYOUT_GENERAL);

    uint32_t voxelCount = sdfParam_.voxelResolution * sdfParam_.voxelResolution * sdfParam_.voxelResolution;
    VkDeviceSize sdfBufferSize = voxelCount * sizeof(float); // float类型SDF缓冲区
    VkDeviceSize jumpBufferSize = voxelCount * sizeof(int);  // int类型跳跃洪水缓冲区

    Tool::CheckResult(device_->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &sdfBuffer_, sdfBufferSize));
    // 4. 创建乒乓SDF缓冲区
    Tool::CheckResult(device_->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &sdfBufferBis_, sdfBufferSize));

    // 5. 根据洪水算法模式创建跳跃缓冲区
    if (sdfParam_.floodMode == FloodMode::Jump)
    {
        // 创建跳跃洪水主缓冲区
        Tool::CheckResult(device_->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &jumpBuffer_, jumpBufferSize));
        // 创建跳跃洪水乒乓缓冲区
        Tool::CheckResult(device_->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &jumpBufferBis_, jumpBufferSize));
    }
    else
    {
        ;
    }

    Log::Debug("SDF resources created successfully.");
}
void MeshToSdf::CreateComputePipeline()
{

    // 创建描述符布局和集合
    std::array<VkDescriptorSetLayout, 3> setLayouts{descriptor_.NormalLayout, descriptor_.PingPongLayout, descriptor_.JumpPingPongLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{Init::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size())};

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.size = sizeof(MeshToSdf::MeshToSDFConstant);

    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    Tool::CheckResult(vkCreatePipelineLayout(device_->logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelinelayouts_.general));

    std::string shaderPath = Tool::GetShadersPath() + "MeshToSdf/";
    CreateSingleComputePipeline(shaderPath + "Initialize.Comp.spv", pipelines_.initialize);
    CreateSingleComputePipeline(shaderPath + "SplatTrianglesSigned.Comp.spv", pipelines_.splateSigned);
    CreateSingleComputePipeline(shaderPath + "SplatTrianglesUnsigned.Comp.spv", pipelines_.splatUnsigned);
    CreateSingleComputePipeline(shaderPath + "Finalize.Comp.spv", pipelines_.finalize);
    CreateSingleComputePipeline(shaderPath + "LinearFloodStep.Comp.spv", pipelines_.linearFlood);
    CreateSingleComputePipeline(shaderPath + "LinearFloodStepUltra.Comp.spv", pipelines_.linearFloodUltra);
    CreateSingleComputePipeline(shaderPath + "JumpFloodInitialize.Comp.spv", pipelines_.jumpFloodInit);
    CreateSingleComputePipeline(shaderPath + "JumpFloodStep.Comp.spv", pipelines_.jumpFloodStep);
    CreateSingleComputePipeline(shaderPath + "JumpFloodStepUltra.Comp.spv", pipelines_.jumpFloodStepUltra);
    CreateSingleComputePipeline(shaderPath + "JumpFloodFinalize.Comp.spv", pipelines_.jumpFloodFinalize);
    CreateSingleComputePipeline(shaderPath + "BufferToTexture.Comp.spv", pipelines_.bufferToTexture);

    Log::Info("All 11 MeshToSDF compute pipelines created");
}
void MeshToSdf::CreateDescriptorSet(VkDescriptorPool descriptorPool)
{
    // 加载11个计算着色器，创建管线
    std::array<VkDescriptorSetLayoutBinding, 4> normalBindings{};
    std::array<VkDescriptorSetLayoutBinding, 2> pingPongBindings{};
    std::array<VkDescriptorSetLayoutBinding, 2> jumpPingPongBindings{};

    // 0: SignedDistanceField
    normalBindings[0] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0);
    // 1: ByteAddressBuffer vertexBuffer
    normalBindings[1] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1);
    // 2: ByteAddressBuffer indexBuffer
    normalBindings[2] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2);
    // 3: RWTexture3D<float> outputTexture
    normalBindings[3] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 3);

    // 0: Ping
    pingPongBindings[0] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0);
    // 1: Pong
    pingPongBindings[1] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1);

    // 0: JumpPing
    jumpPingPongBindings[0] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0);
    // 1: JumpPong
    jumpPingPongBindings[1] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1);

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo =
        Init::descriptorSetLayoutCreateInfo(normalBindings.data(), static_cast<uint32_t>(normalBindings.size()));
    Tool::CheckResult(vkCreateDescriptorSetLayout(device_->logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptor_.NormalLayout));

    descriptorSetLayoutCreateInfo = Init::descriptorSetLayoutCreateInfo(pingPongBindings.data(), static_cast<uint32_t>(pingPongBindings.size()));
    Tool::CheckResult(vkCreateDescriptorSetLayout(device_->logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptor_.PingPongLayout));

    descriptorSetLayoutCreateInfo =
        Init::descriptorSetLayoutCreateInfo(jumpPingPongBindings.data(), static_cast<uint32_t>(jumpPingPongBindings.size()));
    Tool::CheckResult(vkCreateDescriptorSetLayout(device_->logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptor_.JumpPingPongLayout));

    VkDescriptorSetAllocateInfo setAllocateInfo = Init::descriptorSetAllocateInfo(descriptorPool, &descriptor_.NormalLayout, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(device_->logicalDevice, &setAllocateInfo, &descriptor_.NormalSet));

    setAllocateInfo = Init::descriptorSetAllocateInfo(descriptorPool, &descriptor_.PingPongLayout, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(device_->logicalDevice, &setAllocateInfo, &descriptor_.PingSet));
    Tool::CheckResult(vkAllocateDescriptorSets(device_->logicalDevice, &setAllocateInfo, &descriptor_.PongSet));

    setAllocateInfo = Init::descriptorSetAllocateInfo(descriptorPool, &descriptor_.JumpPingPongLayout, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(device_->logicalDevice, &setAllocateInfo, &descriptor_.JumpPingSet));
    Tool::CheckResult(vkAllocateDescriptorSets(device_->logicalDevice, &setAllocateInfo, &descriptor_.JumpPongSet));

    Log::Info("MeshToSDF descriptor sets created successfully");
}

void MeshToSdf::UpdateDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> descriptorWrites;

    // 0: RWStructuredBuffer<uint> SignedDistanceField : register(u0)
    VkWriteDescriptorSet sdfBufferWrite =
        Init::writeDescriptorSet(descriptor_.NormalSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &sdfBuffer_.descriptor);
    descriptorWrites.push_back(sdfBufferWrite);
    // 3: ByteAddressBuffer VertexBuffer : register(t3)
    if (currentMesh_ && currentMesh_->vertices.buffer)
    {
        VkDescriptorBufferInfo vertexBufferInfo{};
        vertexBufferInfo.buffer = currentMesh_->vertices.buffer;
        vertexBufferInfo.offset = 0;
        vertexBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet vertexBufferWrite =
            Init::writeDescriptorSet(descriptor_.NormalSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &vertexBufferInfo);
        descriptorWrites.push_back(vertexBufferWrite);
    }
    // 4: ByteAddressBuffer IndexBuffer : register(t4)
    if (currentMesh_ && currentMesh_->indices.buffer)
    {
        VkDescriptorBufferInfo indexBufferInfo{};
        indexBufferInfo.buffer = currentMesh_->indices.buffer;
        indexBufferInfo.offset = 0;
        indexBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet indexBufferWrite =
            Init::writeDescriptorSet(descriptor_.NormalSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &indexBufferInfo);
        descriptorWrites.push_back(indexBufferWrite);
    }

    // 5: RWTexture3D<float> OutputTexture : register(u5)
    VkWriteDescriptorSet outputTextureWrite =
        Init::writeDescriptorSet(descriptor_.NormalSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3, &sdfTexture_->descriptor);
    descriptorWrites.push_back(outputTextureWrite);

    // PingSet
    descriptorWrites.push_back(Init::writeDescriptorSet(descriptor_.PingSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &sdfBuffer_.descriptor));
    descriptorWrites.push_back(Init::writeDescriptorSet(descriptor_.PingSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &sdfBufferBis_.descriptor));

    // PongSet
    descriptorWrites.push_back(Init::writeDescriptorSet(descriptor_.PongSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &sdfBuffer_.descriptor));
    descriptorWrites.push_back(Init::writeDescriptorSet(descriptor_.PongSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &sdfBufferBis_.descriptor));

    // JumpPingSet
    if (sdfParam_.floodMode == FloodMode::Jump && jumpBuffer_.buffer && jumpBufferBis_.buffer)
    {
        descriptorWrites.push_back(Init::writeDescriptorSet(descriptor_.JumpPingSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &jumpBuffer_.descriptor));
        descriptorWrites.push_back(
            Init::writeDescriptorSet(descriptor_.JumpPingSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &jumpBufferBis_.descriptor));
    }

    // JumpPongSet
    if (sdfParam_.floodMode == FloodMode::Jump && jumpBuffer_.buffer && jumpBufferBis_.buffer)
    {
        descriptorWrites.push_back(Init::writeDescriptorSet(descriptor_.JumpPongSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &jumpBuffer_.descriptor));
        descriptorWrites.push_back(
            Init::writeDescriptorSet(descriptor_.JumpPongSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &jumpBufferBis_.descriptor));
    }

    vkUpdateDescriptorSets(device_->logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}
void MeshToSdf::CreateSingleComputePipeline(const std::string& shaderName, VkPipeline& pipeline)
{
    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStage.module = Tool::LoadShader(shaderName.c_str(), device_->logicalDevice);
    shaderStage.pName = "main";
    if (shaderStage.module == nullptr)
    {
        Log::Error(shaderName + "is nullptr");
    }

    VkComputePipelineCreateInfo computePipelineCreateInfo = Init::computePipelineCreateInfo(pipelinelayouts_.general, 0);
    computePipelineCreateInfo.stage = shaderStage;

    Tool::CheckResult(vkCreateComputePipelines(device_->logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline));
}