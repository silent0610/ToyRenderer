module;
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <fstream>

module RendererMod;
import std;
import DatasetBench;
import InitMod;
import CubqlBvh;

namespace
{
struct QuotaQualityMetrics
{
    double rmseAbs{0.0};
    double maeAbs{0.0};
    double maxAbs{0.0};
    double rmseNarrow{0.0};
    uint32_t narrowCount{0};
    uint32_t cameras{0};
};

QuotaQualityMetrics CompareAbsToUnsigned(const std::vector<float> &multiview, const std::vector<float> &gt, float narrowBand)
{
    QuotaQualityMetrics m;
    if (multiview.size() != gt.size() || multiview.empty())
    {
        return m;
    }
    double sumAbs = 0.0;
    double sumSq = 0.0;
    double sumSqNarrow = 0.0;
    for (std::size_t i = 0; i < gt.size(); ++i)
    {
        const double err = std::abs(static_cast<double>(std::abs(multiview[i]) - gt[i]));
        sumAbs += err;
        sumSq += err * err;
        m.maxAbs = std::max(m.maxAbs, err);
        if (gt[i] <= narrowBand)
        {
            sumSqNarrow += err * err;
            ++m.narrowCount;
        }
    }
    const double n = static_cast<double>(gt.size());
    m.maeAbs = sumAbs / n;
    m.rmseAbs = std::sqrt(sumSq / n);
    m.rmseNarrow = m.narrowCount > 0 ? std::sqrt(sumSqNarrow / static_cast<double>(m.narrowCount)) : 0.0;
    return m;
}

void AppendQuotaQualityCsv(const std::string &path, const std::string &model, uint32_t triangles, uint32_t resolution, uint32_t budget,
                           const std::string &variant, const QuotaQualityMetrics &m)
{
    const bool needsHeader = !std::filesystem::exists(path) || std::filesystem::file_size(path) == 0;
    std::ofstream out(path, std::ios::app);
    if (!out)
    {
        throw std::runtime_error("failed to open quality csv: " + path);
    }
    if (needsHeader)
    {
        out << "model,triangles,resolution,budget,variant,cameras,rmse_abs,mae_abs,max_abs,rmse_narrow,narrow_count\n";
    }
    out << model << ',' << triangles << ',' << resolution << ',' << budget << ',' << variant << ',' << m.cameras << ','
        << std::format("{:.6f}", m.rmseAbs) << ',' << std::format("{:.6f}", m.maeAbs) << ',' << std::format("{:.6f}", m.maxAbs) << ','
        << std::format("{:.6f}", m.rmseNarrow) << ',' << m.narrowCount << '\n';
}

std::vector<float> DownloadSdfTexture(OldVulkanDevice *device, VkQueue queue, Texture *texture, VkImageLayout oldLayout)
{
    vkDeviceWaitIdle(device->logicalDevice);
    const uint32_t resolution = texture->dimZ;
    const size_t totalVoxels = static_cast<size_t>(resolution) * resolution * resolution;
    const size_t dataSize = totalVoxels * sizeof(float);

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = dataSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    Tool::CheckResult(vkCreateBuffer(device->logicalDevice, &bufferInfo, nullptr, &stagingBuffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        device->GetMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    Tool::CheckResult(vkAllocateMemory(device->logicalDevice, &allocInfo, nullptr, &stagingMemory));
    Tool::CheckResult(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

    VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture->image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {resolution, resolution, resolution};
    vkCmdCopyImageToBuffer(commandBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

    VkImageMemoryBarrier restore = barrier;
    restore.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    restore.newLayout = oldLayout;
    restore.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    restore.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &restore);
    device->FlushCommandBuffer(commandBuffer, queue, true);

    void *mapped = nullptr;
    Tool::CheckResult(vkMapMemory(device->logicalDevice, stagingMemory, 0, dataSize, 0, &mapped));
    std::vector<float> out(totalVoxels);
    std::memcpy(out.data(), mapped, dataSize);
    vkUnmapMemory(device->logicalDevice, stagingMemory);
    vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
    return out;
}
} // namespace

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
        std::vector<double> stageSamples[7];
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
            for (int stage = 0; stage < 7; ++stage)
            {
                stageSamples[stage].push_back(lastStageMs_[stage]);
            }
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
            for (int stage = 0; stage < 7; ++stage)
            {
                context.stageMs[stage] = *DatasetBench::Mean(stageSamples[stage]);
            }
            std::cout << "bench " << (modelIndex + 1) << "/" << models.size() << " " << context.model
                      << " triangles=" << context.triangles << " frames=" << jfaSamples.size() << "\n"
                      << "  JFA       " << context.jfaPrecompMs << " ms\n"
                      << "  MultiView " << context.multiViewPrecompMs << " ms\n"
                      << "    mark=" << context.stageMs[0] << " fill=" << context.stageMs[1]
                      << " octree=" << context.stageMs[2] << " select=" << context.stageMs[3] << "\n"
                      << "    prepare=" << context.stageMs[4] << " depth=" << context.stageMs[5]
                      << " fusion=" << context.stageMs[6] << "\n";
        }
        else
        {
            allMeasured = false;
            std::cout << "bench " << (modelIndex + 1) << "/" << models.size() << " " << context.model << " failed, collected " << jfaSamples.size()
                      << "/" << options.repeat << " frames\n";
        }

        // BVH: precomp = WideBVH build, eval = fill N^3 unsigned distance.
        {
            CubqlBvhSdf cubql;
            std::vector<double> buildSamples;
            std::vector<double> fillSamples;
            buildSamples.reserve(options.repeat);
            fillSamples.reserve(options.repeat);
            std::vector<float> volume;
            if (cubql.Create())
            {
                cubql.SetModel(&m_glTFModel);
                const uint32_t totalBvh = options.warmup + options.repeat;
                for (uint32_t i = 0; i < totalBvh; ++i)
                {
                    float buildMs = 0.f;
                    float fillMs = 0.f;
                    const bool built = cubql.Build(buildMs);
                    const bool filled = built && cubql.FillVolume(config_->Sdf.WorldSize, options.resolution, volume, fillMs);
                    if (!filled)
                    {
                        break;
                    }
                    if (i >= options.warmup)
                    {
                        buildSamples.push_back(buildMs);
                        fillSamples.push_back(fillMs);
                    }
                }
            }
            const auto buildMean = DatasetBench::Mean(buildSamples);
            const auto fillMean = DatasetBench::Mean(fillSamples);
            context.hasBvh = buildSamples.size() == options.repeat && fillSamples.size() == options.repeat && buildMean.has_value() &&
                             fillMean.has_value();
            if (context.hasBvh)
            {
                context.bvhPrecompMs = *buildMean;
                context.bvhEvalMs = *fillMean;
                std::cout << "  BVH       build=" << context.bvhPrecompMs << " ms  fill=" << context.bvhEvalMs << " ms\n";
            }
            else
            {
                allMeasured = false;
                std::cout << "  BVH failed, collected " << buildSamples.size() << "/" << options.repeat << " samples\n";
            }
        }

        DatasetBench::AppendRows(options.outPath, DatasetBench::MakeRows(context));
    }

