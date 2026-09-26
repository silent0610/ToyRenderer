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
import BruteForceSdf;
import ToolMod;
import GlmMod;

namespace
{
struct QuotaQualityMetrics
{
    double rmseAbs{0.0};
    double maeAbs{0.0};
    double maxAbs{0.0};
    double rmseNarrow{0.0};
    uint32_t narrowCount{0};
    double signAccuracy{-1.0};
    double signIoU{-1.0};
    bool hasMetrics{false};
};

struct MethodTiming
{
    double totalMs{0.0};
    double stageMs[7]{};
    double bvhBuildMs{0.0};
    double bvhFillMs{0.0};
    double bruteGenMs{0.0};
};

struct QualityRow
{
    std::string method;
    uint32_t cameras{0};
    QuotaQualityMetrics metrics{};
    MethodTiming timing{};
};

QuotaQualityMetrics CompareToSignedGt(const std::vector<float> &pred, const std::vector<float> &gt, float narrowBand, bool computeSign)
{
    QuotaQualityMetrics m;
    if (pred.size() != gt.size() || pred.empty())
    {
        return m;
    }

    double sumAbs = 0.0;
    double sumSq = 0.0;
    double sumSqNarrow = 0.0;
    uint32_t tp = 0;
    uint32_t fp = 0;
    uint32_t fn = 0;
    uint32_t tn = 0;

    for (std::size_t i = 0; i < gt.size(); ++i)
    {
        const double magErr = std::abs(static_cast<double>(std::abs(pred[i]) - std::abs(gt[i])));
        sumAbs += magErr;
        sumSq += magErr * magErr;
        m.maxAbs = std::max(m.maxAbs, magErr);
        if (std::abs(gt[i]) <= narrowBand)
        {
            sumSqNarrow += magErr * magErr;
            ++m.narrowCount;
        }

        if (computeSign)
        {
            const bool predIn = pred[i] < 0.0f;
            const bool gtIn = gt[i] < 0.0f;
            if (predIn && gtIn)
            {
                ++tp;
            }
            else if (predIn && !gtIn)
            {
                ++fp;
            }
            else if (!predIn && gtIn)
            {
                ++fn;
            }
            else
            {
                ++tn;
            }
        }
    }

    const double n = static_cast<double>(gt.size());
    m.maeAbs = sumAbs / n;
    m.rmseAbs = std::sqrt(sumSq / n);
    m.rmseNarrow = m.narrowCount > 0 ? std::sqrt(sumSqNarrow / static_cast<double>(m.narrowCount)) : 0.0;
    if (computeSign)
    {
        m.signAccuracy = static_cast<double>(tp + tn) / n;
        const uint32_t denom = tp + fp + fn;
        m.signIoU = denom > 0 ? static_cast<double>(tp) / static_cast<double>(denom) : 1.0;
    }
    m.hasMetrics = true;
    return m;
}

void AppendQualityCsv(const std::string &path, const std::string &model, uint32_t triangles, uint32_t resolution, uint32_t budget,
                      const QualityRow &row)
{
    const bool needsHeader = !std::filesystem::exists(path) || std::filesystem::file_size(path) == 0;
    std::ofstream out(path, std::ios::app);
    if (!out)
    {
        throw std::runtime_error("failed to open quality csv: " + path);
    }
    if (needsHeader)
    {
        out << "model,triangles,resolution,budget,method,cameras,has_metrics,"
               "rmse_abs,mae_abs,max_abs,rmse_narrow,narrow_count,sign_acc,sign_iou,"
               "total_ms,mark_ms,fill_ms,octree_ms,select_ms,prepare_ms,depth_ms,fusion_ms,"
               "bvh_build_ms,bvh_fill_ms,brute_gen_ms\n";
    }

    const auto fmt = [](double v) { return std::format("{:.6f}", v); };
    out << model << ',' << triangles << ',' << resolution << ',' << budget << ',' << row.method << ',' << row.cameras << ','
        << (row.metrics.hasMetrics ? 1 : 0) << ',' << fmt(row.metrics.rmseAbs) << ',' << fmt(row.metrics.maeAbs) << ','
        << fmt(row.metrics.maxAbs) << ',' << fmt(row.metrics.rmseNarrow) << ',' << row.metrics.narrowCount << ','
        << fmt(row.metrics.signAccuracy) << ',' << fmt(row.metrics.signIoU) << ',' << fmt(row.timing.totalMs) << ','
        << fmt(row.timing.stageMs[0]) << ',' << fmt(row.timing.stageMs[1]) << ',' << fmt(row.timing.stageMs[2]) << ','
        << fmt(row.timing.stageMs[3]) << ',' << fmt(row.timing.stageMs[4]) << ',' << fmt(row.timing.stageMs[5]) << ','
        << fmt(row.timing.stageMs[6]) << ',' << fmt(row.timing.bvhBuildMs) << ',' << fmt(row.timing.bvhFillMs) << ','
        << fmt(row.timing.bruteGenMs) << '\n';
}

bool WriteVolumeRaw(const std::string &path, const std::vector<float> &volume)
{
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return false;
    }
    out.write(reinterpret_cast<const char *>(volume.data()), static_cast<std::streamsize>(volume.size() * sizeof(float)));
    return static_cast<bool>(out);
}

