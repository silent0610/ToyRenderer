module;
#include "vulkan/vulkan.h"
module MeshToSdf;
import Logger;

MeshToSdf::MeshToSdf() : device_(nullptr), queue_(VK_NULL_HANDLE), currentMesh_(nullptr), sdfTexture_(nullptr)
{
    sdfParam_ = {};
}
void MeshToSdf::Initialize(OldVulkanDevice *device, VkQueue queue, VkDescriptorPool descriptorPool)
{
    Log::Info("Initializing MeshToSdf");
    device_ = device;
    queue_ = queue;

    // 纹理资源
    CreateSdfResource();

    CreateDescriptorSet(descriptorPool);
    // pipeline
    CreateComputePipeline();

    Log::Info("MeshToSdf initialized");
    // 创建管线布局、描述符布局
    // 加载计算着色器，创建11个计算管线
    // 创建描述符池和集合
}

// 2. GenerateSdf 方法 - 执行SDF生成管线
void MeshToSdf::GenerateSdf(VkCommandBuffer cmd, vkglTF::Model *mesh, const glm::mat4 &worldToLocal)
{
    currentMesh_ = mesh;

    // 准备push constant数据
    MeshToSDFConstant constants{};
    constants.worldToLocal = worldToLocal;
    constants.voxelResolution = glm::ivec4(sdfParam_.voxelResolution, sdfParam_.voxelResolution, sdfParam_.voxelResolution,
        sdfParam_.voxelResolution * sdfParam_.voxelResolution * sdfParam_.voxelResolution);

    // 计算基础参数
    float cellSize = sdfParam_.size / static_cast<float>(sdfParam_.voxelResolution);
    float maxDistance = cellSize * sdfParam_.voxelResolution * 0.5f;
    float initialDistance = maxDistance;
    glm::vec3 origin = glm::vec3(-sdfParam_.size * 0.5f);

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
    constants.dispatchSizeX = (sdfParam_.voxelResolution + 7) / 8;

    // 内存屏障
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // 1. Initialize - 初始化SDF纹理
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.initialize);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelinelayouts_.general, 0, 1, &descriptor_.set, 0,
                            nullptr);
    vkCmdPushConstants(cmd, pipelinelayouts_.general, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MeshToSDFConstant),
                       &constants);
    vkCmdDispatch(cmd, constants.dispatchSizeX, constants.dispatchSizeX, constants.dispatchSizeX);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                         &memoryBarrier, 0, nullptr, 0, nullptr);

    // 2. SplatTrianglesSigned - 将三角形光栅化到SDF中
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.splateSigned);
    uint32_t triangleCount = mesh->indexBuffer.size() / 3;
    
    uint32_t dispatchX = (triangleCount + 63) / 64;
    vkCmdDispatch(cmd, dispatchX, 1, 1);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                         &memoryBarrier, 0, nullptr, 0, nullptr);

    // 3-11. 距离传播算法 (Jump Flood 或 Linear Flood)
    if (sdfParam_.floodMode == FloodMode::Jump)
    {
        // Jump Flood算法的多个pass
        for (int step = sdfParam_.voxelResolution / 2; step >= 1; step /= 2)
        {
            constants.jumpOffset = step;
            vkCmdPushConstants(cmd, pipelinelayouts_.general, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MeshToSDFConstant),
                               &constants);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.jumpFloodStep);
            vkCmdDispatch(cmd, constants.dispatchSizeX, constants.dispatchSizeX, constants.dispatchSizeX);
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                                 &memoryBarrier, 0, nullptr, 0, nullptr);
        }
    }
    else
    {
        // Linear Flood算法，使用linearFlood管线
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.linearFlood);
        vkCmdDispatch(cmd, constants.dispatchSizeX, constants.dispatchSizeX, constants.dispatchSizeX);
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &memoryBarrier, 0, nullptr, 0, nullptr);
    }

    // 最终化处理
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.finalize);
    vkCmdDispatch(cmd, constants.dispatchSizeX, constants.dispatchSizeX, constants.dispatchSizeX);
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

void MeshToSdf::CreateSdfResource()
{
    // 创建3D纹理资源
    sdfTexture_ = new Texture3D{};
    sdfTexture_->Create(sdfParam_.voxelResolution, sdfParam_.voxelResolution, sdfParam_.voxelResolution, device_,
                        queue_, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_IMAGE_LAYOUT_GENERAL);

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
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&descriptor_.layout);

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.size = sizeof(MeshToSdf::MeshToSDFConstant);

    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    Tool::CheckResult(
        vkCreatePipelineLayout(device_->logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelinelayouts_.general));

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
    std::array<VkDescriptorSetLayoutBinding, 9> bindings{};

    // RWStructuredBuffer<uint> sdfBuffer : register(u0);           // 主SDF缓冲区
    // RWStructuredBuffer<float> sdfBufferRW : register(u1);        // 读写SDF缓冲区
    // StructuredBuffer<float> sdfBufferRead : register(t2);        // 只读SDF缓冲区
    // ByteAddressBuffer vertexBuffer : register(t3);               // 顶点缓冲区
    // ByteAddressBuffer indexBuffer : register(t4);                // 索引缓冲区
    // RWTexture3D<float> outputTexture : register(u5);             // 输出3D纹理
    // RWStructuredBuffer<int> jumpBuffer : register(u6);           // 跳跃洪水缓冲区
    // RWStructuredBuffer<int> jumpBufferRW : register(u7);         // 跳跃洪水读写缓冲区
    // StructuredBuffer<int> jumpBufferRead : register(t8);         // 跳跃洪水只读缓冲区

    // 0: RWStructuredBuffer<uint> sdfBuffer
    bindings[0] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0);
    // 1: RWStructuredBuffer<float> sdfBufferRW
    bindings[1] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1);
    // 2: StructuredBuffer<float> sdfBufferRead
    bindings[2] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2);
    // 3: ByteAddressBuffer vertexBuffer
    bindings[3] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3);
    // 4: ByteAddressBuffer indexBuffer
    bindings[4] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4);
    // 5: RWTexture3D<float> outputTexture
    bindings[5] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 5);
    // 6: RWStructuredBuffer<int> jumpBuffer
    bindings[6] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 6);
    // 7: RWStructuredBuffer<int> jumpBufferRW
    bindings[7] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 7);
    // 8: StructuredBuffer<int> jumpBufferRead
    bindings[8] = Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 8);

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo =
        Init::descriptorSetLayoutCreateInfo(bindings.data(), static_cast<uint32_t>(bindings.size()));
    Tool::CheckResult(vkCreateDescriptorSetLayout(device_->logicalDevice, &descriptorSetLayoutCreateInfo, nullptr,
                                                  &descriptor_.layout));

    VkDescriptorSetAllocateInfo setAllocateInfo =
        Init::descriptorSetAllocateInfo(descriptorPool, &descriptor_.layout, 1);

    Tool::CheckResult(vkAllocateDescriptorSets(device_->logicalDevice, &setAllocateInfo, &descriptor_.set));

    Log::Info("MeshToSDF descriptor sets created successfully");
}
void MeshToSdf::CreateSingleComputePipeline(const std::string &shaderName, VkPipeline &pipeline)
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

    VkComputePipelineCreateInfo computePipelineCreateInfo =
        Init::computePipelineCreateInfo(pipelinelayouts_.general, 0);
    computePipelineCreateInfo.stage = shaderStage;

    Tool::CheckResult(vkCreateComputePipelines(device_->logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo,
                                               nullptr, &pipeline));
}