    std::cout << "bench csv: " << options.outPath << " models=" << models.size() << "\n" << std::flush;
    vkDeviceWaitIdle(m_device);
    // 短跑之后现有 Cleanup 会在堆检查处中止，交互模式的退出路径保持不动。
    std::_Exit(allMeasured ? EXIT_SUCCESS : EXIT_FAILURE);
}

void Renderer::RunQuotaQualityBench(const DatasetBenchOptions &options)
{
    const std::vector<std::string> models = DatasetBench::ModelPaths(options);
    const uint32_t settleFrames = std::max(3u, options.warmup == 0 ? 3u : std::min(options.warmup, 5u));
    std::vector<uint32_t> budgets;
    if (options.cameras > 0)
    {
        budgets.push_back(options.cameras);
    }
    else
    {
        budgets = {30u, 40u, 50u, 60u};
    }

    std::cout << "quality-quota: legacy(complexity) vs hierarchical(parent quota)\n"
              << "  models=" << models.size() << " resolution=" << options.resolution << " settle_frames=" << settleFrames
              << " MaxChildrenPerParent=" << config_->Sdf.MaxChildrenPerParent << " budgets=";
    for (std::size_t i = 0; i < budgets.size(); ++i)
    {
        std::cout << budgets[i] << (i + 1 < budgets.size() ? "," : "");
    }
    std::cout << "\n";

    // 深度/挑选缓冲按 MultiViewUsedCameraNum 分配，需能盖住最大 budget
    uint32_t maxBudget = 0;
    for (uint32_t b : budgets)
    {
        maxBudget = std::max(maxBudget, b);
    }
    if (config_->Sdf.MultiViewUsedCameraNum < maxBudget)
    {
        config_->Sdf.MultiViewUsedCameraNum = maxBudget;
    }
    config_->Sdf.MaxCameraNum = budgets.front();

    InitWindow();
    InitVulkan();
    perfStats_.targetFrames = settleFrames + 1000;
    perfStats_.warmupFrames = 0;

    if (std::filesystem::exists(options.outPath))
    {
        std::filesystem::remove(options.outPath);
    }

    bool allOk = true;
    const float cellSize = config_->Sdf.WorldSize / static_cast<float>(options.resolution);
    const float narrowBand = 2.0f * cellSize;

    for (std::size_t modelIndex = 0; modelIndex < models.size(); ++modelIndex)
    {
        if (modelIndex > 0)
        {
            ReloadBenchModel(models[modelIndex]);
        }

        const std::string modelName = GetModelNameFromPath(config_->modelPath);
        const uint32_t triangles = static_cast<uint32_t>(m_glTFModel.indexBuffer.size() / 3);
        std::cout << "quality " << (modelIndex + 1) << "/" << models.size() << " " << modelName << " triangles=" << triangles << "\n";

        std::vector<float> gt;
        {
            CubqlBvhSdf cubql;
            float buildMs = 0.f;
            float fillMs = 0.f;
            if (!cubql.Create())
            {
                std::cout << "  GT(BVH) create failed\n";
                allOk = false;
                continue;
            }
            cubql.SetModel(&m_glTFModel);
            if (!cubql.Build(buildMs) || !cubql.FillVolume(config_->Sdf.WorldSize, options.resolution, gt, fillMs))
            {
                std::cout << "  GT(BVH) failed\n";
                allOk = false;
                continue;
            }
            std::cout << "  GT(BVH) build=" << buildMs << " ms fill=" << fillMs << " ms\n";
        }

        auto rerecord = [&]() {
            vkDeviceWaitIdle(m_device);
            vkResetCommandBuffer(m_unifiedGPUPipeline.commandBuffer, 0);
            m_unifiedGPUPipeline.commandsRecorded = false;
            RecordUnifiedGPUPipelineCommands();
        };

        auto runVariant = [&](uint32_t budget, bool hierarchical, const char *variantName) -> bool {
            config_->Sdf.MaxCameraNum = budget;
            SetHierarchicalParentQuotaEnabled(hierarchical);
            if (hierarchical)
            {
                SetMaxChildrenPerParent(std::clamp(config_->Sdf.MaxChildrenPerParent, 1u, 8u));
            }
            rerecord();

            for (uint32_t frame = 0; frame < settleFrames; ++frame)
            {
                glfwPollEvents();
                DrawFrame();
            }
            vkDeviceWaitIdle(m_device);
            ReadSelectedCameraCount();

            Texture *tex = GetMultiViewDepthSdfTexture();
            std::vector<float> mv = DownloadSdfTexture(m_vulkanDevice, m_queues.graphicsQueue, tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            delete tex;

            if (mv.size() != gt.size())
            {
                std::cout << "  K=" << budget << " " << variantName << " size mismatch\n";
                return false;
            }

            QuotaQualityMetrics metrics = CompareAbsToUnsigned(mv, gt, narrowBand);
            metrics.cameras = std::min(lastSelectedCameraCount_, budget);
            AppendQuotaQualityCsv(options.outPath, modelName, triangles, options.resolution, budget, variantName, metrics);
            std::cout << "  K=" << budget << " " << variantName << " cameras=" << metrics.cameras << " rmse_abs=" << metrics.rmseAbs
                      << " mae_abs=" << metrics.maeAbs << " rmse_narrow=" << metrics.rmseNarrow << "\n";
            return true;
        };

        for (uint32_t budget : budgets)
        {
            if (!runVariant(budget, false, "legacy_complexity") || !runVariant(budget, true, "hierarchical_quota"))
            {
                allOk = false;
            }
        }
    }

    std::cout << "quality csv: " << options.outPath << "\n" << std::flush;
    vkDeviceWaitIdle(m_device);
    std::_Exit(allOk ? EXIT_SUCCESS : EXIT_FAILURE);
}