std::string QualityExportDir(const std::string &outPath)
{
    const std::filesystem::path outDir = std::filesystem::path(outPath).parent_path();
    return (outDir.empty() ? std::filesystem::path(".") : outDir).string();
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
    const uint32_t warmupFrames = options.warmup;
    const uint32_t measureFrames = std::max(1u, options.repeat);
    std::vector<uint32_t> budgets;
    if (options.cameras > 0)
    {
        budgets.push_back(options.cameras);
    }
    else
    {
        budgets = {30u, 40u, 50u, 60u};
    }

    std::cout << "quality-quota: methods=multiview,jfa,bvh"
              << " export_sdf=" << (options.exportSdf ? 1 : 0) << " brute_gt=" << options.bruteGt
              << " brute_only=" << (options.bruteOnly ? 1 : 0) << "\n"
              << "  models=" << models.size() << " resolution=" << options.resolution << " warmup=" << warmupFrames
              << " measure=" << measureFrames << " budgets=";
    for (std::size_t i = 0; i < budgets.size(); ++i)
    {
        std::cout << budgets[i] << (i + 1 < budgets.size() ? "," : "");
    }
    std::cout << "\n";

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
    config_->Sdf.SdfResolution = options.resolution;

    InitWindow();
    InitVulkan();
    perfStats_.warmupFrames = 0;
    perfStats_.targetFrames = warmupFrames + measureFrames + 1000;
    perfStats_.isCollecting = true;
    perfStats_.isComplete = false;

    if (std::filesystem::exists(options.outPath))
    {
        std::filesystem::remove(options.outPath);
    }

    bool allOk = true;
    const float cellSize = config_->Sdf.WorldSize / static_cast<float>(options.resolution);
    const float narrowBand = 2.0f * cellSize;
    const std::string exportDir = QualityExportDir(options.outPath);
    const bool wantMetrics = options.bruteGt != "off";

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
        double bruteGenMs = 0.0;
        {
            const uint32_t resolution = options.resolution;
            const float worldSize = config_->Sdf.WorldSize;
            const std::string cachedName = "Signed" + GenerateSdfFileName("BruteSdf");
            const std::string cachedPath = Tool::GetAssetsPath() + "Sdf/" + cachedName;

            BruteForceSdf brute;
            BruteForceSdf::SdfParameters params{};
            params.signedDistance = true;
            params.voxelResolution = glm::ivec3(static_cast<int>(resolution));
            params.origin = glm::vec3(-worldSize * 0.5f);
            params.cellSize = worldSize / static_cast<float>(resolution);
            brute.Initialize(params);

            const bool needGenerate = options.bruteGt == "generate" || options.bruteOnly;
            if (!needGenerate && options.bruteGt == "cache" && std::filesystem::exists(cachedPath))
            {
                gt = brute.LoadFromFile(cachedPath);
                if (gt.size() != static_cast<std::size_t>(resolution) * resolution * resolution)
                {
                    std::cout << "  GT(Brute) cache size mismatch\n";
                    gt.clear();
                    allOk = false;
                }
                else
                {
                    std::cout << "  GT(Brute signed) cache " << cachedName << "\n";
                }
            }
            else if (needGenerate)
            {
                brute.SetModel(&m_glTFModel);
                gt.assign(static_cast<std::size_t>(resolution) * resolution * resolution, std::numeric_limits<float>::max());
                const auto t0 = std::chrono::high_resolution_clock::now();
                brute.GenerateGroundTruth(gt);
                const auto t1 = std::chrono::high_resolution_clock::now();
                bruteGenMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
                brute.SaveToFile(gt, cachedPath);
                std::cout << "  GT(Brute signed) generated " << bruteGenMs << " ms -> " << cachedName << "\n";
            }
            else if (options.bruteGt == "cache")
            {
                std::cout << "  GT(Brute) cache missing: " << cachedPath << " (use --brute-gt generate or --brute-only)\n";
                allOk = false;
            }

            if (wantMetrics && gt.empty())
            {
                allOk = false;
                if (options.bruteOnly)
                {
                    continue;
                }
            }

            if (options.exportSdf && !gt.empty())
            {
                const std::string gtExport = exportDir + "/" + modelName + "_n" + std::to_string(resolution) + "_gt_brute.raw";
                WriteVolumeRaw(gtExport, gt);
            }

            if (options.bruteGt == "generate" || options.bruteOnly)
            {
                QualityRow bruteRow;
                bruteRow.method = "brute_gt";
                bruteRow.timing.totalMs = bruteGenMs;
                bruteRow.timing.bruteGenMs = bruteGenMs;
                if (!gt.empty())
                {
                    bruteRow.metrics = CompareToSignedGt(gt, gt, narrowBand, true);
                }
                AppendQualityCsv(options.outPath, modelName, triangles, options.resolution, 0, bruteRow);
            }
        }

        if (options.bruteOnly)
        {
            continue;
        }

        auto rerecord = [&]() {
            vkDeviceWaitIdle(m_device);
            vkResetCommandBuffer(m_unifiedGPUPipeline.commandBuffer, 0);
            m_unifiedGPUPipeline.commandsRecorded = false;
            RecordUnifiedGPUPipelineCommands();
        };

        auto sampleGpuFrameTimes = [&](uint32_t budget, bool hierarchical, MethodTiming &mvTiming, MethodTiming &jfaTiming) {
            config_->Sdf.MaxCameraNum = budget;
            SetHierarchicalParentQuotaEnabled(hierarchical);
            if (hierarchical)
            {
                SetMaxChildrenPerParent(std::clamp(config_->Sdf.MaxChildrenPerParent, 1u, 8u));
            }
            rerecord();

            std::vector<double> jfaSamples;
            std::vector<double> mvTotalSamples;
            std::vector<double> stageSamples[7];
            jfaSamples.reserve(measureFrames);
            mvTotalSamples.reserve(measureFrames);

            perfStats_.currentFrame = 0;
            perfStats_.isCollecting = true;
            for (uint32_t frame = 0; frame < warmupFrames + measureFrames; ++frame)
            {
                glfwPollEvents();
                DrawFrame();
                if (frame < warmupFrames)
                {
                    continue;
                }
                jfaSamples.push_back(lastJfaMs_);
                double mvSum = 0.0;
                for (int s = 0; s < 7; ++s)
                {
                    stageSamples[s].push_back(lastStageMs_[s]);
                    mvSum += lastStageMs_[s];
                }
                mvTotalSamples.push_back(mvSum);
            }
            vkDeviceWaitIdle(m_device);

            const auto meanOr0 = [](const std::vector<double> &v) -> double {
                const auto m = DatasetBench::Mean(v);
                return m.value_or(0.0);
            };
            jfaTiming.totalMs = meanOr0(jfaSamples);
            mvTiming.totalMs = meanOr0(mvTotalSamples);
            for (int s = 0; s < 7; ++s)
            {
                mvTiming.stageMs[s] = meanOr0(stageSamples[s]);
            }
        };

        for (uint32_t budget : budgets)
        {
            // --- MultiView variants ---
            for (bool hierarchical : {false, true})
            {
                const char *methodName = hierarchical ? "multiview_hierarchical" : "multiview_legacy";
                MethodTiming mvTiming{};
                MethodTiming jfaTimingUnused{};
                sampleGpuFrameTimes(budget, hierarchical, mvTiming, jfaTimingUnused);
                ReadSelectedCameraCount();

                Texture *tex = GetMultiViewDepthSdfTexture();
                std::vector<float> mv =
                    DownloadSdfTexture(m_vulkanDevice, m_queues.graphicsQueue, tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                delete tex;

                QualityRow row;
                row.method = methodName;
                row.cameras = std::min(lastSelectedCameraCount_, budget);
                row.timing = mvTiming;
                if (wantMetrics && !gt.empty() && mv.size() == gt.size())
                {
                    row.metrics = CompareToSignedGt(mv, gt, narrowBand, true);
                }
                else if (wantMetrics)
                {
                    allOk = false;
                }
                AppendQualityCsv(options.outPath, modelName, triangles, options.resolution, budget, row);

                if (options.exportSdf && !mv.empty())
                {
                    const std::string path = exportDir + "/" + modelName + "_n" + std::to_string(options.resolution) + "_k" +
                                            std::to_string(budget) + "_" + methodName + ".raw";
                    WriteVolumeRaw(path, mv);
                }

                std::cout << "  K=" << budget << " " << methodName << " total=" << mvTiming.totalMs << " ms"
                          << " select=" << mvTiming.stageMs[3] << " depth=" << mvTiming.stageMs[5] << " fusion=" << mvTiming.stageMs[6];
                if (row.metrics.hasMetrics)
                {
                    std::cout << " rmse_n=" << row.metrics.rmseNarrow << " sign_iou=" << row.metrics.signIoU;
                }
                std::cout << "\n";
            }

            // --- JFA (reuse last DrawFrame volumes; re-sample timing once with hierarchical off) ---
            {
                MethodTiming mvUnused{};
                MethodTiming jfaTiming{};
                sampleGpuFrameTimes(budget, false, mvUnused, jfaTiming);

                Texture3D *jfaTex = GetMeshToSdfOperator()->GetSdfTexture();
                std::vector<float> jfaVol =
                    DownloadSdfTexture(m_vulkanDevice, m_queues.graphicsQueue, jfaTex, VK_IMAGE_LAYOUT_GENERAL);

                QualityRow row;
                row.method = "jfa";
                row.cameras = 0;
                row.timing = jfaTiming;
                if (wantMetrics && !gt.empty() && jfaVol.size() == gt.size())
                {
                    row.metrics = CompareToSignedGt(jfaVol, gt, narrowBand, false);
                }
                AppendQualityCsv(options.outPath, modelName, triangles, options.resolution, budget, row);

                if (options.exportSdf && !jfaVol.empty())
                {
                    const std::string path =
                        exportDir + "/" + modelName + "_n" + std::to_string(options.resolution) + "_k" + std::to_string(budget) + "_jfa.raw";
                    WriteVolumeRaw(path, jfaVol);
                }
                std::cout << "  K=" << budget << " jfa total=" << jfaTiming.totalMs << " ms";
                if (row.metrics.hasMetrics)
                {
                    std::cout << " rmse_n=" << row.metrics.rmseNarrow;
                }
                std::cout << "\n";
            }

            // --- BVH (cuBQL): independent of K, but keep budget column for join ---
            {
                CubqlBvhSdf cubql;
                MethodTiming timing{};
                std::vector<float> bvhVol;
                if (cubql.Create())
                {
                    cubql.SetModel(&m_glTFModel);
                    float buildMs = 0.f;
                    float fillMs = 0.f;
                    std::vector<double> buildSamples;
                    std::vector<double> fillSamples;
                    buildSamples.reserve(measureFrames);
                    fillSamples.reserve(measureFrames);
                    for (uint32_t i = 0; i < warmupFrames + measureFrames; ++i)
                    {
                        const bool built = cubql.Build(buildMs);
                        const bool filled = built && cubql.FillVolume(config_->Sdf.WorldSize, options.resolution, bvhVol, fillMs);
                        if (!filled)
                        {
                            allOk = false;
                            bvhVol.clear();
                            break;
                        }
                        if (i >= warmupFrames)
                        {
                            buildSamples.push_back(buildMs);
                            fillSamples.push_back(fillMs);
                        }
                    }
                    const auto buildMean = DatasetBench::Mean(buildSamples);
                    const auto fillMean = DatasetBench::Mean(fillSamples);
                    timing.bvhBuildMs = buildMean.value_or(0.0);
                    timing.bvhFillMs = fillMean.value_or(0.0);
                    timing.totalMs = timing.bvhBuildMs + timing.bvhFillMs;
                }
                else
                {
                    allOk = false;
                }

                QualityRow row;
                row.method = "bvh";
                row.cameras = 0;
                row.timing = timing;
                if (wantMetrics && !gt.empty() && bvhVol.size() == gt.size())
                {
                    row.metrics = CompareToSignedGt(bvhVol, gt, narrowBand, false);
                }
                AppendQualityCsv(options.outPath, modelName, triangles, options.resolution, budget, row);

                if (options.exportSdf && !bvhVol.empty())
                {
                    const std::string path =
                        exportDir + "/" + modelName + "_n" + std::to_string(options.resolution) + "_k" + std::to_string(budget) + "_bvh.raw";
                    WriteVolumeRaw(path, bvhVol);
                }
                std::cout << "  K=" << budget << " bvh total=" << timing.totalMs << " ms build=" << timing.bvhBuildMs
                          << " fill=" << timing.bvhFillMs;
                if (row.metrics.hasMetrics)
                {
                    std::cout << " rmse_n=" << row.metrics.rmseNarrow;
                }
                std::cout << "\n";
            }
        }
    }

    std::cout << "quality csv: " << options.outPath << "\n" << std::flush;
    vkDeviceWaitIdle(m_device);
    std::_Exit(allOk ? EXIT_SUCCESS : EXIT_FAILURE);
}
