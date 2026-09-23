module;
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <cstdlib>

module RendererMod;
import std;
import DatasetBench;
import InitMod;

void Renderer::ReloadBenchModel(const std::string &relativePath)
{
    vkDeviceWaitIdle(m_device);
    config_->modelPath = relativePath;

    m_glTFModel.Destroy();
    const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors |
                                      vkglTF::FileLoadingFlags::DontLoadImages;
    m_glTFModel.loadFromFile(Tool::GetAssetsPath() + relativePath, m_vulkanDevice, m_queues.graphicsQueue, glTFLoadingFlags);
    meshToSdfOperator_->SetMesh(&m_glTFModel);

    auto &staticData = m_multiViewDepthSDF4C.staticData;
    staticData.modelPartsBuffer.Destroy();
    staticData.modelPartsBuffer.buffer = VK_NULL_HANDLE;
    staticData.modelPartsBuffer.memory = VK_NULL_HANDLE;
    staticData.modelMatricesBuffer.Destroy();
    staticData.modelMatricesBuffer.buffer = VK_NULL_HANDLE;
    staticData.modelMatricesBuffer.memory = VK_NULL_HANDLE;
    LoadModelStaticData4C();

    auto &gpuPrep = m_multiViewDepthSDF4C.gpuPreparation;
    const VkDeviceSize needed = sizeof(VkDrawIndexedIndirectCommand) * std::max(1u, staticData.totalPartCount);
    if (gpuPrep.indirectCommandsBuffer.buffer == VK_NULL_HANDLE || gpuPrep.indirectCommandsBuffer.size < needed)
    {
        gpuPrep.indirectCommandsBuffer.Destroy();
        gpuPrep.indirectCommandsBuffer.buffer = VK_NULL_HANDLE;
        gpuPrep.indirectCommandsBuffer.memory = VK_NULL_HANDLE;
        Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                       &gpuPrep.indirectCommandsBuffer, needed));
    }

    VkDescriptorBufferInfo partInfo{};
    partInfo.buffer = staticData.modelPartsBuffer.buffer;
    partInfo.offset = 0;
    partInfo.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo indirectInfo{};
    indirectInfo.buffer = gpuPrep.indirectCommandsBuffer.buffer;
    indirectInfo.offset = 0;
    indirectInfo.range = VK_WHOLE_SIZE;
    const VkWriteDescriptorSet writes[] = {
        Init::writeDescriptorSet(gpuPrep.indirectCommandDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &partInfo),
        Init::writeDescriptorSet(gpuPrep.indirectCommandDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &indirectInfo),
    };
    vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);

    vkResetCommandBuffer(m_unifiedGPUPipeline.commandBuffer, 0);
    m_unifiedGPUPipeline.commandsRecorded = false;
    RecordUnifiedGPUPipelineCommands();
    RecordMainCommandBuffer();
}

void Renderer::RunDatasetBench(const DatasetBenchOptions &options)
{
    const std::vector<std::string> models = DatasetBench::ModelPaths(options);
    perfStats_.warmupFrames = options.warmup;
    perfStats_.targetFrames = options.repeat;
    std::cout << "bench per model: warmup " << options.warmup << " frames, measure " << options.repeat << " frames, mean\n";
    InitWindow();
    InitVulkan();
    // 交互模式在收满 targetFrames 时打印统计并写文件。批量自己取平均，避免最后一帧触发它。
    perfStats_.targetFrames = options.repeat + 1;
    bool allMeasured = true;

    for (std::size_t modelIndex = 0; modelIndex < models.size(); ++modelIndex)
    {
        if (modelIndex > 0)
        {
            ReloadBenchModel(models[modelIndex]);
        }

        perfStats_.currentFrame = 0;
        perfStats_.isCollecting = options.warmup == 0;
        perfStats_.isComplete = false;
        perfStats_.meshToSdfTimes.clear();
        perfStats_.unifiedPipelineTimes.clear();

        std::vector<double> jfaSamples;
        std::vector<double> multiViewSamples;
        jfaSamples.reserve(options.repeat);
        multiViewSamples.reserve(options.repeat);

        const uint32_t totalFrames = options.warmup + options.repeat;
        for (uint32_t frame = 0; frame < totalFrames; ++frame)
        {
            glfwPollEvents();
            const std::size_t collected = perfStats_.meshToSdfTimes.size();
            DrawFrame();
            if (perfStats_.meshToSdfTimes.size() == collected)
            {
                continue;
            }
            jfaSamples.push_back(lastJfaMs_);
            multiViewSamples.push_back(lastUnifiedMs_);
        }

        BenchContext context;
        context.model = GetModelNameFromPath(config_->modelPath);
        context.triangles = static_cast<uint32_t>(m_glTFModel.indexBuffer.size() / 3);
        context.resolution = options.resolution;
        context.queries = options.queries;
        const auto jfaMean = DatasetBench::Mean(jfaSamples);
        const auto multiViewMean = DatasetBench::Mean(multiViewSamples);
        context.hasPrecomp = jfaSamples.size() == options.repeat && multiViewSamples.size() == options.repeat && jfaMean.has_value() &&
                             multiViewMean.has_value();
        if (context.hasPrecomp)
        {
            context.jfaPrecompMs = *jfaMean;
            context.multiViewPrecompMs = *multiViewMean;
            std::cout << "bench " << (modelIndex + 1) << "/" << models.size() << " " << context.model << " triangles=" << context.triangles
                      << " frames=" << jfaSamples.size() << " JFA=" << context.jfaPrecompMs << " ms MultiView=" << context.multiViewPrecompMs
                      << " ms\n";
        }
        else
        {
            allMeasured = false;
            std::cout << "bench " << (modelIndex + 1) << "/" << models.size() << " " << context.model << " failed, collected " << jfaSamples.size()
                      << "/" << options.repeat << " frames\n";
        }

        DatasetBench::AppendRows(options.outPath, DatasetBench::MakeRows(context));
    }

    std::cout << "bench csv: " << options.outPath << " models=" << models.size() << "\n" << std::flush;
    vkDeviceWaitIdle(m_device);
    // 短跑之后现有 Cleanup 会在堆检查处中止，交互模式的退出路径保持不动。
    std::_Exit(allMeasured ? EXIT_SUCCESS : EXIT_FAILURE);
}
