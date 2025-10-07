module;
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "stb/stb_image.h"
#include "stb_image_write.h"
#include "tiny_gltf.h"
#include <algorithm>
#include <fstream>
#include <memory>

module RendererMod;
import BruteForceSdf;

const float PI = 3.1415929;

Renderer::Renderer(Config* config) : config_(config)
{
    m_neededFeatures.validation = config->enableValidation;
    m_camera.type = Camera::CameraType::firstperson;
    m_camera.flipY = true;
    m_camera.setPosition(config->camera.pos);
    m_camera.setRotation(glm::vec3(0.0f));
    m_camera.setPerspective(config_->camera.fov, (float)m_width / (float)m_height, config->camera.znear, config->camera.zfar);
    m_camera.setMovementSpeed(config->camera.movementSpeed);
}

void Renderer::Run()
{
    InitWindow();
    InitVulkan();
    MainLoop();
    Cleanup();
}
// 得到窗口大小改变的信息
void Renderer::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    // 得到从window传递的this指针
    auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    app->m_framebufferResized = true;
}
/// @brief 初始化窗口
void Renderer::InitWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // 设置 GLFW 创建的窗口是否可以调整大小。
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    // 使用glfw创建窗口
    m_window = glfwCreateWindow(m_width, m_height, "Renderer", nullptr, nullptr);

    // 传递对象this指针给回调函数。这样我们就可以在回调函数中访问类的成员变量, 也就是传递窗口变化信息
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, KeyCallback);                           // 键盘事件
    glfwSetCursorPosCallback(m_window, MouseCallback);                   // 鼠标移动
    glfwSetScrollCallback(m_window, ScrollCallback);                     // 鼠标滚轮
    glfwSetMouseButtonCallback(m_window, MouseButtonCallback);           // 鼠标按键
    glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback); // 窗口大小改变、最小化
}

void Renderer::PreCreateSubmitInfo()
{

    m_submitInfo = Init::submitInfo();
    m_submitInfo.pWaitDstStageMask = &m_submitPipelineStages;
    m_submitInfo.waitSemaphoreCount = 1;
    m_submitInfo.pWaitSemaphores = &m_semaphores.presentComplete;
    m_submitInfo.signalSemaphoreCount = 1;
    m_submitInfo.pSignalSemaphores = &m_semaphores.renderComplete;
}
void Renderer::SetEnabledFeatures()
{
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_deviceFeatures);
    if (m_deviceFeatures.geometryShader)
    {
        m_enabledFeatures.geometryShader = VK_TRUE;
    }
    if (m_deviceFeatures.samplerAnisotropy)
    {
        m_enabledFeatures.samplerAnisotropy = VK_TRUE;
    }
    if (m_deviceFeatures.depthClamp)
    {
        m_enabledFeatures.depthClamp = VK_TRUE;
    }
    if (m_deviceFeatures.fragmentStoresAndAtomics)
    {
        m_enabledFeatures.fragmentStoresAndAtomics = VK_TRUE;
    }
    if (m_deviceFeatures.sampleRateShading)
    {
        m_enabledFeatures.sampleRateShading = VK_TRUE;
    }
    if (m_deviceFeatures.multiDrawIndirect)
    {
        m_enabledFeatures.multiDrawIndirect = VK_TRUE; // 启用multiDrawIndirect特性用于Stage4
    }
    if (m_deviceFeatures.imageCubeArray)
    {
        m_enabledFeatures.imageCubeArray = VK_TRUE; // 启用imageCubeArray特性用于立方体贴图数组
    }
    // if (m_deviceFeatures.shaderSampledImageArrayDynamicIndexing)
    //{
    //	m_enabledFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    // }
    m_enabledFeatures.independentBlend = VK_TRUE;
}
void Renderer::EncapsulationDevice()
{
    VkResult result;
    m_vulkanDevice = new OldVulkanDevice(m_physicalDevice);

    // RHI Device Testing - DISABLED to avoid conflicts with old renderer
    // rhiDevice_ = Rhi::CreateVulkanDevice(m_window);
    // if (rhiDevice_)
    // {
    // }
    // else
    // {
    //     std::cerr << "[ERROR] Failed to create RHI Device" << std::endl;
    // }

    SetEnabledFeatures();

    // 计算着色器导数扩展已通过修改HLSL代码解决，无需启用扩展

    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.runtimeDescriptorArray = VK_TRUE;    // 启用 runtimeDescriptorArray
    vulkan12Features.descriptorIndexing = VK_TRUE;        // 启用 descriptorIndexing 用于VK_EXT_descriptor_indexing
    vulkan12Features.shaderOutputViewportIndex = VK_TRUE; // 启用 shaderOutputViewportIndex
    vulkan12Features.shaderOutputLayer = VK_TRUE;         // 启用 shaderOutputLayer 用于VK_EXT_shader_viewport_index_layer

    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.dynamicRendering = VK_TRUE; // 启用动态渲染

    // 启用 Multiview 特性 (用于多视角深度渲染)
    VkPhysicalDeviceMultiviewFeatures multiviewFeatures{};
    multiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
    multiviewFeatures.multiview = VK_TRUE;                    // 启用基本multiview功能
    multiviewFeatures.multiviewGeometryShader = VK_FALSE;     // 不需要几何着色器支持
    multiviewFeatures.multiviewTessellationShader = VK_FALSE; // 不需要曲面细分着色器支持

    // 将 Vulkan 1.2, 1.3 和 Multiview 特性结构体链接到 pNextChain
    if (m_deviceCreatepNextChain)
    {
        // 如果已经有链存在，链接 Vulkan 1.2 特性到链的末尾
        VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
        physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        physicalDeviceFeatures2.pNext = m_deviceCreatepNextChain; // 将原有链连接到新的链
        physicalDeviceFeatures2.features = m_enabledFeatures;     // 启用基础特性
        m_deviceCreatepNextChain = &physicalDeviceFeatures2;      // 更新链
    }
    else
    {
        // 如果链为空，直接链接Vulkan特性
        // 构建特性链: multiview -> vulkan13 -> vulkan12
        multiviewFeatures.pNext = &vulkan13Features;   // multiview 连接 Vulkan 1.3
        vulkan13Features.pNext = &vulkan12Features;    // Vulkan 1.3 连接 Vulkan 1.2
        m_deviceCreatepNextChain = &multiviewFeatures; // 从 multiview 开始
    }
    result = m_vulkanDevice->CreateLogicalDevice(m_enabledFeatures, m_deviceExtensions, m_deviceCreatepNextChain);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Could not create Vulkan device: \n" + Tool::ErrorString(result));
    }
    m_device = m_vulkanDevice->logicalDevice;

    // Get a graphics queue from the device
    vkGetDeviceQueue(m_device, m_vulkanDevice->queueFamilyIndices.graphics, 0, &m_queues.graphicsQueue);
    vkGetDeviceQueue(m_device, m_vulkanDevice->queueFamilyIndices.graphics, 0, &m_queues.presentQueue);
}
/// @brief 初始化 Vulkan，包括相关设置
void Renderer::InitVulkan()
{
    CreateInstance();
    SetupDebugMessenger(); // 先创建实例，相当于先指定和调试的接口，再之后再实际链接
    CreateSurface();
    PickPhysicalDevice();
    // CreateLogicalDevice();

    EncapsulationDevice();
    LoadDebugUtilsFunctions();
    m_index.compute = m_vulkanDevice->queueFamilyIndices.compute;
    m_index.graphics = m_vulkanDevice->queueFamilyIndices.graphics;

    m_swapChain.SetContext(m_instance, m_physicalDevice, m_device);

    CreateSyncObjects();
    PreCreateSubmitInfo();
    m_swapChain.InitSurface(m_surface);
    m_swapChain.Create(m_width, m_height, false, false);

    // prepare
    // CreateSwapChain();
    CreateCommandPool();
    CreateCommandBuffers();
    CreateDepthResources();
    CreateRenderPass();
    CreateFramebuffers();
    CreateDescriptorPool();

    // Main
    CreateDefaultTextures();
    LoadAssets();
    GenerateBRDFLUT();
    GenerateIrradianceCube();
    GeneratePrefilteredCube();

    InitLights();
    CreateBuffers(); // 2

    SetupPasses(); // 3

    SetupTileBasedLightingPass();
    SetupDeferedPass();
    SetupLightingPass();
    SetupSkyBoxPass();
    // SetupShadow();
    SetupFinalPass();
    SetupBloomPass();
    SetupToneMappingPass();
    InitUI();

    PrepareUniformBuffers();

    SetupDescriptors();
    AllocateDescriptorSets(); // 4

    PreparePipelines(); // 5

    BuildCommandBuffers();
    BuildDeferredCommandBuffer();


    {
        
        if (!m_gpuMipmapOctree)
        {
            m_gpuMipmapOctree = std::make_unique<GPUMipmapOctree>(m_vulkanDevice, config_->Sdf.SdfMode,config_->Sdf.Resolution);
        }
        // SetupGpuOctreePass();
        SetupVoxelizationPass();
        BuildVoxelizationCommandBuffer();

        // 初始化统一GPU管线资源和预录制命令
        InitializeUnifiedGPUPipelineResources();
        RecordUnifiedGPUPipelineCommands();
        // OffscreenWork();

        // MeshToSdf
        InitializeMeshToSdfOperator();
    }


}
void Renderer::TestBruteSdfAndSave()
{
    BruteForceSdf sdfGenerator;
    BruteForceSdf::SdfParameters sdfParams{};

    // 更新参数
    sdfParams.signedDistance = false;
    sdfParams.voxelResolution = glm::vec3(config_->Sdf.Resolution, config_->Sdf.Resolution, config_->Sdf.Resolution);
    sdfParams.origin = glm::vec3(-config_->Sdf.WorldSize / 2.0f, -config_->Sdf.WorldSize / 2.0f, -config_->Sdf.WorldSize / 2.0f);
    sdfParams.cellSize = config_->Sdf.WorldSize / static_cast<float>(config_->Sdf.Resolution);
    sdfGenerator.Initialize(sdfParams);
    sdfGenerator.SetModel(&m_glTFModel);
    const int totalVoxels = sdfParams.voxelResolution.x * sdfParams.voxelResolution.y * sdfParams.voxelResolution.z;
    std::vector<float> sdfData(totalVoxels, std::numeric_limits<float>::max());
    sdfGenerator.GenerateGroundTruth(sdfData);

    // 保存 SDF 结果为图像文件
    sdfGenerator.SaveToFile(sdfData, Tool::GetAssetsPath() + "Sdf/" + "BruteSdf.raw");
}
void Renderer::InitializeMeshToSdfOperator()
{

    meshToSdfOperator_ = new MeshToSdf{};
    meshToSdfOperator_->Initialize(m_vulkanDevice, m_queues.graphicsQueue, m_descriptorPool, &m_glTFModel);
    MeshToSdf::SdfParam sdfParams{};
    
    sdfParams.distanceMode = static_cast<MeshToSdf::DistanceMode>(config_->Sdf.MeshToSdfDistanceMode);
    sdfParams.FloodFillQuality = static_cast<MeshToSdf::FloodFillQuality>(config_->Sdf.MeshToSdfQuality);
    sdfParams.floodMode = static_cast<MeshToSdf::FloodMode>(config_->Sdf.MeshToSdfMode);
    sdfParams.floodIterations = static_cast<int>(config_->Sdf.MeshToSdfIteration);
    sdfParams.offset = 0.0f;
    sdfParams.size = config_->Sdf.WorldSize;
    sdfParams.voxelResolution = static_cast<int>(config_->Sdf.Resolution);
    meshToSdfOperator_->SetSdfParams(sdfParams);

    meshToSdfCommandBuffer_ = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
}
void Renderer::OffscreenWork()
{
    SetupPassDepthCubeMap();
    RenderToCube(m_glTFModel, glm::vec3(0, 0, 0), "inside_cat");
}
void Renderer::InitUI()
{
    m_UI.device = m_vulkanDevice;
    m_UI.queue = m_queues.graphicsQueue;
    m_UI.shaders = {
        LoadShader(Tool::GetShadersPath() + "Base/uioverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
        LoadShader(Tool::GetShadersPath() + "Base/uioverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
    };
    m_UI.PrepareResources();
    m_UI.PreparePipeline(nullptr, m_finalPass, m_swapChain.colorFormat, m_depthFormat);
}


void ::Renderer::CreateCommandBuffers()
{
    // Create one command buffer for each swap chain image
    m_drawCmdBuffers.resize(m_swapChain.images.size());
    VkCommandBufferAllocateInfo cmdBufAllocateInfo =
        Init::commandBufferAllocateInfo(m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(m_drawCmdBuffers.size()));
    Tool::CheckResult(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, m_drawCmdBuffers.data()));
}

// composition
void Renderer::BuildCommandBuffers()
{
    for (int32_t i = 0; i < m_drawCmdBuffers.size(); ++i)
    {
        vkResetCommandBuffer(m_drawCmdBuffers[i], 0);
    }

    VkCommandBufferBeginInfo cmdBufInfo = Init::commandBufferBeginInfo();

    VkClearValue clearValues[2];
    clearValues[0].color = {{0.0f, 0.0f, 0.2f, 1.0f}};
    clearValues[1].color = {{0.0f, 0.0f, 0.2f, 1.0f}};
    // clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = Init::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = m_finalPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = m_width;
    renderPassBeginInfo.renderArea.extent.height = m_height;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    VkViewport viewport = Init::viewport((float)m_width, (float)m_height, 0.0f, 1.0f);
    VkRect2D scissor = Init::rect2D(m_width, m_height, 0, 0);

    for (int32_t i = 0; i < m_drawCmdBuffers.size(); ++i)
    {
        // Set target frame buffer
        renderPassBeginInfo.framebuffer = m_finalFramebuffers[i];

        Tool::CheckResult(vkBeginCommandBuffer(m_drawCmdBuffers[i], &cmdBufInfo));

        BeginDebugLabel(m_drawCmdBuffers[i], "Final Pass FXAA", 1.0f);
        vkCmdBeginRenderPass(m_drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);
        vkCmdSetScissor(m_drawCmdBuffers[i], 0, 1, &scissor);

        vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.FXAA);
        vkCmdBindDescriptorSets(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.FXAA, 0, 1, &m_descriptorSets.FXAA, 0, NULL);

        vkCmdDraw(m_drawCmdBuffers[i], 3, 1, 0, 0);
        EndDebugLabel(m_drawCmdBuffers[i]);
        BeginDebugLabel(m_drawCmdBuffers[i], "UI", 0.50f);
        DrawUI(m_drawCmdBuffers[i]);
        EndDebugLabel(m_drawCmdBuffers[i]);
        vkCmdEndRenderPass(m_drawCmdBuffers[i]);

        Tool::CheckResult(vkEndCommandBuffer(m_drawCmdBuffers[i]));
    }
}

void Renderer::DrawUI(const VkCommandBuffer commandBuffer)
{
    const VkViewport viewport = Init::viewport((float)m_width, (float)m_height, 0.0f, 1.0f);
    const VkRect2D scissor = Init::rect2D(m_width, m_height, 0, 0);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    m_UI.Draw(commandBuffer);
}

VkPipelineShaderStageCreateInfo Renderer::LoadShader(std::string fileName, VkShaderStageFlagBits stage)
{
    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = stage;
    shaderStage.module = Tool::LoadShader(fileName.c_str(), m_device);
    shaderStage.pName = "main";
    assert(shaderStage.module != VK_NULL_HANDLE);
    m_shaderModules.push_back(shaderStage.module);
    return shaderStage;
}

void Renderer::CreateDepthResources()
{
    VkImageCreateInfo imageCI{};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    m_depthFormat = FindDepthFormat();
    imageCI.format = m_depthFormat;
    imageCI.extent = {m_width, m_height, 1};
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    vkCreateImage(m_device, &imageCI, nullptr, &m_depthStencil.image);
    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(m_device, m_depthStencil.image, &memReqs);

    VkMemoryAllocateInfo memAllloc{};
    memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllloc.allocationSize = memReqs.size;
    FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Tool::CheckResult(vkAllocateMemory(m_device, &memAllloc, nullptr, &m_depthStencil.memory));

    if (vkBindImageMemory(m_device, m_depthStencil.image, m_depthStencil.memory, 0) != VK_SUCCESS)
    {
        throw std::runtime_error("fail to allocate depth memory");
    };

    VkImageViewCreateInfo imageViewCI{};
    imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.image = m_depthStencil.image;
    imageViewCI.format = m_depthFormat;
    imageViewCI.subresourceRange.baseMipLevel = 0;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount = 1;
    imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    // Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
    if (m_depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
    {
        imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    if (vkCreateImageView(m_device, &imageViewCI, nullptr, &m_depthStencil.view) != VK_SUCCESS)
    {
        throw std::runtime_error("fail to create depth image view");
    };
}
void Renderer::CreateFramebuffers()
{
    m_frameBuffers.resize(m_swapChain.images.size());
    for (uint32_t i = 0; i < m_frameBuffers.size(); ++i)
    {
        const VkImageView attachments[2]{m_swapChain.imageViews[i], m_depthStencil.view};
        VkFramebufferCreateInfo frameBufferCI{};
        frameBufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferCI.renderPass = m_renderPass;
        frameBufferCI.pAttachments = attachments;
        frameBufferCI.layers = 1;
        frameBufferCI.height = m_height;
        frameBufferCI.width = m_width;
        frameBufferCI.attachmentCount = 2;
        if (vkCreateFramebuffer(m_device, &frameBufferCI, nullptr, &m_frameBuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("fail to create framebuffer");
        };
    }
}
void Renderer::CreateCommandPool()
{
    VkCommandPoolCreateInfo cmdPoolCI{};
    cmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCI.queueFamilyIndex = m_indices.graphicsFamily.value();
    cmdPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(m_device, &cmdPoolCI, nullptr, &m_commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create command pool!");
    }
}
uint32_t Renderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}
VkResult Renderer::CreateBuffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer* buffer, VkDeviceSize size, void* data)
{
    buffer->device = m_device;

    VkBufferCreateInfo bufferCI{};
    bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCI.size = size;
    bufferCI.usage = usage;
    // bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferCI, nullptr, &buffer->buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create buffer!");
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer->buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    // If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
    VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
        allocInfo.pNext = &allocFlagsInfo;
    }

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &buffer->memory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    buffer->alignment = memRequirements.alignment;
    buffer->size = size;
    buffer->usageFlags = usage;
    buffer->memoryPropertyFlags = properties;

    // If a pointer to the buffer data has been passed, map the buffer and copy over the data
    if (data != nullptr)
    {
        buffer->Map();
        memcpy(buffer->mapped, data, size);
        if ((properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            buffer->Flush();

        buffer->Unmap();
    }
    buffer->SetupDescriptor();

    // vkBindBufferMemory(m_device, buffer->buffer, buffer->memory, 0);
    return buffer->Bind();
}
// void Renderer::CreateUniformBuffer()
//{
//	CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_uboBuffer,
//sizeof(m_uboMatrices)); 	m_uboBuffer.Map();
//	//Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_defered.uniformBuffers.offscreen, sizeof(UniformDataOffscreen)));
//	//Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_defered.uniformBuffers.composition, sizeof(UniformDataComposition)));
//
//	//// Map persistent
//	//Tool::CheckResult(m_defered.uniformBuffers.offscreen.Map());
//	//Tool::CheckResult(m_defered.uniformBuffers.composition.Map());
//
//	//// Update
//	//UpdateUniformBufferOffscreen();
//	//UpdateUniformBufferComposition();
// }
//  Update matrices used for the offscreen rendering of the scene
void Renderer::UpdateUniformBufferPost()
{
    m_postParams.gamma = Settings.PostSetting.Gamma;
    m_postParams.exposure = Settings.PostSetting.Exposure;
    memcpy(m_uniformBuffers.postParam.mapped, &m_postParams, sizeof(m_postParams));
}
void Renderer::UpdateUniformBufferFXAA()
{
    m_FXAAParams.rcpFrame.x = 1.0f / m_width;
    m_FXAAParams.rcpFrame.y = 1.0f / m_height;
    memcpy(m_uniformBuffers.FXAA.mapped, &m_FXAAParams, sizeof(m_FXAAParams));
}
void Renderer::UpdateUniformBufferOffscreen()
{
    m_uniformDataOffscreen.projection = m_camera.matrices.perspective;
    m_uniformDataOffscreen.view = m_camera.matrices.view;
    m_uniformDataOffscreen.model = glm::mat4(1.0f);
    memcpy(m_uniformBuffers.defered.mapped, &m_uniformDataOffscreen, sizeof(UniformDataOffscreen));
}

void Renderer::UpdateCbufferSkyBox()
{
    ;
}
void Renderer::AllocateDescriptorSetSkyBox()
{
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        // Binding 0: Camera
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        // Binding 1: lighting texture
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
        // Binding 2: skybox texture
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_descriptorSetLayouts.skyBox));

    // skyBox
    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_descriptorSetLayouts.skyBox, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.skyBox));

    VkDescriptorImageInfo texDescriptorLighting = Init::descriptorImageInfo(
        m_framebuffers.lighting->sampler, m_framebuffers.lighting->attachments[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        Init::writeDescriptorSet(m_descriptorSets.skyBox, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_sharedBuffers.ConstBufferCamera.descriptor),
        Init::writeDescriptorSet(m_descriptorSets.skyBox, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorLighting),
        Init::writeDescriptorSet(m_descriptorSets.skyBox, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &scene.textures.environmentCube.descriptor),
    };
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}
void Renderer::CreateBuffersLighting()
{
    m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 &m_lightingPass.Buffers.ConstBuffer, sizeof(m_lightingPass.Buffers.ConstBuffer));
    Tool::CheckResult(m_lightingPass.Buffers.ConstBuffer.Map());
    // for (uint32_t i = 0; i < LIGHT_COUNT; i++)
    //{
    //	m_lightingPass.CBufferData.lights[i].viewMatrix = m_uniformDataShadows.mvp[i];
    // }
    UpdateCBufferLighting();
    // GenerateNoiseTextureSSAO();
}
void Renderer::UpdateCBufferSpotLight()
{
    // for (uint32_t i = 0; i < LIGHT_COUNT; i++)
    //{
    //	// mvp from light's pov (for shadows)
    //	glm::mat4 shadowProj = glm::perspective(glm::radians(m_shadowSettings.lightFOV), 1.0f, m_shadowSettings.zNear, m_shadowSettings.zFar);
    //	glm::mat4 shadowView = glm::lookAt(glm::vec3(m_lightingPass.CBufferData.lights[i].position),
    //glm::vec3(m_lightingPass.CBufferData.lights[i].target), glm::vec3(0.0f, 1.0f, 0.0f));
    //	//glm::mat4 shadowModel = glm::mat4(1.0f);
    //	m_uniformDataShadows.mvp[i] = shadowProj * shadowView;// *shadowModel;
    //	//m_lightingPass.CBufferData.lights[i].viewMatrix = m_uniformDataShadows.mvp[i];
    // }
    // memcpy(m_uniformBuffers.shadowGeometryShader.mapped, &m_uniformDataShadows, sizeof(UniformDataShadows));
}
void Renderer::UpdateCBufferLighting()
{
    // for (uint32_t i = 0; i < LIGHT_COUNT; i++)
    //{
    //	m_lightingPass.CBufferData.lights[i].viewMatrix = m_uniformDataShadows.mvp[i];
    // }
    if (m_shadowDirLightCount != 0)
    {
        m_lightingPass.CBufferData.SplitDepth = glm::vec4{m_CSMPass.Cascades[0][0].splitDepth, m_CSMPass.Cascades[0][1].splitDepth,
                                                          m_CSMPass.Cascades[0][2].splitDepth, m_CSMPass.Cascades[0][3].splitDepth};
    }
    m_lightingPass.CBufferData.DirLightCount = m_lights.DirLights.size();
    m_lightingPass.CBufferData.ShadowDirCount = m_shadowDirLightCount;
    m_lightingPass.CBufferData.PointLightCount = m_lights.PointLights.size();
    m_lightingPass.CBufferData.ShadowPointCount = m_shadowPointLightCount;
    m_lightingPass.CBufferData.SpotLightCount = m_lights.SpotLights.size();
    m_lightingPass.CBufferData.ShadowSpotCount = m_shadowSpotLightCount;

    m_lightingPass.CBufferData.MetallicFactor = Settings.PBRSetting.MetallicFactor;
    m_lightingPass.CBufferData.RoughnessFactor = Settings.PBRSetting.RoughnessFactor;
    m_lightingPass.CBufferData.debugDisplayTarget = m_debugDisplayTarget;

    memcpy(m_lightingPass.Buffers.ConstBuffer.mapped, &m_lightingPass.CBufferData, sizeof(LightingPass::CBufferDesc));
}

void Renderer::CreateRenderPass()
{
    std::array<VkAttachmentDescription, 2> attachments = {};

    // Color attachment
    attachments[0].format = m_swapChain.colorFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    attachments[1].format = FindDepthFormat();
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference{};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;
    subpassDescription.pDepthStencilAttachment = &depthReference;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;
    subpassDescription.pResolveAttachments = nullptr;

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies{};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[0].dependencyFlags = 0;

    dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].dstSubpass = 0;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask = 0;
    dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[1].dependencyFlags = 0;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}
VkFormat Renderer::FindDepthFormat()
{
    return FindSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL,
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

/// @brief
/// @param candidates
/// @param tiling 图片在设备中的存储格式
/// @param features 该格式需要支持的特性
/// @return
VkFormat Renderer::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}
VkImageView Renderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
    VkImageViewCreateInfo viewCI{};
    viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.image = image;
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format = format;
    viewCI.subresourceRange.aspectMask = aspectFlags;
    viewCI.subresourceRange.baseMipLevel = 0;
    viewCI.subresourceRange.levelCount = mipLevels;
    viewCI.subresourceRange.baseArrayLayer = 0;
    viewCI.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(m_device, &viewCI, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create texture image view!");
    }
    return imageView;
}

// 为获取的交换链图像创建视图，封装
void Renderer::CreateSwapChainImageViews()
{
    m_swapChain.imageViews.resize(m_swapChain.images.size());
    for (size_t i = 0; i < m_swapChain.images.size(); ++i)
    {
        m_swapChain.imageViews[i] = CreateImageView(m_swapChain.images[i], m_swapChain.colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}
VkExtent2D Renderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);
        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        std::cout << "actualExtent: " << actualExtent.width << " " << actualExtent.height << std::endl;
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
};
VkPresentModeKHR Renderer::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

/// @brief 选择合适的交换链格式
/// @param availableFormats
/// @return 合适的交换链格式
VkSurfaceFormatKHR Renderer::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

// void Renderer::CreateSwapChain()
//{
//	SwapChainSupportDetails& swapChainSupport = m_swapChainSupport;
//	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
//	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
//	VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);
//
//	// 设置交换链图像数量
//	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
//	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
//	{
//		imageCount = swapChainSupport.capabilities.maxImageCount;
//	}
//
//	VkSwapchainCreateInfoKHR createInfo{};
//	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
//	createInfo.surface = m_surface;
//	createInfo.minImageCount = imageCount;
//	createInfo.imageFormat = surfaceFormat.format;
//	createInfo.imageColorSpace = surfaceFormat.colorSpace;
//	createInfo.imageExtent = extent;
//	createInfo.imageArrayLayers = 1;
//	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
//
//	QueueFamilyIndices& indices = m_indices;
//	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
//	// 如果是两个队列族，交换链就需要在两个队列族之间共享图像
//	if (indices.graphicsFamily != indices.presentFamily)
//	{
//		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
//		createInfo.queueFamilyIndexCount = 2;
//		createInfo.pQueueFamilyIndices = queueFamilyIndices;
//	}
//	else
//	{
//		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
//		createInfo.queueFamilyIndexCount = 0; // Optional
//		createInfo.pQueueFamilyIndices = nullptr; // Optional
//	}
//	// 预变换，这里是不变换
//	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
//	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
//	createInfo.presentMode = presentMode;
//	createInfo.clipped = VK_TRUE;
//	createInfo.oldSwapchain = VK_NULL_HANDLE;
//
//	if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain.swapChain) != VK_SUCCESS)
//	{
//		throw std::runtime_error("failed to create swap chain!");
//	}
//
//	//查询交换链包含的图像数量，但不会返回图像句柄。这一步的目的是确定需要分配多少内存来存储这些图像。
//	vkGetSwapchainImagesKHR(m_device, m_swapChain.swapChain, &imageCount, nullptr);
//	m_swapChain.images.resize(imageCount);
//
//	//第二次调用 vkGetSwapchainImagesKHR：
//	//传递 mSwapChainImages.data()，将交换链的图像句柄存储到向量中。
//	// mSwapChainImages 现在包含交换链的所有图像句柄，后续可通过这些句柄操作每帧的渲染目标。
//	// Vulkan 管理这些图像的内存。开发者无需手动分配或释放，只需通过 VkSwapchainImagesKHR 获取句柄即可。
//	vkGetSwapchainImagesKHR(m_device, m_swapChain.swapChain, &imageCount, m_swapChain.images.data());
//	m_swapChain.colorFormat = surfaceFormat.format;
//	//m_swapChain.swapChainExtent = extent;
//
//	CreateSwapChainImageViews();
// }
/// @brief 1. 检查是否支持验证层 2. 获取需要的扩展（glfw + valid），好像没有检查扩展是否可用
void Renderer::CreateInstance()
{
    // 如果要使用验证层而系统不支持，则抛出异常
    if (m_neededFeatures.validation && !CheckValidationLayerSupport())
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appIF{};
    appIF.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appIF.pApplicationName = "Renderer";
    appIF.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appIF.pEngineName = "No Engine";
    appIF.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appIF.apiVersion = VK_API_VERSION_1_3;
    appIF.pNext = nullptr;

    VkInstanceCreateInfo createIF{};
    createIF.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createIF.pApplicationInfo = &appIF;

    auto extensions = GetRequiredExtensions();

    // 专指扩展数量，创建时会检查扩展是否可用
    createIF.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createIF.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_neededFeatures.validation)
    {
        // enabledLayerCount vulkan所需的验证层数量
        createIF.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createIF.ppEnabledLayerNames = m_validationLayers.data();

        // 这里是动态创建，先创建实例，再创建调试回调函数，然后再创建将他们链接
        PopulateDebugMessengerCreateInfo(debugCreateInfo);
        createIF.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else
    {
        createIF.enabledLayerCount = 0;
        createIF.pNext = nullptr;
    }

    if (auto result = vkCreateInstance(&createIF, nullptr, &m_instance); result != VK_SUCCESS)
    {
        throw std::runtime_error(std::string("failed to create instance! result:") + std::to_string(result));
    }
}
// 获取所需的 Vulkan 实例扩展。 包括所需的glfw扩展和可选的验证层扩展
std::vector<const char*> Renderer::GetRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions{nullptr};
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // 验证层本身不是扩展，但某些调试功能（如 VK_EXT_debug_utils）需要同时启用特定扩展才能工作！
    //  所以这里加入了 调试扩展
    if (m_neededFeatures.validation)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    //  VK_KHR_surface // 窗口渲染所需的扩展
    //  VK_KHR_win32_surface //特定于windows 窗口渲染所需的扩展，是对VK_KHR_surface的进一步扩展
    //  Vk_EXT_debug_utils
    return extensions;
}

void Renderer::SetRequiredFeatures()
{
}
void Renderer::GetDeviceProperties()
{
    uint32_t layerCount{};
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr); // 获取系统中所有可用的Vulkan实例层的总数

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount,
                                       availableLayers.data()); // 实际填充 availableLayers 向量中的每个元素，每个元素包含一个Vulkan实例层的属性信息。

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions{nullptr};
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
}
/// @brief 检查系统中是否支持验证层
bool Renderer::CheckValidationLayerSupport()
{
    uint32_t layerCount{};
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr); // 获取系统中所有可用的Vulkan实例层的总数

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount,
                                       availableLayers.data()); // 实际填充 availableLayers 向量中的每个元素，每个元素包含一个Vulkan实例层的属性信息。

    // 检查 validationLayers 中的每个层是否都在 availableLayers 中
    // 验证层是否在所有可行层中
    for (const char* layerName : m_validationLayers)
    {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers)
        {
            if (std::strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

/// @brief 设置 createInfo 的信息，包括回调函数
/// @param createInfo
void Renderer::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;
}
VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                       const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}
/// @brief 设置Vulkan实例的调试回调函数
void Renderer::SetupDebugMessenger()
{
    if (!m_neededFeatures.validation)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    PopulateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}
VkResult Renderer::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                                const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    // 这是做了cast？这个函数在扩展中提供，所以需要GetAddr
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

/// @brief  创建surface,surface是对窗口的抽象
void Renderer::CreateSurface()
{
    // 调用glfw的函数创建surface，方便 //需要定义 glfw include vulkan宏
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
}
VkSampleCountFlagBits Renderer::GetMaxUsableSampleCount()
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts =
        physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT)
    {
        return VK_SAMPLE_COUNT_64_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_32_BIT)
    {
        return VK_SAMPLE_COUNT_32_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_16_BIT)
    {
        return VK_SAMPLE_COUNT_16_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_8_BIT)
    {
        return VK_SAMPLE_COUNT_8_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_4_BIT)
    {
        return VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT)
    {
        return VK_SAMPLE_COUNT_2_BIT;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}
// 选取物理设备
void Renderer::PickPhysicalDevice()
{
    uint32_t deviceCount{0};
    // 获取支持vulkan的gpu，经典两段式，先获取数量，再填充数据
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    // 选择合适的 device
    for (const auto& device : devices)
    {
        if (IsDeviceSuitable(device))
        {
            m_physicalDevice = device;

            // 获取支持的采样数
            VkPhysicalDeviceProperties deviceProperties;
            m_msaaSamples = GetMaxUsableSampleCount();

            // 获取支持的各向异性
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            m_maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;
            break;
        }
    }
    // FindQueueFamilies(mPhysicalDevice);
    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

/// @brief 寻找需要的队列族
/// @param device
/// @return
QueueFamilyIndices Renderer::FindQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    VkBool32 presentSupport = false;
    for (int i = 0; const auto& queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) // queueflags。按位与，如果有相同位为1，则true
        {
            indices.graphicsFamily = i;
        }

        // 用于查询物理设备（VkPhysicalDevice）的某个队列族（queue family）是否支持特定的surface（VkSurfaceKHR）的呈现（presentation）。
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport)
        {
            indices.presentFamily = i;
        }
        // break; 多此一举？没有多此一举，complete里面还有
        if (indices.IsComplete())
        {
            break;
        }
        ++i;
    }
    return indices;
}

/// @brief 检查设备是否支持所有所需的扩展、设备扩展和vulkan实例扩展是不同的
/// @param device
/// @return
bool Renderer::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
    // 所有可用的扩展
    uint32_t extensionCount{};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

    // 从需求的扩展中删除可用的扩展，如果结果为空，说明所有需求的扩展都可用
    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

void Renderer::CreateSyncObjects()
{
    // Create synchronization objects
    VkSemaphoreCreateInfo semaphoreCreateInfo = Init::semaphoreCreateInfo();
    // Create a semaphore used to synchronize image presentation
    // Ensures that the image is displayed before we start submitting new commands to the queue
    Tool::CheckResult(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_semaphores.presentComplete));
    // Create a semaphore used to synchronize command submission
    // Ensures that the image is not presented until all commands have been submitted and executed
    Tool::CheckResult(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_semaphores.renderComplete));
}
/// @brief 判断某个device是否合适 // 支持队列族、交换链扩展、交换链合适、且支持各向异性采样
/// @param device
/// @return
bool Renderer::IsDeviceSuitable(VkPhysicalDevice device)
{
    // 支持队列族（渲染和显示）
    QueueFamilyIndices indices = FindQueueFamilies(device);
    if (!indices.IsComplete())
        return false;

    // 支持设备扩展（这里只有swapchain）
    bool extensionsSupported = CheckDeviceExtensionSupport(device);
    if (!extensionsSupported)
        return false;

    // 不止支持交换链扩展，还需要交换链合适
    bool swapChainAdequate = false;
    // 交换链必须支持至少一种format和一种presentmode
    m_swapChainSupport = QuerySwapChainSupport(device);
    swapChainAdequate = !m_swapChainSupport.formats.empty() && !m_swapChainSupport.presentModes.empty();
    if (!swapChainAdequate)
        return false;

    // 且支持各向异性采样
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    // Check Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features supportedVulkan13Features{};
    supportedVulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &supportedVulkan13Features;
    vkGetPhysicalDeviceFeatures2(device, &features2);

    // TODO 改造为支持neededFeature 如果不支持feature 返回不支持的feature名称
    bool support = supportedFeatures.samplerAnisotropy && supportedFeatures.sampleRateShading && supportedFeatures.fragmentStoresAndAtomics &&
                   supportedVulkan13Features.dynamicRendering;

    // Debug output
    // std::cout << "Device features check:" << std::endl;
    // std::cout << "  samplerAnisotropy: " << (supportedFeatures.samplerAnisotropy ? "YES" : "NO") << std::endl;
    // std::cout << "  sampleRateShading: " << (supportedFeatures.sampleRateShading ? "YES" : "NO") << std::endl;
    // std::cout << "  fragmentStoresAndAtomics: " << (supportedFeatures.fragmentStoresAndAtomics ? "YES" : "NO") << std::endl;
    // std::cout << "  dynamicRendering: " << (supportedVulkan13Features.dynamicRendering ? "YES" : "NO") << std::endl;
    // std::cout << "  Overall support: " << (support ? "YES" : "NO") << std::endl;

    if (support)
        m_indices = indices;

    return support;
}

/// @brief 查询并返回交换链支持的细节信息，如format，和present mode 数组
SwapChainSupportDetails Renderer::QuerySwapChainSupport(VkPhysicalDevice device)
{
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
    }
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
    }
    return details;
}

void Renderer::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}
void Renderer::ExportSDFDataForVisualization()
{
    if (config_->Sdf.SdfMode == 0)
    {
        ExportSDFDataForVisualization(GetAnalyticalSdfTexture(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      Tool::GetAssetsPath() + "Sdf/" + "AnalyticalSdf.raw");
    }
    else if (config_->Sdf.SdfMode == 1)
    {
        Texture* tex{GetMultiViewDepthSdfTexture()};
        ExportSDFDataForVisualization(tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Tool::GetAssetsPath() + "Sdf/" + "MultiViewSdf.raw");
        delete tex;
    }

}
void Renderer::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if (action == GLFW_PRESS) // 按键按下
    {
        switch (key)
        {
        // case GLFW_KEY_P:
        //	app->m_camera.paused = !paused;
        //	break;
        // case GLFW_KEY_F1:
        //	uiVisible = !uiVisible;
        //	break;
        case GLFW_KEY_F2:
            app->m_camera.type = (app->m_camera.type == Camera::CameraType::lookat) ? Camera::CameraType::firstperson : Camera::CameraType::lookat;
            break;
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, true);
            break;
        case GLFW_KEY_V:
            app->TestBruteSdfAndSave();
            break;
        case GLFW_KEY_E: { // 按E键导出SDF数据用于Python可视化
            printf("Exporting SDF data for visualization...\n");
            app->ExportSDFDataForVisualization();
            break;
        }
        case GLFW_KEY_M: // 按E键导出SDF数据用于Python可视化
            printf("Exporting SDF data for visualization...\n");
            app->ExportSDFDataForVisualization(app->GetMeshToSdfOperator()->GetSdfTexture(), VK_IMAGE_LAYOUT_GENERAL,
                                               Tool::GetAssetsPath() + "Sdf/" + "MeshToSdf.raw");
            break;
        }

        if (app->m_camera.type == Camera::firstperson)
        {
            switch (key)
            {
            case GLFW_KEY_W:
                app->m_camera.keys.up = true;
                break;
            case GLFW_KEY_S:
                app->m_camera.keys.down = true;
                break;
            case GLFW_KEY_A:
                app->m_camera.keys.left = true;
                break;
            case GLFW_KEY_D:
                app->m_camera.keys.right = true;
                break;
            }
        }
    }
    else if (action == GLFW_RELEASE) // 按键释放
    {
        if (app->m_camera.type == Camera::CameraType::firstperson)
        {
            switch (key)
            {
            case GLFW_KEY_W:
                app->m_camera.keys.up = false;
                break;
            case GLFW_KEY_S:
                app->m_camera.keys.down = false;
                break;
            case GLFW_KEY_A:
                app->m_camera.keys.left = false;
                break;
            case GLFW_KEY_D:
                app->m_camera.keys.right = false;
                break;
            }
        }
    }
}
void Renderer::MouseCallback(GLFWwindow* window, double xpos, double ypos)
{

    auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    auto& mouseState = app->m_mouseState;
    auto& camera = app->m_camera;
    int32_t dx = (int32_t)mouseState.Position.x - (int32_t)xpos;
    int32_t dy = (int32_t)mouseState.Position.y - (int32_t)ypos;

    bool handled = false;

    if (mouseState.Buttons.Left)
    {
        camera.rotate(glm::vec3(dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
        // viewUpdated = true;
    }
    if (mouseState.Buttons.Right)
    {
        camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f));
        // viewUpdated = true;
    }
    if (mouseState.Buttons.Middle)
    {
        camera.translate(glm::vec3(-dx * 0.005f, -dy * 0.005f, 0.0f));
        // viewUpdated = true;
    }
    mouseState.Position = glm::vec2((float)xpos, (float)ypos);
}
void Renderer::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));

    auto& mouseState = app->m_mouseState;
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
            mouseState.Buttons.Left = true;
        else if (action == GLFW_RELEASE)
            mouseState.Buttons.Left = false;
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
            mouseState.Buttons.Right = true;
        else if (action == GLFW_RELEASE)
            mouseState.Buttons.Right = false;
    }
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        if (action == GLFW_PRESS)
            mouseState.Buttons.Middle = true;
        else if (action == GLFW_RELEASE)
            mouseState.Buttons.Middle = false;
    }
}
void Renderer::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    app->m_camera.translate(glm::vec3(0.0f, 0.0f, (float)yoffset * 0.1f));
    /*app->viewUpdated = true;*/
}

void Renderer::DisplayUI(UIOverlay* overlay)
{

    if (overlay->Header("Settings"))
    {
        // TODO
        if (overlay->Header("Shadow"))
        {

            if (overlay->CheckBox("Show Dir Light", &Settings.ShadowSetting.DirLight))
            {
                PreparePipelineLighting();
                BuildDeferredCommandBuffer();
            }
            if (overlay->CheckBox("Show Dir Light Shadow", &Settings.ShadowSetting.DirLightShadow))
            {
                PreparePipelineLighting();
                BuildDeferredCommandBuffer();
            }
            if (overlay->CheckBox("Dir Light Shadow PCF", &Settings.ShadowSetting.DirLightPCF))
            {
                PreparePipelineLighting();
                BuildDeferredCommandBuffer();
            }
            if (overlay->CheckBox("Show Point Light", &Settings.ShadowSetting.PointLight))
            {
                PreparePipelineLighting();
                BuildDeferredCommandBuffer();
            }
            if (overlay->CheckBox("Show Point Light Shadow", &Settings.ShadowSetting.PointLightShadow))
            {
                PreparePipelineLighting();
                BuildDeferredCommandBuffer();
            }
            if (overlay->CheckBox("Show Spot Light", &Settings.ShadowSetting.SpotLight))
            {
                PreparePipelineLighting();
                BuildDeferredCommandBuffer();
            }

            // if (overlay->CheckBox("SpotLight", &Settings.ShadowSetting.SpotLightShadow))
            //{
            //	;
            // }
            // if (overlay->SliderFloat("DepthBiasCons", &Settings.ShadowSetting.DepthBiasCons, 0.0f, 10.0f))
            //{
            //	UpdateCBufferLighting();
            // }
            // if (overlay->SliderFloat("DepthBiasSlope", &Settings.ShadowSetting.DepthBiasCons, 0.0f, 10.0f))
            //{
            //	UpdateCBufferLighting();
            // }
        }
        // Done
        if (overlay->Header("PBR"))
        {
            if (overlay->SliderFloat("metallicFactor", &Settings.PBRSetting.MetallicFactor, 0.0f, 1.0f))
            {
                // 目前每帧更新,需要修改
                UpdateCBufferLighting();
            }
            if (overlay->SliderFloat("roughnessFactor", &Settings.PBRSetting.RoughnessFactor, 0.0f, 1.0f))
            {
                UpdateCBufferLighting();
            }

            if (overlay->ComboBox("SkyBox Type", &Settings.PBRSetting.SkyBoxIndex, {"None", "Default"}))
            {
                PreparePipelineSkyBox();
                PreparePipelineLighting();
                BuildDeferredCommandBuffer();
            }

            if (overlay->CheckBox("Use IBL", &Settings.PBRSetting.UseIBL))
            {
                PreparePipelineLighting();
                BuildDeferredCommandBuffer();
            }
        }
        // if (overlay->Header("Deferred"))
        //{
        //	overlay->ComboBox("Display", &m_debugDisplayTarget, { "Final composition", "Shadows", "Position", "Normals", "Albedo", "Specular","MRAO"
        //});
        // }

        // Pass
        if (overlay->Header("AA"))
        {
            if (overlay->CheckBox("useFXAA", &Settings.AASetting.UseAA))
            {
                if (Settings.AASetting.UseAA)
                    m_FXAAParams.sth.y = 1.0f;
                else
                    m_FXAAParams.sth.y = 0.0f;
                UpdateUniformBufferFXAA();
            }
            if (overlay->SliderFloat("FXAA edgeThreshold", &m_FXAAParams.sth.x, 0.0f, 1.0f))
            {
                UpdateUniformBufferFXAA();
            }
        }
        // Done
        if (overlay->Header("AO"))
        {
            if (overlay->ComboBox("Switch", &Settings.AOSetting.UseAO, {"None", "SSAO", "HBAO", "GTAO", "SDFAO"}))
            {
                PreparePipelineLighting();
                UpdateDescritporSetLighting();
                UpdateDescriptorSetCBF();
                BuildDeferredCommandBuffer();
            }
            if (Settings.AOSetting.UseAO != 0)
            {
                if (overlay->CheckBox("UseBlur", &Settings.AOSetting.UseCBFBlur))
                {
                    PreparePipelineLighting();
                    UpdateDescritporSetLighting();
                    UpdateDescriptorSetCBF();
                    BuildDeferredCommandBuffer();
                }
            }
            if (Settings.AOSetting.UseAO != 0 && Settings.AOSetting.UseCBFBlur)
            {
                if (overlay->SliderFloat("ZStrength", &Settings.AOSetting.ZStrength, 0.1f, 50.0f))
                {
                    UpdateCBufferCBF();
                }
                if (overlay->SliderFloat("BlurRadius", &Settings.AOSetting.BlurKernelRadius, 1.0f, 16.0f))
                {
                    UpdateCBufferCBF();
                }
            }
            if (Settings.AOSetting.UseAO == 1)
            {
                if (overlay->SliderInt("Num Samples", &Settings.AOSetting.NumSamples, 12, 64))
                {
                    UpdateCBufferSSAO();
                }
                if (overlay->SliderFloat("View Radius", &Settings.AOSetting.Radius, 0.01f, 1.0f))
                {
                    UpdateCBufferSSAO();
                }
            }
            if (Settings.AOSetting.UseAO == 2)
            {
                if (overlay->SliderInt("Num Dir", &Settings.AOSetting.NumDir, 4, 10))
                {
                    UpdateCBufferHBAO();
                }
                if (overlay->SliderInt("Num Steps", &Settings.AOSetting.NumSteps, 2, 8))
                {
                    UpdateCBufferHBAO();
                }
                if (overlay->SliderFloat("View Radius", &Settings.AOSetting.Radius, 0.01f, 1.0f))
                {
                    UpdateCBufferHBAO();
                }
                if (overlay->SliderFloat("Max Pix Radius", &Settings.AOSetting.MaxRadiusPixels, 10.0f, 200.0f))
                {
                    UpdateCBufferHBAO();
                }
                if (overlay->SliderFloat("TangentBias", &Settings.AOSetting.HBAOTangentBias, 0.0f, 90.0f))
                {
                    UpdateCBufferHBAO();
                }
                if (overlay->SliderFloat("HBAO Strength", &Settings.AOSetting.Strength, 1.0f, 16.0f))
                {
                    UpdateCBufferHBAO();
                }
            }
            if (Settings.AOSetting.UseAO == 3)
            {
                if (overlay->SliderInt("Num Dir", &Settings.AOSetting.NumDir, 4, 10))
                {
                    UpdateCBufferGTAO();
                }
                if (overlay->SliderInt("Num Steps", &Settings.AOSetting.NumSteps, 2, 8))
                {
                    UpdateCBufferGTAO();
                }
                if (overlay->SliderFloat("View Radius", &Settings.AOSetting.Radius, 0.01f, 1.0f))
                {
                    UpdateCBufferGTAO();
                }
                if (overlay->SliderFloat("Max Pix Radius", &Settings.AOSetting.MaxRadiusPixels, 10.0f, 200.0f))
                {
                    UpdateCBufferGTAO();
                }

                if (overlay->SliderFloat("GTAO Strength", &Settings.AOSetting.Strength, 0.50f, 2.0f))
                {
                    UpdateCBufferGTAO();
                }
            }
        }
        // Done
        if (overlay->Header("Post"))
        {
            if (overlay->SliderFloat("exposure", &Settings.PostSetting.Exposure, 0.0f, 10.0f))
            {
                UpdateUniformBufferPost();
            }
            if (overlay->SliderFloat("gamma", &Settings.PostSetting.Gamma, 0.0f, 10.0f))
            {
                UpdateUniformBufferPost();
            }

            if (overlay->CheckBox("Bloom", &m_postSettings.bloom))
            {
                if (!m_postSettings.bloom)
                {
                    VkDescriptorImageInfo texDescriptorBloomEnd = Init::descriptorImageInfo(
                        m_framebuffers.bloom1->sampler, m_framebuffers.bloom1->defaultMaterials[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    VkWriteDescriptorSet write =
                        Init::writeDescriptorSet(m_descriptorSets.toneMapping, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorBloomEnd);

                    vkUpdateDescriptorSets(m_device, 1, &write, 0, NULL);
                }
                else
                {
                    VkDescriptorImageInfo texDescriptorBloomEnd = Init::descriptorImageInfo(
                        m_framebuffers.bloom1->sampler, m_framebuffers.bloom1->attachments[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    VkWriteDescriptorSet write =
                        Init::writeDescriptorSet(m_descriptorSets.toneMapping, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorBloomEnd);

                    vkUpdateDescriptorSets(m_device, 1, &write, 0, NULL);
                }
                BuildDeferredCommandBuffer();
            }
            if (overlay->InputFloat("Scale", &Settings.PostSetting.BloomScale, 0.1f))
            {
                UpdateUniformBuffersBlur();
            }
            if (overlay->SliderFloat("Bloom Strength", &Settings.PostSetting.BloomStength, 0.0f, 10.0f))
            {
                UpdateUniformBuffersBlur();
            }
        }
        if (overlay->Header("Others"))
        {
        }
    }
}

void Renderer::UpdateOverlay()
{
    m_UI.updateTimer -= m_frameTimer;
    if (m_UI.updateTimer >= 0.0f)
    {
        return;
    }
    // Update at max. rate of 30 fps
    m_UI.updateTimer = 1.0f / 30.0f;

    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2((float)m_width, (float)m_height);
    io.DeltaTime = m_frameTimer;

    io.MousePos = ImVec2(m_mouseState.Position.x, m_mouseState.Position.y);
    io.MouseDown[0] = m_mouseState.Buttons.Left && m_UI.visible;
    io.MouseDown[1] = m_mouseState.Buttons.Right && m_UI.visible;
    io.MouseDown[2] = m_mouseState.Buttons.Middle && m_UI.visible;

    ImGui::NewFrame();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::SetNextWindowPos(ImVec2(10 * m_UI.scale, 10 * m_UI.scale));
    ImGui::SetNextWindowSize(ImVec2(0, 0), 4);
    ImGui::Begin("MyToyRenderer", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::TextUnformatted(m_title.c_str());
    ImGui::TextUnformatted(m_vulkanDevice->properties.deviceName);
    ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / m_lastFPS), m_lastFPS);

    ImGui::PushItemWidth(110.0f * m_UI.scale);
    DisplayUI(&m_UI);
    ImGui::PopItemWidth();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();

    if (m_UI.Update() || m_UI.updated)
    {
        BuildCommandBuffers();
        m_UI.updated = false;
    }
}
std::string Renderer::GetWindowTitle() const
{
    std::string windowTitle{m_title + " - " + m_vulkanDevice->properties.deviceName};

    windowTitle += " - " + std::to_string(m_frameCounter) + " fps";

    return windowTitle;
}
void Renderer::MainLoop()
{
    // Test voxelization once at startup
    TestVoxelization();

    while (!glfwWindowShouldClose(m_window))
    {
        auto tStart = std::chrono::high_resolution_clock::now();

        DrawFrame();

        m_frameCounter++;
        auto tEnd = std::chrono::high_resolution_clock::now();
        auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        m_frameTimer = (float)tDiff / 1000.0f;
        m_camera.update(m_frameTimer);
        m_timer += m_timerSpeed * m_frameTimer;
        if (m_timer > 1.0)
        {
            m_timer -= 1.0f;
        }
        float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - m_lastTimestamp).count());
        if (fpsTimer > 1000.0f)
        {
            m_lastFPS = static_cast<uint32_t>((float)m_frameCounter * (1000.0f / fpsTimer));

            std::string windowTitle = GetWindowTitle();
            glfwSetWindowTitle(m_window, windowTitle.c_str());

            m_frameCounter = 0;
            m_lastTimestamp = tEnd;
        }
        m_tPrevEnd = tEnd;
        UpdateOverlay();
        glfwPollEvents();
    }
}

void Renderer::ResizeWindow()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    while (width == 0 || height == 0)
    {
        // 等待窗口事件，如窗口大小改变或其他事件
        glfwWaitEvents();
        glfwGetFramebufferSize(m_window, &width, &height);
    }
    vkDeviceWaitIdle(m_device);
    m_width = static_cast<uint32_t>(width);
    m_height = static_cast<uint32_t>(height);
    m_swapChain.Create(m_width, m_height, false, false);

    // Recreate the frame buffers
    vkDestroyImageView(m_device, m_depthStencil.view, nullptr);
    vkDestroyImage(m_device, m_depthStencil.image, nullptr);
    vkFreeMemory(m_device, m_depthStencil.memory, nullptr);
    CreateDepthResources();

    for (auto& frameBuffer : m_finalFramebuffers)
    {
        vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
    }
    SetupFinalPass();
    // CreateFramebuffers();
    if ((m_width > 0.0f) && (m_height > 0.0f))
    {
        m_UI.Resize(m_width, m_height);
    }

    // Command buffers need to be recreated as they may store
    // references to the recreated frame buffer
    vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_drawCmdBuffers.size()), m_drawCmdBuffers.data());
    CreateCommandBuffers();
    BuildCommandBuffers();

    // SRS - Recreate fences in case number of swapchain images has changed on resize
    // for (auto& fence : m_waitFences)
    //{
    //	vkDestroyFence(m_device, fence, nullptr);
    //}
    // CreateSyncObjects();

    vkDeviceWaitIdle(m_device);

    if ((m_width > 0.0f) && (m_height > 0.0f))
    {
        m_camera.updateAspectRatio((float)m_width / (float)m_height);
    }
}
void Renderer::PrepareFrame()
{

    VkResult result =
        vkAcquireNextImageKHR(m_device, m_swapChain.swapChain, UINT64_MAX, m_semaphores.presentComplete, (VkFence) nullptr, &currentBuffer);

    if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            ResizeWindow();
        }
        return;
    }
    else
    {
        Tool::CheckResult(result);
    }
}
// void Renderer::UpdateUniformBuffers()
//{
//	m_uboMatrices.proj = m_camera.matrices.perspective;
//	m_uboMatrices.view = m_camera.matrices.view;
//	//camera.matrices.view;
//	m_uboMatrices.lightPos = glm::vec3(0.0f, 100.0f, 0.0f);
//	//m_camera.position.x = -m_camera.position.x;
//	m_uboMatrices.camPos = m_camera.GetCameraPos();
//	memcpy(m_uboBuffer.mapped, &m_uboMatrices, sizeof(m_uboMatrices));
// }

void Renderer::Draw()
{

    {

        vkResetCommandBuffer(meshToSdfCommandBuffer_, 0);
        VkCommandBufferBeginInfo cmdBufInfo = Init::commandBufferBeginInfo();
        Tool::CheckResult(vkBeginCommandBuffer(meshToSdfCommandBuffer_, &cmdBufInfo));
        BeginDebugLabel(meshToSdfCommandBuffer_, "MeshToSDF", 1.0f, 0.0f, 0.0f, 1.0f);
        meshToSdfOperator_->GenerateSdf(meshToSdfCommandBuffer_);
        EndDebugLabel(meshToSdfCommandBuffer_);
        m_vulkanDevice->FlushCommandBuffer(meshToSdfCommandBuffer_, m_queues.graphicsQueue, false, false);
    }
    // Wait for rendering finished
    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    // Submit compute commands
    VkSubmitInfo computeSubmitInfo = Init::submitInfo();
    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers = &m_compute.commandBuffer;
    computeSubmitInfo.waitSemaphoreCount = 1;
    computeSubmitInfo.pWaitSemaphores = &m_semaphores.presentComplete;
    computeSubmitInfo.pWaitDstStageMask = &waitStageMask;
    computeSubmitInfo.signalSemaphoreCount = 1;
    computeSubmitInfo.pSignalSemaphores = &m_compute.semaphore;

    Tool::CheckResult(vkQueueSubmit(m_compute.queue, 1, &computeSubmitInfo, VK_NULL_HANDLE));

    // === 提交统一GPU管线 (Mark + Fill + Mipmap + SDF) ===
    // if (m_enableVoxelization) {
    SubmitUnifiedGPUPipeline();
    // }

    // 体素化调试已整合到统一GPU管线中，无需额外调用
    //{
    //	printf("=== Executing voxelization in render loop ===\n");

    //	// 确保体素化已初始化
    //	// static bool voxelizationInitialized = false;
    //	// if (!voxelizationInitialized)
    //	//{
    //	//	SetupVoxelizationPass();
    //	//	voxelizationInitialized = true;
    //	//}

    //	// 更新常量
    //	UpdateVoxelizationConstants();

    //	// 执行体素化
    //	ExecuteVoxelizationWithSync();

    //	// 保存纹理用于验证
    //	// static int frameCount = 0;
    //	// if (frameCount % 60 == 0) // 每60帧保存一次
    //	//{
    //	//	SaveVoxelTextureWithValidation("voxel_debug_frame_" + std::to_string(frameCount) + ".raw");
    //	//}
    //	// frameCount++;
    //}
    VkPipelineStageFlags graphicsWaitStageMasks[] = {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

    m_submitInfo.pWaitDstStageMask = graphicsWaitStageMasks;
    // 等待统一GPU管线完成，而不是compute semaphore
    m_submitInfo.pWaitSemaphores = &m_unifiedGPUPipeline.completionSemaphore;
    m_submitInfo.pSignalSemaphores = &m_semaphores.deferedSemaphore;
    m_submitInfo.commandBufferCount = 1;
    m_submitInfo.pCommandBuffers = &m_offScreenCmdBuffer;
    Tool::CheckResult(vkQueueSubmit(m_queues.graphicsQueue, 1, &m_submitInfo, VK_NULL_HANDLE));

    VkPipelineStageFlags graphicsWaitStageMasks1[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    m_submitInfo.pWaitDstStageMask = graphicsWaitStageMasks1;
    m_submitInfo.pWaitSemaphores = &m_semaphores.deferedSemaphore;
    m_submitInfo.pSignalSemaphores = &m_semaphores.renderComplete;
    m_submitInfo.pCommandBuffers = &m_drawCmdBuffers[currentBuffer];
    Tool::CheckResult(vkQueueSubmit(m_queues.graphicsQueue, 1, &m_submitInfo, VK_NULL_HANDLE));
}
void Renderer::SubmitFrame()
{
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = NULL;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapChain.swapChain;
    presentInfo.pImageIndices = &currentBuffer;
    // Check if a wait semaphore has been specified to wait for before presenting the image
    if (m_semaphores.renderComplete != VK_NULL_HANDLE)
    {
        presentInfo.pWaitSemaphores = &m_semaphores.renderComplete;
        presentInfo.waitSemaphoreCount = 1;
    }
    VkResult result = vkQueuePresentKHR(m_queues.presentQueue, &presentInfo);

    // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
    if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
    {
        ResizeWindow();
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            return;
        }
    }
    else
    {
        Tool::CheckResult(result);
    }
    Tool::CheckResult(vkQueueWaitIdle(m_queues.graphicsQueue));
    Tool::CheckResult(vkQueueWaitIdle(m_compute.queue));
}

// 信号量一组
// 无fence
// commande buffer 和 swapchain size皆为3
// 这里的绘制逻辑是逐帧渲染，，所以下一帧提交前所以不会有资源冲突问题，
// 因为这里不需要重新buildcommandbufer（没有东西改变），所以inflight 没有用（不需要进行tutorial中的同时多帧处理）
void Renderer::DrawFrame()
{
    UpdateUniformBufferOffscreen();
    UpdateLightCullingUBO();
    UpdateBuffers();
    PrepareFrame();
    Draw();
    SubmitFrame();
    // currentBuffer = (currentBuffer + 1) % m_swapChain.images.size();
}


void Renderer::SetupVoxelizationPass()
{
    // Initialize all voxelization components
    InitVoxelizationTextures();
    InitVoxelizationDescriptors();
    InitVoxelizationPipelines();
    UpdateVoxelizationConstants();
}

void Renderer::InitVoxelizationTextures()
{
    // Create voxel counter texture (R32_SINT format for atomic operations)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_3D;
    imageInfo.extent.width = config_->Sdf.Resolution;
    imageInfo.extent.height = config_->Sdf.Resolution;
    imageInfo.extent.depth = config_->Sdf.Resolution;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R32_SINT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_voxelizationPass.voxelCounterTexture.image) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create voxel counter texture!");
    }

    // Allocate memory for the texture
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_voxelizationPass.voxelCounterTexture.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_voxelizationPass.voxelCounterTexture.deviceMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate voxel counter texture memory!");
    }

    vkBindImageMemory(m_device, m_voxelizationPass.voxelCounterTexture.image, m_voxelizationPass.voxelCounterTexture.deviceMemory, 0);

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_voxelizationPass.voxelCounterTexture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    viewInfo.format = VK_FORMAT_R32_SINT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_voxelizationPass.voxelCounterTexture.view) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create voxel counter texture view!");
    }

    // Create final voxel state texture (R32_UINT format to match HLSL RWTexture3D<uint>)
    imageInfo.format = VK_FORMAT_R32_UINT;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_voxelizationPass.finalVoxelStateTexture.image) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create final voxel state texture!");
    }

    // Allocate memory for the final texture
    vkGetImageMemoryRequirements(m_device, m_voxelizationPass.finalVoxelStateTexture.image, &memRequirements);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_voxelizationPass.finalVoxelStateTexture.deviceMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate final voxel state texture memory!");
    }

    vkBindImageMemory(m_device, m_voxelizationPass.finalVoxelStateTexture.image, m_voxelizationPass.finalVoxelStateTexture.deviceMemory, 0);

    // Create image view for final texture
    viewInfo.image = m_voxelizationPass.finalVoxelStateTexture.image;
    viewInfo.format = VK_FORMAT_R32_UINT;

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_voxelizationPass.finalVoxelStateTexture.view) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create final voxel state texture view!");
    }

    // 将view传递给Mipmap
    m_gpuMipmapOctree->SetVoxelTexture(m_voxelizationPass.finalVoxelStateTexture.view);

    // Create uniform buffer for voxelization constants
    VkDeviceSize bufferSize = sizeof(VoxelizationPass::VoxelConstants);
    VkResult result = CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   &m_voxelizationPass.voxelUniformBuffer, bufferSize);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create voxelization uniform buffer!");
    }
}

void Renderer::InitVoxelizationDescriptors()
{
    // Ensure images are in the correct layout before updating descriptor sets
    // Create a temporary command buffer for layout transitions
    VkCommandBuffer layoutCmdBuffer = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Transition voxel counter texture to GENERAL layout
    VkImageMemoryBarrier voxelCounterBarrier{};
    voxelCounterBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    voxelCounterBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    voxelCounterBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    voxelCounterBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    voxelCounterBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    voxelCounterBarrier.image = m_voxelizationPass.voxelCounterTexture.image;
    voxelCounterBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    voxelCounterBarrier.subresourceRange.baseMipLevel = 0;
    voxelCounterBarrier.subresourceRange.levelCount = 1;
    voxelCounterBarrier.subresourceRange.baseArrayLayer = 0;
    voxelCounterBarrier.subresourceRange.layerCount = 1;
    voxelCounterBarrier.srcAccessMask = 0;
    voxelCounterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    // Transition final voxel state texture to GENERAL layout
    VkImageMemoryBarrier finalVoxelBarrier{};
    finalVoxelBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    finalVoxelBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    finalVoxelBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    finalVoxelBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalVoxelBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalVoxelBarrier.image = m_voxelizationPass.finalVoxelStateTexture.image;
    finalVoxelBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    finalVoxelBarrier.subresourceRange.baseMipLevel = 0;
    finalVoxelBarrier.subresourceRange.levelCount = 1;
    finalVoxelBarrier.subresourceRange.baseArrayLayer = 0;
    finalVoxelBarrier.subresourceRange.layerCount = 1;
    finalVoxelBarrier.srcAccessMask = 0;
    finalVoxelBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    VkImageMemoryBarrier barriers[] = {voxelCounterBarrier, finalVoxelBarrier};
    vkCmdPipelineBarrier(layoutCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 2,
                         barriers);

    // Submit and wait for layout transitions
    m_vulkanDevice->FlushCommandBuffer(layoutCmdBuffer, m_queues.graphicsQueue);

    // Create descriptor set layout for marking pass - 保守光栅化版本
    std::vector<VkDescriptorSetLayoutBinding> markBindings = {
        // Binding 0: Voxel counter texture UAV (matches register(u0) in shader)
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        // Binding 1: Uniform buffer (matches register(b1) in shader)
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};

    VkDescriptorSetLayoutCreateInfo markLayoutInfo{};
    markLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    markLayoutInfo.bindingCount = static_cast<uint32_t>(markBindings.size());
    markLayoutInfo.pBindings = markBindings.data();

    if (vkCreateDescriptorSetLayout(m_device, &markLayoutInfo, nullptr, &m_voxelizationPass.markPassDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create mark pass descriptor set layout!");
    }

    // Create descriptor set layout for fill pass
    std::vector<VkDescriptorSetLayoutBinding> fillBindings = {// Binding 0: Uniform buffer
                                                              {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                                                              // Binding 1: Voxel counter texture (input) - Texture3D<int> uses SAMPLED_IMAGE
                                                              {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                                                              // Binding 2: Final voxel state texture (output)
                                                              {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};

    VkDescriptorSetLayoutCreateInfo fillLayoutInfo{};
    fillLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    fillLayoutInfo.bindingCount = static_cast<uint32_t>(fillBindings.size());
    fillLayoutInfo.pBindings = fillBindings.data();

    if (vkCreateDescriptorSetLayout(m_device, &fillLayoutInfo, nullptr, &m_voxelizationPass.fillPassDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create fill pass descriptor set layout!");
    }

    // Allocate descriptor sets
    VkDescriptorSetAllocateInfo markAllocInfo{};
    markAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    markAllocInfo.descriptorPool = m_descriptorPool;
    markAllocInfo.descriptorSetCount = 1;
    markAllocInfo.pSetLayouts = &m_voxelizationPass.markPassDescriptorSetLayout;

    if (vkAllocateDescriptorSets(m_device, &markAllocInfo, &m_voxelizationPass.markPassDescriptorSet) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate mark pass descriptor set!");
    }

    VkDescriptorSetAllocateInfo fillAllocInfo{};
    fillAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    fillAllocInfo.descriptorPool = m_descriptorPool;
    fillAllocInfo.descriptorSetCount = 1;
    fillAllocInfo.pSetLayouts = &m_voxelizationPass.fillPassDescriptorSetLayout;

    if (vkAllocateDescriptorSets(m_device, &fillAllocInfo, &m_voxelizationPass.fillPassDescriptorSet) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate fill pass descriptor set!");
    }

    // Update descriptor sets
    // Mark pass descriptor set
    VkDescriptorBufferInfo markBufferInfo{};
    markBufferInfo.buffer = m_voxelizationPass.voxelUniformBuffer.buffer;
    markBufferInfo.offset = 0;
    markBufferInfo.range = sizeof(VoxelizationPass::VoxelConstants);

    VkDescriptorImageInfo markImageInfo{};
    markImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    markImageInfo.imageView = m_voxelizationPass.voxelCounterTexture.view;
    markImageInfo.sampler = VK_NULL_HANDLE;

    std::vector<VkWriteDescriptorSet> markWrites = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_voxelizationPass.markPassDescriptorSet, 0, 0,
                                                     1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &markImageInfo, nullptr, nullptr},
                                                    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_voxelizationPass.markPassDescriptorSet, 1, 0,
                                                     1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &markBufferInfo, nullptr}};

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(markWrites.size()), markWrites.data(), 0, nullptr);

    // Fill pass descriptor set
    VkDescriptorBufferInfo fillBufferInfo{};
    fillBufferInfo.buffer = m_voxelizationPass.voxelUniformBuffer.buffer;
    fillBufferInfo.offset = 0;
    fillBufferInfo.range = sizeof(VoxelizationPass::VoxelConstants);

    VkDescriptorImageInfo fillInputImageInfo{};
    fillInputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    fillInputImageInfo.imageView = m_voxelizationPass.voxelCounterTexture.view;
    fillInputImageInfo.sampler = VK_NULL_HANDLE;

    VkDescriptorImageInfo fillOutputImageInfo{};
    fillOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    fillOutputImageInfo.imageView = m_voxelizationPass.finalVoxelStateTexture.view;
    fillOutputImageInfo.sampler = VK_NULL_HANDLE;

    std::vector<VkWriteDescriptorSet> fillWrites = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_voxelizationPass.fillPassDescriptorSet, 0, 0,
                                                     1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &fillBufferInfo, nullptr},
                                                    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_voxelizationPass.fillPassDescriptorSet, 1, 0,
                                                     1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &fillInputImageInfo, nullptr, nullptr},
                                                    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_voxelizationPass.fillPassDescriptorSet, 2, 0,
                                                     1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &fillOutputImageInfo, nullptr, nullptr}};

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(fillWrites.size()), fillWrites.data(), 0, nullptr);
}
// 方案2: 使用“动态渲染”
void Renderer::InitVoxelizationPipelines()
{
    // 1. 对于动态渲染，我们不需要创建 VkRenderPass 和 VkFramebuffer 对象
    m_voxelizationPass.renderPass = VK_NULL_HANDLE;
    m_voxelizationPass.framebuffer = VK_NULL_HANDLE;
    m_voxelizationPass.depthImage = VK_NULL_HANDLE;
    m_voxelizationPass.depthImageView = VK_NULL_HANDLE;
    m_voxelizationPass.depthImageMemory = VK_NULL_HANDLE;

    // 加载着色器
    VkPipelineShaderStageCreateInfo vertShaderStageInfo =
        LoadShader(Tool::GetShadersPath() + "Voxelize/Voxelize.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    VkPipelineShaderStageCreateInfo fragShaderStageInfo =
        LoadShader(Tool::GetShadersPath() + "Voxelize/Voxelize.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipelineShaderStageCreateInfo compShaderStageInfo =
        LoadShader(Tool::GetShadersPath() + "Voxelize/ScanFill.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

    // 创建标记阶段的管线布局
    VkPipelineLayoutCreateInfo markPipelineLayoutInfo{};
    markPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    markPipelineLayoutInfo.setLayoutCount = 1;
    markPipelineLayoutInfo.pSetLayouts = &m_voxelizationPass.markPassDescriptorSetLayout;
    if (vkCreatePipelineLayout(m_device, &markPipelineLayoutInfo, nullptr, &m_voxelizationPass.markPassPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create mark pass pipeline layout!");
    }

    // 创建填充阶段的管线布局
    VkPipelineLayoutCreateInfo fillPipelineLayoutInfo{};
    fillPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    fillPipelineLayoutInfo.setLayoutCount = 1;
    fillPipelineLayoutInfo.pSetLayouts = &m_voxelizationPass.fillPassDescriptorSetLayout;
    if (vkCreatePipelineLayout(m_device, &fillPipelineLayoutInfo, nullptr, &m_voxelizationPass.fillPassPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create fill pass pipeline layout!");
    }

    // 创建标记阶段的图形管线
    VkGraphicsPipelineCreateInfo markPipelineInfo{};
    markPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    // 2. 关键修改：为动态渲染提供渲染信息
    VkPipelineRenderingCreateInfo renderingCreateInfo{};
    renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCreateInfo.colorAttachmentCount = 0; // 无颜色附件
    renderingCreateInfo.pColorAttachmentFormats = nullptr;
    renderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;   // 无深度附件
    renderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED; // 无模板附件

    markPipelineInfo.pNext = &renderingCreateInfo; // 将动态渲染信息链接到pNext

    // 着色器阶段
    std::vector<VkPipelineShaderStageCreateInfo> markShaderStages = {vertShaderStageInfo, fragShaderStageInfo};
    markPipelineInfo.stageCount = static_cast<uint32_t>(markShaderStages.size());
    markPipelineInfo.pStages = markShaderStages.data();

    // 顶点输入状态
    markPipelineInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position});

    // 输入装配状态
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    markPipelineInfo.pInputAssemblyState = &inputAssembly;

    // 视口状态
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    markPipelineInfo.pViewportState = &viewportState;

    // 光栅化状态
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE; // 体素化需要处理所有面
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    markPipelineInfo.pRasterizationState = &rasterizer;

    // 多重采样状态
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    markPipelineInfo.pMultisampleState = &multisampling;

    // 深度模板状态
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;  // 关闭深度测试
    depthStencil.depthWriteEnable = VK_FALSE; // 关闭深度写入
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    markPipelineInfo.pDepthStencilState = &depthStencil;

    // 颜色混合状态
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 0; // 无颜色附件
    colorBlending.pAttachments = nullptr;
    markPipelineInfo.pColorBlendState = &colorBlending;

    // 动态状态
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();
    markPipelineInfo.pDynamicState = &dynamicState;

    // 3. 将管线与动态渲染关联
    markPipelineInfo.layout = m_voxelizationPass.markPassPipelineLayout;
    markPipelineInfo.renderPass = VK_NULL_HANDLE; // 关键：对于动态渲染，renderPass必须为 VK_NULL_HANDLE
    markPipelineInfo.subpass = 0;
    markPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &markPipelineInfo, nullptr, &m_voxelizationPass.markPassPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create mark pass pipeline!");
    }

    // 创建计算管线
    VkComputePipelineCreateInfo fillPipelineInfo{};
    fillPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    fillPipelineInfo.stage = compShaderStageInfo;
    fillPipelineInfo.layout = m_voxelizationPass.fillPassPipelineLayout;

    if (vkCreateComputePipelines(m_device, m_pipelineCache, 1, &fillPipelineInfo, nullptr, &m_voxelizationPass.fillPassPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create fill pass pipeline!");
    }
}
void Renderer::UpdateVoxelizationConstants()
{
    // Model 和 View 矩阵保持为单位矩阵
    m_voxelizationPass.constants.model = m_glTFModel.GetModelToStandardTransform();
    m_voxelizationPass.constants.view = glm::mat4(1.0f);

    m_voxelizationPass.constants.projection = glm::ortho(-1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f);
    m_voxelizationPass.constants.voxelGridSize = glm::uvec3(config_->Sdf.Resolution);

    // 拷贝到GPU缓冲
    void* data;
    Tool::CheckResult(vkMapMemory(m_device, m_voxelizationPass.voxelUniformBuffer.memory, 0, sizeof(VoxelizationPass::VoxelConstants), 0, &data));

    memcpy(data, &m_voxelizationPass.constants, sizeof(VoxelizationPass::VoxelConstants));
    vkUnmapMemory(m_device, m_voxelizationPass.voxelUniformBuffer.memory);
}

void Renderer::VoxelizationMarkPass(VkCommandBuffer cmd)
{
    // Validate required resources
    if (m_voxelizationPass.voxelCounterTexture.image == VK_NULL_HANDLE || m_voxelizationPass.markPassPipeline == VK_NULL_HANDLE)
    {
        printf("Error: Voxelization mark pass resources not properly initialized\n");
        return;
    }

    // Begin debug label
    BeginDebugLabel(cmd, "Voxelization Mark Pass", 0.0f, 1.0f, 0.0f, 1.0f);

    // --- 纹理清理和屏障部分=---
    // Clear voxel counter texture to zero
    VkImageMemoryBarrier clearBarrier{};
    clearBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    clearBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    clearBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    clearBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    clearBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    clearBarrier.image = m_voxelizationPass.voxelCounterTexture.image;
    clearBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearBarrier.subresourceRange.baseMipLevel = 0;
    clearBarrier.subresourceRange.levelCount = 1;
    clearBarrier.subresourceRange.baseArrayLayer = 0;
    clearBarrier.subresourceRange.layerCount = 1;
    clearBarrier.srcAccessMask = 0;
    clearBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &clearBarrier);

    VkClearColorValue clearValue{};
    clearValue.int32[0] = 0;
    vkCmdClearColorImage(cmd, m_voxelizationPass.voxelCounterTexture.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearBarrier.subresourceRange);

    // Transition to storage layout for marking pass
    VkImageMemoryBarrier storageBarrier{};
    storageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    storageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    storageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    storageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    storageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    storageBarrier.image = m_voxelizationPass.voxelCounterTexture.image;
    storageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    storageBarrier.subresourceRange.levelCount = 1;
    storageBarrier.subresourceRange.layerCount = 1;
    storageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    storageBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &storageBarrier);
    // --- 屏障部分结束 ---

    // === 新增：开始动态渲染 ===
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, {config_->Sdf.Resolution, config_->Sdf.Resolution}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 0; // 无颜色附件
    renderingInfo.pColorAttachments = nullptr;
    renderingInfo.pDepthAttachment = nullptr; // 无深度附件
    renderingInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderingInfo); // <--- 关键修复：添加此行

    // === 设置视口和裁剪 ===
    VkViewport viewport = {};
    viewport.width = static_cast<float>(config_->Sdf.Resolution);
    viewport.height = static_cast<float>(config_->Sdf.Resolution);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = {config_->Sdf.Resolution, config_->Sdf.Resolution};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // === 绑定管线和描述符集 ===
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_voxelizationPass.markPassPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_voxelizationPass.markPassPipelineLayout, 0, 1,
                            &m_voxelizationPass.markPassDescriptorSet, 0, nullptr);

    // === 绘制模型 ===
    m_glTFModel.Draw(cmd);

    // === 新增：结束动态渲染 ===
    vkCmdEndRendering(cmd); 

    // === 队列族所有权释放屏障  ===
    VkImageMemoryBarrier releaseBarrier{};
    releaseBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    releaseBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    releaseBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    releaseBarrier.srcQueueFamilyIndex = m_index.graphics;
    releaseBarrier.dstQueueFamilyIndex = m_index.compute;
    releaseBarrier.image = m_voxelizationPass.voxelCounterTexture.image;
    releaseBarrier.subresourceRange = storageBarrier.subresourceRange; // 复用之前的subresourceRange
    releaseBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    releaseBarrier.dstAccessMask = 0;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &releaseBarrier);

    EndDebugLabel(cmd);
}

void Renderer::VoxelizationFillPass(VkCommandBuffer cmd)
{
    // Validate required resources
    if (m_voxelizationPass.voxelCounterTexture.image == VK_NULL_HANDLE || m_voxelizationPass.finalVoxelStateTexture.image == VK_NULL_HANDLE ||
        m_voxelizationPass.fillPassPipeline == VK_NULL_HANDLE)
    {
        printf("Error: Voxelization fill pass resources not properly initialized\n");
        return;
    }

    // Begin debug label
    BeginDebugLabel(cmd, "Voxelization Fill Pass", 0.0f, 0.0f, 1.0f, 1.0f);

    // 关键修改：获取队列族所有权 (Graphics -> Compute)
    VkImageMemoryBarrier acquireBarrier{};
    acquireBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    acquireBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    acquireBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    acquireBarrier.srcQueueFamilyIndex = m_index.graphics; // 来源：图形队列族
    acquireBarrier.dstQueueFamilyIndex = m_index.compute;  // 目标：计算队列族
    acquireBarrier.image = m_voxelizationPass.voxelCounterTexture.image;
    acquireBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    acquireBarrier.subresourceRange.baseMipLevel = 0;
    acquireBarrier.subresourceRange.levelCount = 1;
    acquireBarrier.subresourceRange.baseArrayLayer = 0;
    acquireBarrier.subresourceRange.layerCount = 1;
    acquireBarrier.srcAccessMask = 0; // 获取时不需要源访问权限
    acquireBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &acquireBarrier);

    // Bind fill pass pipeline and descriptor set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_voxelizationPass.fillPassPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_voxelizationPass.fillPassPipelineLayout, 0, 1,
                            &m_voxelizationPass.fillPassDescriptorSet, 0, nullptr);

    // Dispatch compute shader
    uint32_t groupSizeX = 8;
    uint32_t groupSizeY = 8;
    uint32_t dispatchX = (config_->Sdf.Resolution + groupSizeX - 1) / groupSizeX;
    uint32_t dispatchY = (config_->Sdf.Resolution + groupSizeY - 1) / groupSizeY;
    vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

    EndDebugLabel(cmd);
}
void Renderer::SaveVoxelTextureWithValidation(const std::string& filename)
{
    const uint32_t gridSize = config_->Sdf.Resolution;
    const VkDeviceSize imageSize = gridSize * gridSize * gridSize * 4; // RGBA

    printf("Starting voxel texture save: %dx%dx%d (%llu bytes)\n", gridSize, gridSize, gridSize, imageSize);

    // 1. 验证源纹理
    if (m_voxelizationPass.finalVoxelStateTexture.image == VK_NULL_HANDLE)
    {
        printf("Error: finalVoxelStateTexture is null\n");
        return;
    }

    // 2. 创建staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    // 创建buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    Tool::CheckResult(vkCreateBuffer(m_device, &bufferInfo, nullptr, &stagingBuffer));

    // 分配内存
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        m_vulkanDevice->GetMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    Tool::CheckResult(vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingBufferMemory));
    Tool::CheckResult(vkBindBufferMemory(m_device, stagingBuffer, stagingBufferMemory, 0));

    // 3. 创建命令缓冲区
    VkCommandBuffer copyCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    if (copyCmd == VK_NULL_HANDLE)
    {
        printf("Error: Failed to create copy command buffer\n");
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingBufferMemory, nullptr);
        return;
    }

    // 4. 图像布局转换 - 确保从当前状态正确转换
    VkImageMemoryBarrier transferBarrier{};
    transferBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transferBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL; // 确保这与当前布局匹配
    transferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    transferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transferBarrier.image = m_voxelizationPass.finalVoxelStateTexture.image;
    transferBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transferBarrier.subresourceRange.baseMipLevel = 0;
    transferBarrier.subresourceRange.levelCount = 1;
    transferBarrier.subresourceRange.baseArrayLayer = 0;
    transferBarrier.subresourceRange.layerCount = 1;
    transferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // 来自计算着色器写入
    transferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &transferBarrier);

    // 5. 复制图像到buffer
    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;   // 0表示紧密排列
    copyRegion.bufferImageHeight = 0; // 0表示紧密排列
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent = {gridSize, gridSize, gridSize};

    vkCmdCopyImageToBuffer(copyCmd, m_voxelizationPass.finalVoxelStateTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1,
                           &copyRegion);

    // 6. 提交命令并等待完成
    m_vulkanDevice->FlushCommandBuffer(copyCmd, m_queues.graphicsQueue, true);

    // 7. 映射内存并验证数据
    void* mappedData;
    Tool::CheckResult(vkMapMemory(m_device, stagingBufferMemory, 0, imageSize, 0, &mappedData));

    // 8. 快速数据验证
    uint8_t* byteData = static_cast<uint8_t*>(mappedData);
    size_t nonZeroCount = 0;
    size_t totalBytes = imageSize;

    // 采样检查（避免遍历所有数据影响性能）
    size_t sampleStep = std::max(totalBytes / 10000, size_t(1)); // 采样约10000个点
    for (size_t i = 0; i < totalBytes; i += 1)
    {
        if (byteData[i] != 0)
            nonZeroCount++;
    }

    printf("Data validation: %zu/%zu sampled bytes are non-zero\n", nonZeroCount, totalBytes / sampleStep);

    // 9. 保存到文件
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        printf("Error: Could not open file for writing: %s\n", filename.c_str());
        vkUnmapMemory(m_device, stagingBufferMemory);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingBufferMemory, nullptr);
        return;
    }

    file.write(static_cast<const char*>(mappedData), imageSize);
    if (file.fail())
    {
        printf("Error: Failed to write data to file\n");
    }
    else
    {
        printf("✅ Successfully saved voxel texture: %s (%llu bytes)\n", filename.c_str(), imageSize);
    }

    file.close();
    vkUnmapMemory(m_device, stagingBufferMemory);

    // 10. 清理资源
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingBufferMemory, nullptr);
}

void Renderer::BuildVoxelizationCommandBuffer()
{
}
void Renderer::ExecuteVoxelizationWithSync()
{
    // 开始时间统计
    auto startTime = std::chrono::high_resolution_clock::now();

    // Step 1: Execute graphics pass (marking phase)
    VkCommandBuffer graphicsCmdBuffer = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    if (graphicsCmdBuffer == VK_NULL_HANDLE)
    {
        printf("Error: Failed to create graphics command buffer for voxelization\n");
        return;
    }

    // Execute marking phase (graphics pipeline)
    VoxelizationMarkPass(graphicsCmdBuffer);
    Tool::CheckResult(vkEndCommandBuffer(graphicsCmdBuffer));

    // Step 2: Create compute command buffer
    VkCommandBufferAllocateInfo cmdBufAllocateInfo = Init::commandBufferAllocateInfo(m_compute.commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
    VkCommandBuffer computeCmdBuffer;
    Tool::CheckResult(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, &computeCmdBuffer));

    VkCommandBufferBeginInfo cmdBufInfo = Init::commandBufferBeginInfo();
    Tool::CheckResult(vkBeginCommandBuffer(computeCmdBuffer, &cmdBufInfo));
    VoxelizationFillPass(computeCmdBuffer);
    Tool::CheckResult(vkEndCommandBuffer(computeCmdBuffer));

    // Step 3: 使用信号量进行队列间同步
    VkSemaphoreCreateInfo semaphoreInfo = Init::semaphoreCreateInfo();
    VkSemaphore transferSemaphore;
    Tool::CheckResult(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &transferSemaphore));
    auto cmdBufferTime = std::chrono::high_resolution_clock::now();
    // 提交图形命令缓冲区，发出信号量
    VkSubmitInfo graphicsSubmitInfo = Init::submitInfo();
    graphicsSubmitInfo.commandBufferCount = 1;
    graphicsSubmitInfo.pCommandBuffers = &graphicsCmdBuffer;
    graphicsSubmitInfo.signalSemaphoreCount = 1;
    graphicsSubmitInfo.pSignalSemaphores = &transferSemaphore;

    Tool::CheckResult(vkQueueSubmit(m_queues.graphicsQueue, 1, &graphicsSubmitInfo, VK_NULL_HANDLE));

    // 提交计算命令缓冲区，等待信号量
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
    VkSubmitInfo computeSubmitInfo = Init::submitInfo();
    computeSubmitInfo.waitSemaphoreCount = 1;
    computeSubmitInfo.pWaitSemaphores = &transferSemaphore;
    computeSubmitInfo.pWaitDstStageMask = waitStages;
    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers = &computeCmdBuffer;

    // 创建围栏等待计算完成
    VkFenceCreateInfo fenceInfo = Init::fenceCreateInfo(VkFenceCreateFlags(0));
    VkFence computeFence;
    Tool::CheckResult(vkCreateFence(m_device, &fenceInfo, nullptr, &computeFence));

    Tool::CheckResult(vkQueueSubmit(m_compute.queue, 1, &computeSubmitInfo, computeFence));

    // 等待计算完成
    Tool::CheckResult(vkWaitForFences(m_device, 1, &computeFence, VK_TRUE, UINT64_MAX));
    auto endTime = std::chrono::high_resolution_clock::now();
    // 注意：由于使用信号量同步，无法准确测量各个阶段的独立执行时间
    // 这里只能测量整个voxelization过程的总时间
    // 如果需要精确测量各阶段时间，需要使用时间戳查询或事件

    // 清理资源
    vkDestroySemaphore(m_device, transferSemaphore, nullptr);
    vkDestroyFence(m_device, computeFence, nullptr);
    vkFreeCommandBuffers(m_device, m_vulkanDevice->commandPool, 1, &graphicsCmdBuffer);
    vkFreeCommandBuffers(m_device, m_compute.commandPool, 1, &computeCmdBuffer);

    // 结束时间统计

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    printf("Voxelization execution time: %.3f ms\n", duration.count() / 1000.0f);
}
// 统计八叉树中不同节点状态数量的递归函数
// void CountVoxelOctreeNodeStates(
//	const std::shared_ptr<VoxelOctreeNode>& node,
//	std::unordered_map<VoxelState, int>& stateCounts)
//{
//	if (!node)
//		return;
//
//	// 统计当前节点状态
//	stateCounts[node->state]++;
//
//	// 递归遍历子节点
//	for (const auto& child : node->children)
//	{
//		if (child)
//		{
//			CountVoxelOctreeNodeStates(child, stateCounts);
//		}
//	}
//}
// void Renderer::TestVoxelOctreeFromVoxelData()
//{
//	// 1. 读取体素数据并构造八叉树，同时计时
//	auto tStartReadData = std::chrono::high_resolution_clock::now();
//
//	// 读取体素数据
//	ReadFinalVoxelStateTextureToCPU_Staging();
//	auto tEndReadData = std::chrono::high_resolution_clock::now();
//	auto tDuration = std::chrono::duration<double, std::milli>(tEndReadData - tStartReadData).count();
//	std::print("Read Data time:{}", tDuration);
//	// 获取网格大小
//	const int gridSize = m_voxelizationPass.GRID_SIZE;
//
//	// 构造VoxelOctree实例
//	VoxelOctree voxelOctree(voxelData_, gridSize);
//	auto tEndOctree = std::chrono::high_resolution_clock::now();
//	tDuration = std::chrono::duration<double, std::milli>(tEndOctree - tEndReadData).count();
//	std::print("Octree time:{}", tDuration);
//	// 4. 获取根节点，简单打印验证
//	auto root = voxelOctree.GetRoot();
//	if (root)
//	{
//		std::cout << "VoxelOctree root center: (" << root->center.x << ", " << root->center.y << ", " << root->center.z << ")\n";
//		std::cout << "VoxelOctree root halfSize: " << root->halfSize << "\n";
//		std::cout << "VoxelOctree root state: " << static_cast<int>(root->state) << "\n";
//	}
//	else
//	{
//		std::cout << "VoxelOctree root is null.\n";
//	}
//
//	// 5. 统计八叉树节点状态数量
//	std::unordered_map<VoxelState, int> stateCounts;
//	CountVoxelOctreeNodeStates(root, stateCounts);
//
//	// 6. 输出统计结果
//	std::cout << "VoxelOctree Node States Count:\n";
//	std::cout << "EMPTY: " << stateCounts[VoxelState::EMPTY] << "\n";
//	std::cout << "SOLID: " << stateCounts[VoxelState::SOLID] << "\n";
//	std::cout << "MIXED: " << stateCounts[VoxelState::MIXED] << "\n";
//}

void Renderer::TestVoxelization()
{
    try
    {
        // Initialize voxelization if not already done
        static bool initialized = false;
        if (!initialized)
        {
            // Check if model is loaded before setting up voxelization
            if (m_glTFModel.vertexBuffer.empty())
            {
                printf("Warning: Model not loaded, skipping voxelization test\n");
                return;
            }

            SetupVoxelizationPass();
            initialized = true;
        }

        //// Update constants (仅在测试时更新，实际更新在Draw()中)
        // UpdateVoxelizationConstants(); // 移到Draw()中

        // 执行统一GPU管线已移到Draw()函数中，这里不再执行
        // ExecuteVoxelizationWithSync(); // 已合并到统一管线
        // TestVoxelOctreeFromVoxelData();

        // 构建GPU Mipmap隐式八叉树

        // Setup and render voxel point cloud
        // if (!m_voxelPointCloud.initialized)
        // {
        // 	SetupVoxelPointCloud();
        // }

        // // Extract voxel points for rendering
        // ExtractVoxelPoints();

        // printf("Voxelization test completed successfully!\n");
        // printf("Voxel slice rendering is now available!\n");
        // printf("You can call RenderVoxelSlice() to see the voxel slices!\n");
        // printf("Use keyboard: 1=XY, 2=XZ, 3=YZ, +/- to change slice depth\n");

        // Save voxel texture for validation
        // SaveVoxelTextureWithValidation("voxel_texture_validation.raw");
    }
    catch (const std::exception& e)
    {
        printf("Error during voxelization test: %s\n", e.what());
    }
    catch (...)
    {
        printf("Unknown error during voxelization test\n");
    }
}

void Renderer::Cleanup()
{
    // 清理统一GPU管线资源
    m_unifiedGPUPipeline.cleanup(m_device);
    analyticalNodeSelection_.cleanup(m_device);
    // multiViewNodeSelection_.cleanup(m_device);
    m_analyticalSDFGeneration.cleanup(m_device);
    m_gpuDataPreparation.cleanup(m_device);
    m_multiViewDepthSDF4C.cleanup(m_device); // 新的多视角深度SDF方案清理

    // cmd buffer
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);

    // depth
    vkDestroyImageView(m_device, m_depthStencil.view, nullptr);
    vkDestroyImage(m_device, m_depthStencil.image, nullptr);
    vkFreeMemory(m_device, m_depthStencil.memory, nullptr);

    // semaphore
    vkDestroySemaphore(m_device, m_semaphores.presentComplete, nullptr);
    vkDestroySemaphore(m_device, m_semaphores.renderComplete, nullptr);

    // vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.Textures, nullptr);
    // vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.Matrices, nullptr);
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);

    for (auto& frameBuffer : m_frameBuffers)
    {
        vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
    }
    for (auto& shaderModule : m_shaderModules)
    {
        vkDestroyShaderModule(m_device, shaderModule, nullptr);
    }
    // uniformbuffer
    m_uboBuffer.Destroy();

    m_glTFModel.Destroy();
    m_swapChain.Cleanup();

    m_UI.FreeResources();

    // Cleanup voxelization resources
    vkDestroyImageView(m_device, m_voxelizationPass.voxelCounterTexture.view, nullptr);
    vkDestroyImage(m_device, m_voxelizationPass.voxelCounterTexture.image, nullptr);
    vkFreeMemory(m_device, m_voxelizationPass.voxelCounterTexture.deviceMemory, nullptr);

    vkDestroyImageView(m_device, m_voxelizationPass.finalVoxelStateTexture.view, nullptr);
    vkDestroyImage(m_device, m_voxelizationPass.finalVoxelStateTexture.image, nullptr);
    vkFreeMemory(m_device, m_voxelizationPass.finalVoxelStateTexture.deviceMemory, nullptr);

    m_voxelizationPass.voxelUniformBuffer.Destroy();

    vkDestroyPipeline(m_device, m_voxelizationPass.markPassPipeline, nullptr);
    vkDestroyPipeline(m_device, m_voxelizationPass.fillPassPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_voxelizationPass.markPassPipelineLayout, nullptr);
    vkDestroyPipelineLayout(m_device, m_voxelizationPass.fillPassPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_voxelizationPass.markPassDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_voxelizationPass.fillPassDescriptorSetLayout, nullptr);
    vkDestroyFramebuffer(m_device, m_voxelizationPass.framebuffer, nullptr);
    vkDestroyImageView(m_device, m_voxelizationPass.depthImageView, nullptr);
    vkDestroyImage(m_device, m_voxelizationPass.depthImage, nullptr);
    vkFreeMemory(m_device, m_voxelizationPass.depthImageMemory, nullptr);
    vkDestroyRenderPass(m_device, m_voxelizationPass.renderPass, nullptr);

    m_glTFModel.Destroy();

    if (m_neededFeatures.validation)
    {
        DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }

    glfwDestroyWindow(m_window);
    glfwTerminate();

    delete m_vulkanDevice;
    vkDestroyInstance(m_instance, nullptr);
}

void Renderer::CreateLogicalDevice()
{
    // 设置队列属性 还没创建队列
    QueueFamilyIndices& indices = m_indices;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

    // 某个显卡支持特定的队列族，某些队列族只支持渲染，某些只支持显示，某些两者都支持
    // 之前已经判断过物理设备一定支持这两个队列族，这里是防止因为他们相同导致重复创建队列
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        // 只创建该队列族的一个队列
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    // 物理设备属性
    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    // 这里 没有判断，而是指定让物理设备开启这些功能
    // TODO

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE; // Enable dynamic rendering for voxelization
    features13.pNext = &features12;

    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE;
    deviceFeatures.fragmentStoresAndAtomics = VK_TRUE; // Enable fragment shader atomic operations for voxelization

    // Create a chain: features13 -> features12 -> deviceFeatures2
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.features = deviceFeatures;
    deviceFeatures2.pNext = &features13;

    createInfo.pEnabledFeatures = nullptr; // Don't use pEnabledFeatures when using pNext
    createInfo.pNext = &deviceFeatures2;

    // Debug: Print the features we're trying to enable
    std::cout << "Enabling device features:" << std::endl;
    std::cout << "  fragmentStoresAndAtomics: " << (deviceFeatures.fragmentStoresAndAtomics ? "YES" : "NO") << std::endl;
    std::cout << "  dynamicRendering: " << (features13.dynamicRendering ? "YES" : "NO") << std::endl;
    // 指定启用的扩展（交换链）获取物理设备时已判断扩展是否可用
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

    if (m_neededFeatures.validation)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }
    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device");
    }
    // 保存队列
    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_queues.graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_queues.presentQueue);
}

void Renderer::LoadAssets()
{
    uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors;
    /*LoadglTFFile(Tool::GetAssetsPath() + "Models/FlightHelmet/glTF/FlightHelmet.gltf");*/
    m_glTFModel.loadFromFile(Tool::GetAssetsPath() + config_->modelPath, m_vulkanDevice, m_queues.graphicsQueue, glTFLoadingFlags);
    auto tStart = std::chrono::high_resolution_clock::now();
    // m_meshOctree = std::make_unique<MeshOctree>(m_glTFModel, 0.1f, 5);
    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    // std::cout << "Build MeshOctree cost: " << tDiff << " ms\n";
    // glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors |
    // vkglTF::FileLoadingFlags::FlipY;
    scene.skybox.loadFromFile(Tool::GetAssetsPath() + "models/Cube/cube.gltf", m_vulkanDevice, m_queues.graphicsQueue, glTFLoadingFlags);
    scene.textures.environmentCube.LoadFromFile(Tool::GetAssetsPath() + "textures/hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, m_vulkanDevice,
                                                m_queues.graphicsQueue);
    m_blueNoise.LoadFromFile(Tool::GetAssetsPath() + "textures/LDR_RGBA_3.png", VK_FORMAT_R8G8B8A8_UNORM, m_vulkanDevice, m_queues.graphicsQueue,
                             VK_FILTER_NEAREST);
}
void Renderer::CreateBuffersSpotLightShadow()
{
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &m_spotLightPass.CBuffer, m_shadowSpotLightCount * sizeof(SpotLightShadowPass::CBufferDesc)));
    Tool::CheckResult(m_spotLightPass.CBuffer.Map());
}
void Renderer::CreateBuffersSpotLight()
{
    if (m_lights.SpotLights.size() != 0)
    {
        VkDeviceSize bufferSize{m_lights.SpotLights.size() * sizeof(SpotLight)};
        Buffer stagingBuffer;

        m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     &stagingBuffer, bufferSize, m_lights.SpotLights.data());

        m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     &SpotLightBuffer, bufferSize);

        VkCommandBuffer copyCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VkBufferCopy copyRegion{};
        copyRegion.size = bufferSize;
        vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, SpotLightBuffer.buffer, 1, &copyRegion);

        m_vulkanDevice->FlushCommandBuffer(copyCmd, m_queues.graphicsQueue, true);

        stagingBuffer.Destroy();
    }
    // Tool::CheckResult(m_uniformBuffers.shadowGeometryShader.Map());
    // UpdateCBufferSpotLight();
}
void Renderer::PrepareUniformBuffers()
{
    // CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_uboBuffer,
    // sizeof(m_uboMatrices)); m_uboBuffer.Map();
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &m_uniformBuffers.defered, sizeof(UniformDataOffscreen)));

    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &m_uniformBuffers.skyBox, sizeof(UniformDataSkybox)));
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &m_uniformBuffers.postParam, sizeof(Params)));
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &m_uniformBuffers.blurParams, sizeof(UBOBlurParams)));
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_uniformBuffers.FXAA,
                                                   sizeof(FXAAParams)));

    // Map persistent
    Tool::CheckResult(m_uniformBuffers.defered.Map());
    Tool::CheckResult(m_uniformBuffers.skyBox.Map());
    Tool::CheckResult(m_uniformBuffers.postParam.Map());
    Tool::CheckResult(m_uniformBuffers.blurParams.Map());
    Tool::CheckResult(m_uniformBuffers.FXAA.Map());

    UpdateUniformBufferPost();
    UpdateUniformBuffersBlur();
    UpdateUniformBufferFXAA();
}

void Renderer::CreatePipelineCache()
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    Tool::CheckResult(vkCreatePipelineCache(m_device, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache));
}
// model
void Renderer::BuildDeferredCommandBuffer()
{
    if (m_offScreenCmdBuffer == VK_NULL_HANDLE)
    {
        m_offScreenCmdBuffer = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
    }
    else
    {
        vkResetCommandBuffer(m_offScreenCmdBuffer, 0);
    }

    // Create a semaphore used to synchronize offscreen rendering and usage
    if (!m_semaphores.deferedSemaphore)
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo = Init::semaphoreCreateInfo();
        Tool::CheckResult(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_semaphores.deferedSemaphore));
    }

    VkCommandBufferBeginInfo cmdBufInfo = Init::commandBufferBeginInfo();
    Tool::CheckResult(vkBeginCommandBuffer(m_offScreenCmdBuffer, &cmdBufInfo));

    // acquire
    if (m_index.graphics != m_index.compute)
    {
        std::vector<VkBufferMemoryBarrier> bufferBarriers;
        VkBufferMemoryBarrier bufferBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                               nullptr,
                                               0,
                                               VK_ACCESS_SHADER_READ_BIT,
                                               m_index.compute,
                                               m_index.graphics,
                                               m_compute.buffers.tiles.buffer,
                                               0,
                                               m_compute.buffers.tiles.size};
        bufferBarriers.push_back(bufferBarrier);
        bufferBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                         nullptr,
                         0,
                         VK_ACCESS_SHADER_READ_BIT,
                         m_index.compute,
                         m_index.graphics,
                         PointLightBuffer.buffer,
                         0,
                         PointLightBuffer.size};
        vkCmdPipelineBarrier(m_offScreenCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                             bufferBarriers.size(), bufferBarriers.data(), 0, nullptr);
    }

    std::array<VkClearValue, 5> clearValues = {};
    clearValues[0].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo renderPassBeginInfo = Init::renderPassBeginInfo();
    VkViewport viewport;
    VkRect2D scissor;

    // First pass: Shadow map generation
    //  spot light Gemtory shadow pass
    // renderPassBeginInfo.renderPass = m_framebuffers.shadow->renderPass;
    // renderPassBeginInfo.framebuffer = m_framebuffers.shadow->framebuffer;
    // renderPassBeginInfo.renderArea.extent.width = m_framebuffers.shadow->width;
    // renderPassBeginInfo.renderArea.extent.height = m_framebuffers.shadow->height;
    // renderPassBeginInfo.clearValueCount = 1;
    // renderPassBeginInfo.pClearValues = clearValues.data();
    /*BeginDebugLabel(m_offScreenCmdBuffer, "Spot Light Gemotry Pass", 1.0f, 0.0f, 0.0f);
    vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);*/
    // vkCmdSetViewport(m_offScreenCmdBuffer, 0, 1, &viewport);
    // vkCmdSetScissor(m_offScreenCmdBuffer, 0, 1, &scissor);
    // vkCmdSetDepthBias(
    //	m_offScreenCmdBuffer,
    //	m_shadowSettings.depthBiasConstant,
    //	0.0f,
    //	m_shadowSettings.depthBiasSlope);// 给depth image 一个偏移（偏移后是最终结果，采样的时候就不用管了）
    // vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.shadow);
    // vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.shadow, 0, 1, &m_descriptorSets.shadow, 0,
    // nullptr); m_glTFModel.Draw(m_offScreenCmdBuffer); vkCmdEndRenderPass(m_offScreenCmdBuffer); EndDebugLabel(m_offScreenCmdBuffer);

    // Directional Light CSM Generate
    BeginDebugLabel(m_offScreenCmdBuffer, "Directional Light CSM Generate", 1.0f, 0.9f, 0.85f);
    clearValues[0].depthStencil = {1.0f, 0};
    renderPassBeginInfo = Init::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = m_CSMPass.renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = SHADOWMAP_DIM;
    renderPassBeginInfo.renderArea.extent.height = SHADOWMAP_DIM;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues.data();
    viewport = Init::viewport((float)SHADOWMAP_DIM, (float)SHADOWMAP_DIM, 0.0f, 1.0f);
    vkCmdSetViewport(m_offScreenCmdBuffer, 0, 1, &viewport);
    scissor = Init::rect2D(SHADOWMAP_DIM, SHADOWMAP_DIM, 0, 0);
    vkCmdSetScissor(m_offScreenCmdBuffer, 0, 1, &scissor);
    // One pass per cascade
    for (int x = 0; x < m_lights.DirLights.size(); ++x)
    {
        if (m_lights.DirLights[x].castShadow == 1)
        {
            for (uint32_t j = 0; j < SHADOW_MAP_CASCADE_COUNT; j++)
            {
                renderPassBeginInfo.framebuffer = m_CSMPass.Cascades[m_lights.DirLights[x].index][j].frameBuffer;
                vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
                vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CSMPass.pipeline);
                CSMPass::PushBlock pushConstBlock = {m_lights.DirLights[x].index, j};
                vkCmdPushConstants(m_offScreenCmdBuffer, m_CSMPass.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CSMPass::PushBlock),
                                   &pushConstBlock);
                vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CSMPass.pipelineLayout, 0, 1, &m_CSMPass.set, 0,
                                        nullptr);
                m_glTFModel.Draw(m_offScreenCmdBuffer);
                vkCmdEndRenderPass(m_offScreenCmdBuffer);

                // VkImageSubresourceRange subresourceRange = {};
                // subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                // subresourceRange.baseMipLevel = 0;
                // subresourceRange.levelCount = 1;
                // subresourceRange.baseArrayLayer = 0;
                // subresourceRange.layerCount = 4;
                // Tool::SetImageLayout(
                //	m_offScreenCmdBuffer,
                //	m_CSMPass.Depths[j].image,
                //	VK_IMAGE_LAYOUT_UNDEFINED,
                //	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                //	subresourceRange);
            }
        }
    }
    EndDebugLabel(m_offScreenCmdBuffer);

    viewport = Init::viewport((float)ShadowOmniPass::WIDTH, (float)ShadowOmniPass::HEIGHT, 0.0f, 1.0f);
    vkCmdSetViewport(m_offScreenCmdBuffer, 0, 1, &viewport);
    scissor = Init::rect2D(ShadowOmniPass::WIDTH, ShadowOmniPass::HEIGHT, 0, 0);
    vkCmdSetScissor(m_offScreenCmdBuffer, 0, 1, &scissor);
    if (m_shadowPointLightCount != 0)
    {
        BeginDebugLabel(m_offScreenCmdBuffer, "ShadowOmni Pass", 0.0f, 1.0f, 0.0f);
        int j = 0;
        for (int i = 0; i < m_lights.PointLights.size(); ++i)
        {
            if (m_lights.PointLights[i].castShadow == 1)
            {
                for (uint32_t face = 0; face < 6; face++)
                {
                    UpdateCubeFace(j, face, m_offScreenCmdBuffer, m_shadowOmniPass.Set);
                }
                j += 1;
            }
        }
        EndDebugLabel(m_offScreenCmdBuffer);
    }

    // Original deferred G-Buffer generation pass
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[3].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[4].depthStencil = {1.0f, 0};
    renderPassBeginInfo.renderPass = m_framebuffers.deferred->renderPass;
    renderPassBeginInfo.framebuffer = m_framebuffers.deferred->framebuffer;
    renderPassBeginInfo.renderArea.extent.width = m_framebuffers.deferred->width;
    renderPassBeginInfo.renderArea.extent.height = m_framebuffers.deferred->height;
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues = clearValues.data();
    BeginDebugLabel(m_offScreenCmdBuffer, "Model Pass", 0.0f, 1.0f, 0.0f);
    vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    viewport = Init::viewport((float)m_framebuffers.deferred->width, (float)m_framebuffers.deferred->height, 0.0f, 1.0f);
    vkCmdSetViewport(m_offScreenCmdBuffer, 0, 1, &viewport);
    scissor = Init::rect2D(m_framebuffers.deferred->width, m_framebuffers.deferred->height, 0, 0);
    vkCmdSetScissor(m_offScreenCmdBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.defered);
    vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.defered, 0, 1, &m_descriptorSets.deferedModel, 0,
                            nullptr);
    m_glTFModel.Draw(m_offScreenCmdBuffer, vkglTF::RenderFlags::BindImages, m_pipelineLayouts.defered, 1);
    vkCmdEndRenderPass(m_offScreenCmdBuffer);
    EndDebugLabel(m_offScreenCmdBuffer);

    {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.image = m_framebuffers.deferred->attachments[4].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(m_offScreenCmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);
    }
    if (Settings.AOSetting.UseAO == 1)
    {
        // SSAO
        renderPassBeginInfo.renderPass = m_SSAOPass.frameBuffer->renderPass;
        renderPassBeginInfo.framebuffer = m_SSAOPass.frameBuffer->framebuffer;
        renderPassBeginInfo.clearValueCount = 1;
        BeginDebugLabel(m_offScreenCmdBuffer, "SSAO Pass", 0.5f, 0.5f, 0.0f);
        vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SSAOPass.pipeline);
        vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SSAOPass.pipelineLayout, 0, 1, &m_SSAOPass.set, 0, NULL);
        vkCmdDraw(m_offScreenCmdBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(m_offScreenCmdBuffer);
        EndDebugLabel(m_offScreenCmdBuffer);
    }

    if (Settings.AOSetting.UseAO == 2)
    {
        // HBAO
        renderPassBeginInfo.renderPass = m_HBAOPass.FrameBuffer->renderPass;
        renderPassBeginInfo.framebuffer = m_HBAOPass.FrameBuffer->framebuffer;
        renderPassBeginInfo.clearValueCount = 1;
        BeginDebugLabel(m_offScreenCmdBuffer, "HBAO Pass", 0.0f, 1.0f, 1.0f);
        vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_HBAOPass.Pipeline);
        vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_HBAOPass.PipelineLayout, 0, 1, &m_HBAOPass.Set, 0, NULL);
        vkCmdDraw(m_offScreenCmdBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(m_offScreenCmdBuffer);
        EndDebugLabel(m_offScreenCmdBuffer);
    }

    if (Settings.AOSetting.UseAO == 3)
    {
        // GTAO
        renderPassBeginInfo.renderPass = m_GTAOPass.FrameBuffer->renderPass;
        renderPassBeginInfo.framebuffer = m_GTAOPass.FrameBuffer->framebuffer;
        renderPassBeginInfo.clearValueCount = 1;
        BeginDebugLabel(m_offScreenCmdBuffer, "GTAO Pass", 1.0f, 0.412f, 0.706f);
        vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GTAOPass.Pipeline);
        vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GTAOPass.PipelineLayout, 0, 1, &m_GTAOPass.Set, 0, NULL);
        vkCmdDraw(m_offScreenCmdBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(m_offScreenCmdBuffer);
        EndDebugLabel(m_offScreenCmdBuffer);
    }
    if (Settings.AOSetting.UseAO == 4)
    {
        // SdfAO
        renderPassBeginInfo.renderPass = sdfAOPass_.frameBuffer->renderPass;
        renderPassBeginInfo.framebuffer = sdfAOPass_.frameBuffer->framebuffer;
        renderPassBeginInfo.clearValueCount = 1;
        BeginDebugLabel(m_offScreenCmdBuffer, "Sdf AO Pass", 0.5f, 0.5f, 0.0f);
        vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sdfAOPass_.pipeline);
        vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sdfAOPass_.pipelineLayout, 0, 1, &sdfAOPass_.set, 0, NULL);
        vkCmdDraw(m_offScreenCmdBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(m_offScreenCmdBuffer);
        EndDebugLabel(m_offScreenCmdBuffer);
    }
    if (Settings.AOSetting.UseAO != 0 && Settings.AOSetting.UseCBFBlur == true)
    {
        // CrossBilateralFilterPass
        // X
        BeginDebugLabel(m_offScreenCmdBuffer, "CrossBilateralFilter Pass", 0.40f, 0.6f, 0.8f);
        renderPassBeginInfo.renderPass = m_CBFPass.FrameBufferX->renderPass;
        renderPassBeginInfo.framebuffer = m_CBFPass.FrameBufferX->framebuffer;
        renderPassBeginInfo.clearValueCount = 1;
        vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CBFPass.PipelineX);
        vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CBFPass.PipelineLayout, 0, 1, &m_CBFPass.SetX, 0, NULL);
        vkCmdDraw(m_offScreenCmdBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(m_offScreenCmdBuffer);
        // Y
        renderPassBeginInfo.renderPass = m_CBFPass.FrameBufferY->renderPass;
        renderPassBeginInfo.framebuffer = m_CBFPass.FrameBufferY->framebuffer;
        renderPassBeginInfo.clearValueCount = 1;
        vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CBFPass.PipelineY);
        vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CBFPass.PipelineLayout, 0, 1, &m_CBFPass.SetY, 0, NULL);
        vkCmdDraw(m_offScreenCmdBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(m_offScreenCmdBuffer);
        EndDebugLabel(m_offScreenCmdBuffer);
    }

    // lighting pass
    renderPassBeginInfo.renderPass = m_framebuffers.lighting->renderPass;
    renderPassBeginInfo.framebuffer = m_framebuffers.lighting->framebuffer;
    renderPassBeginInfo.clearValueCount = 2;
    BeginDebugLabel(m_offScreenCmdBuffer, "Lighting Pass", 0.0f, 0.0f, 1.0f);
    vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    viewport = Init::viewport((float)m_framebuffers.lighting->width, (float)m_framebuffers.lighting->height, 0.0f, 1.0f);
    scissor = Init::rect2D(m_framebuffers.lighting->width, m_framebuffers.lighting->height, 0, 0);
    vkCmdSetViewport(m_offScreenCmdBuffer, 0, 1, &viewport);
    vkCmdSetScissor(m_offScreenCmdBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.composition);
    vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.composition, 0, 1, &m_descriptorSets.composition,
                            0, NULL);
    vkCmdDraw(m_offScreenCmdBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(m_offScreenCmdBuffer);
    EndDebugLabel(m_offScreenCmdBuffer);

    // skybox
    renderPassBeginInfo.renderPass = m_framebuffers.SkyBox->renderPass;
    renderPassBeginInfo.framebuffer = m_framebuffers.SkyBox->framebuffer;
    renderPassBeginInfo.clearValueCount = 2;
    BeginDebugLabel(m_offScreenCmdBuffer, "SkyBox Pass", 1.0f, 0.0f, 0.0f);
    vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.skyBox);
    vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.skyBox, 0, 1, &m_descriptorSets.skyBox, 0, NULL);
    scene.skybox.Draw(m_offScreenCmdBuffer);
    vkCmdEndRenderPass(m_offScreenCmdBuffer);
    EndDebugLabel(m_offScreenCmdBuffer);

    if (m_postSettings.bloom)
    {
        // bloom 0
        renderPassBeginInfo.renderPass = m_framebuffers.bloom->renderPass;
        renderPassBeginInfo.framebuffer = m_framebuffers.bloom->framebuffer;
        renderPassBeginInfo.clearValueCount = 1;
        BeginDebugLabel(m_offScreenCmdBuffer, "Bloom Passes", 0.0f, 1.0f, 0.0f);
        vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        viewport = Init::viewport((float)m_framebuffers.lighting->width, (float)m_framebuffers.lighting->height, 0.0f, 1.0f);
        scissor = Init::rect2D(m_framebuffers.lighting->width, m_framebuffers.lighting->height, 0, 0);
        vkCmdSetViewport(m_offScreenCmdBuffer, 0, 1, &viewport);
        vkCmdSetScissor(m_offScreenCmdBuffer, 0, 1, &scissor);
        vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.blurVert);
        vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.blur, 0, 1, &m_descriptorSets.blurVert, 0,
                                NULL);
        vkCmdDraw(m_offScreenCmdBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(m_offScreenCmdBuffer);

        // bloom 1
        renderPassBeginInfo.renderPass = m_framebuffers.bloom1->renderPass;
        renderPassBeginInfo.framebuffer = m_framebuffers.bloom1->framebuffer;
        renderPassBeginInfo.clearValueCount = 1;
        vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        viewport = Init::viewport((float)m_framebuffers.lighting->width, (float)m_framebuffers.lighting->height, 0.0f, 1.0f);
        scissor = Init::rect2D(m_framebuffers.lighting->width, m_framebuffers.lighting->height, 0, 0);
        vkCmdSetViewport(m_offScreenCmdBuffer, 0, 1, &viewport);
        vkCmdSetScissor(m_offScreenCmdBuffer, 0, 1, &scissor);
        vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.blurHorz);
        vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.blur, 0, 1, &m_descriptorSets.blurHorz, 0,
                                NULL);
        vkCmdDraw(m_offScreenCmdBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(m_offScreenCmdBuffer);
        EndDebugLabel(m_offScreenCmdBuffer);
    }

    // ToneMapping pass
    renderPassBeginInfo.renderPass = m_framebuffers.ToneMapping->renderPass;
    renderPassBeginInfo.framebuffer = m_framebuffers.ToneMapping->framebuffer;
    renderPassBeginInfo.clearValueCount = 1;
    BeginDebugLabel(m_offScreenCmdBuffer, "ToneMapping Pass", 0.0f, 0.0f, 1.0f);
    vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    viewport = Init::viewport((float)m_framebuffers.ToneMapping->width, (float)m_framebuffers.ToneMapping->height, 0.0f, 1.0f);
    scissor = Init::rect2D(m_framebuffers.ToneMapping->width, m_framebuffers.ToneMapping->height, 0, 0);
    vkCmdSetViewport(m_offScreenCmdBuffer, 0, 1, &viewport);
    vkCmdSetScissor(m_offScreenCmdBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.toneMapping);
    vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.toneMapping, 0, 1, &m_descriptorSets.toneMapping,
                            0, NULL);
    vkCmdDraw(m_offScreenCmdBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(m_offScreenCmdBuffer);
    EndDebugLabel(m_offScreenCmdBuffer);

    // Release barrier
    if (m_index.graphics != m_index.compute)
    {
        std::vector<VkBufferMemoryBarrier> bufferBarriers;

        VkBufferMemoryBarrier bufferBarrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_READ_BIT,   0, m_index.graphics, m_index.compute,
            m_compute.buffers.tiles.buffer,          0,       m_compute.buffers.tiles.size};
        bufferBarriers.push_back(bufferBarrier);
        bufferBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                         nullptr,
                         VK_ACCESS_SHADER_READ_BIT,
                         0,
                         m_index.graphics,
                         m_index.compute,
                         PointLightBuffer.buffer,
                         0,
                         PointLightBuffer.size};
        vkCmdPipelineBarrier(m_offScreenCmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr,
                             bufferBarriers.size(), bufferBarriers.data(), 0, nullptr);
    }
    Tool::CheckResult(vkEndCommandBuffer(m_offScreenCmdBuffer));
}

void Renderer::SetupShadow()
{
    m_framebuffers.shadow = new FramebufferManager(m_vulkanDevice);

    m_framebuffers.shadow->width = m_width;
    m_framebuffers.shadow->height = m_height;

    // Find a suitable depth format
    VkFormat shadowMapFormat;
    VkBool32 validShadowMapFormat = Tool::GetSupportedDepthFormat(m_physicalDevice, &shadowMapFormat);
    assert(validShadowMapFormat);

    // Create a layered depth attachment for rendering the depth maps from the lights' point of view
    // Each layer corresponds to one of the lights
    // The actual output to the separate layers is done in the geometry shader using shader instancing
    // We will pass the matrices of the lights to the GS that selects the layer by the current invocation
    AttachmentCreateInfo attachmentInfo = {};
    attachmentInfo.format = shadowMapFormat;
    attachmentInfo.width = m_framebuffers.shadow->width;
    attachmentInfo.height = m_framebuffers.shadow->height;
    attachmentInfo.layerCount = LIGHT_COUNT;
    attachmentInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    m_framebuffers.shadow->AddAttachment(attachmentInfo);

    // Create sampler to sample from to depth attachment
    // Used to sample in the fragment shader for shadowed rendering
    Tool::CheckResult(m_framebuffers.shadow->CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));

    // Create default renderpass for the framebuffer
    Tool::CheckResult(m_framebuffers.shadow->CreateRenderPass());
};
void Renderer::SetupLightingPass()
{
    m_framebuffers.lighting = new FramebufferManager(m_vulkanDevice);
    m_framebuffers.lighting->width = m_width;
    m_framebuffers.lighting->height = m_height;

    AttachmentCreateInfo attachmentCI{};
    attachmentCI.width = m_framebuffers.lighting->width;
    attachmentCI.height = m_framebuffers.lighting->height;
    attachmentCI.layerCount = 1;
    attachmentCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // lighting RT
    attachmentCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_framebuffers.lighting->AddAttachment(attachmentCI);

    // Create sampler to sample from the color attachments
    Tool::CheckResult(m_framebuffers.lighting->CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    Tool::CheckResult(m_framebuffers.lighting->CreateRenderPass());
}

void Renderer::SetupSkyBoxPass()
{

    m_framebuffers.SkyBox = new FramebufferManager(m_vulkanDevice);
    m_framebuffers.SkyBox->width = m_width;
    m_framebuffers.SkyBox->height = m_height;

    AttachmentCreateInfo attachmentCI{};
    attachmentCI.width = m_framebuffers.SkyBox->width;
    attachmentCI.height = m_framebuffers.SkyBox->height;
    attachmentCI.layerCount = 1;
    attachmentCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // lighting RT
    attachmentCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_framebuffers.SkyBox->AddAttachment(attachmentCI);

    // extract highlight
    m_framebuffers.SkyBox->AddAttachment(attachmentCI);

    // Create sampler to sample from the color attachments
    Tool::CheckResult(m_framebuffers.SkyBox->CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    Tool::CheckResult(m_framebuffers.SkyBox->CreateRenderPass());
}
void Renderer::SetupToneMappingPass()
{
    m_framebuffers.ToneMapping = new FramebufferManager(m_vulkanDevice);
    m_framebuffers.ToneMapping->width = m_width;
    m_framebuffers.ToneMapping->height = m_height;

    AttachmentCreateInfo attachmentCI{};
    attachmentCI.width = m_framebuffers.ToneMapping->width;
    attachmentCI.height = m_framebuffers.ToneMapping->height;
    attachmentCI.layerCount = 1;
    attachmentCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // lighting RT
    attachmentCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_framebuffers.ToneMapping->AddAttachment(attachmentCI);

    // Create sampler to sample from the color attachments
    Tool::CheckResult(m_framebuffers.ToneMapping->CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    Tool::CheckResult(m_framebuffers.ToneMapping->CreateRenderPass());
}
void Renderer::SetupBloomPass()
{
    m_framebuffers.bloom = new FramebufferManager(m_vulkanDevice);
    m_framebuffers.bloom->width = m_width;
    m_framebuffers.bloom->height = m_height;

    AttachmentCreateInfo attachmentCI{};
    attachmentCI.width = m_framebuffers.bloom->width;
    attachmentCI.height = m_framebuffers.bloom->height;
    attachmentCI.layerCount = 1;
    attachmentCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // lighting RT
    attachmentCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_framebuffers.bloom->AddAttachment(attachmentCI);

    // Create sampler to sample from the color attachments
    Tool::CheckResult(m_framebuffers.bloom->CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    Tool::CheckResult(m_framebuffers.bloom->CreateRenderPass());

    m_framebuffers.bloom1 = new FramebufferManager(m_vulkanDevice);
    m_framebuffers.bloom1->width = m_width;
    m_framebuffers.bloom1->height = m_height;

    attachmentCI.width = m_framebuffers.bloom1->width;
    attachmentCI.height = m_framebuffers.bloom1->height;
    attachmentCI.layerCount = 1;
    attachmentCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // lighting RT
    attachmentCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_framebuffers.bloom1->AddAttachment(attachmentCI);

    attachmentCI.width = 1;
    attachmentCI.height = 1;
    attachmentCI.layerCount = 1;
    attachmentCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // lighting RT
    attachmentCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_framebuffers.bloom1->AddAttachment(attachmentCI);

    VkCommandBuffer layoutCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    Tool::SetImageLayout(layoutCmd, m_framebuffers.bloom1->defaultMaterials[0].image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_vulkanDevice->FlushCommandBuffer(layoutCmd, m_queues.graphicsQueue, true);

    // Create sampler to sample from the color attachments
    Tool::CheckResult(m_framebuffers.bloom1->CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    Tool::CheckResult(m_framebuffers.bloom1->CreateRenderPass());
}

void Renderer::SetupFinalPass()
{
    // attachment1.format = VK_FORMAT_R16G16B16A16_SFLOAT;

    // VkImageCreateInfo imageCI = Init::imageCreateInfo();
    // imageCI.imageType = VK_IMAGE_TYPE_2D;
    // imageCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    // imageCI.extent.width = m_width;
    // imageCI.extent.height = m_height;
    // imageCI.extent.depth = 1;
    // imageCI.mipLevels = 1;
    // imageCI.arrayLayers = 1;
    // imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    // imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    // imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    // VkMemoryAllocateInfo memAlloc = Init::memoryAllocateInfo();
    // VkMemoryRequirements memReqs;

    //// Create image for this attachment
    // Tool::CheckResult(vkCreateImage(m_vulkanDevice->logicalDevice, &imageCI, nullptr, &attachment1.image));
    // vkGetImageMemoryRequirements(m_vulkanDevice->logicalDevice, attachment1.image, &memReqs);
    // memAlloc.allocationSize = memReqs.size;
    // memAlloc.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    // Tool::CheckResult(vkAllocateMemory(m_vulkanDevice->logicalDevice, &memAlloc, nullptr, &attachment1.memory));
    // Tool::CheckResult(vkBindImageMemory(m_vulkanDevice->logicalDevice, attachment1.image, attachment1.memory, 0));

    // attachment1.subresourceRange = {};
    // attachment1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // attachment1.subresourceRange.levelCount = 1;
    // attachment1.subresourceRange.layerCount = 1;

    // VkImageViewCreateInfo imageViewCI = Init::imageViewCreateInfo();
    // imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    // imageViewCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    // imageViewCI.subresourceRange = attachment1.subresourceRange;
    // imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // imageViewCI.image = attachment1.image;
    // Tool::CheckResult(vkCreateImageView(m_vulkanDevice->logicalDevice, &imageViewCI, nullptr, &attachment1.view));

    //// Fill attachment description
    // attachment1.description = {};
    // attachment1.description.samples = VK_SAMPLE_COUNT_1_BIT;
    // attachment1.description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // attachment1.description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // attachment1.description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    // attachment1.description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // attachment1.description.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    // attachment1.description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // attachment1.description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription swapChainImage{};
    swapChainImage.format = m_swapChain.colorFormat;
    swapChainImage.samples = VK_SAMPLE_COUNT_1_BIT;
    swapChainImage.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    swapChainImage.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    swapChainImage.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    swapChainImage.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    swapChainImage.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapChainImage.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    std::vector<VkAttachmentDescription> attachmentDescriptions;
    attachmentDescriptions.push_back(swapChainImage);
    /*attachmentDescriptions.push_back(attachment1.description);*/

    // color Reference
    std::vector<VkAttachmentReference> colorReferences;
    colorReferences.push_back({0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    // colorReferences.push_back({ 1,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pColorAttachments = colorReferences.data();
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());

    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create render pass
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pAttachments = attachmentDescriptions.data();
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies.data();

    Tool::CheckResult(vkCreateRenderPass(m_vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &m_finalPass));

    // framebuffer
    m_finalFramebuffers.resize(m_swapChain.images.size());
    for (uint32_t i = 0; i < m_finalFramebuffers.size(); ++i)
    {
        std::vector<VkImageView> attachmentViews = {m_swapChain.imageViews[i]};
        VkFramebufferCreateInfo frameBufferCI{};
        frameBufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferCI.renderPass = m_finalPass;
        frameBufferCI.pAttachments = attachmentViews.data();
        frameBufferCI.layers = 1;
        frameBufferCI.height = m_height;
        frameBufferCI.width = m_width;
        frameBufferCI.attachmentCount = 1;
        Tool::CheckResult(vkCreateFramebuffer(m_vulkanDevice->logicalDevice, &frameBufferCI, nullptr, &m_finalFramebuffers[i]));
    }
}
void Renderer::SetupDeferedPass()
{
    m_framebuffers.deferred = new FramebufferManager(m_vulkanDevice);
    m_framebuffers.deferred->width = m_width;
    m_framebuffers.deferred->height = m_height;

    AttachmentCreateInfo attachmentInfo = {};
    attachmentInfo.width = m_framebuffers.deferred->width;
    attachmentInfo.height = m_framebuffers.deferred->height;
    attachmentInfo.layerCount = 1;
    attachmentInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // Color attachments
    // Attachment 0: (World space) Positions
    attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_framebuffers.deferred->AddAttachment(attachmentInfo);

    // Attachment 1: (World space) Normals
    attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_framebuffers.deferred->AddAttachment(attachmentInfo);

    // Attachment 2: Albedo (color)
    attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    m_framebuffers.deferred->AddAttachment(attachmentInfo);

    // Attachment 3: metallic Roughness AO ? (color)
    attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    m_framebuffers.deferred->AddAttachment(attachmentInfo);

    // Depth attachment
    // Find a suitable depth format
    VkFormat attDepthFormat;
    VkBool32 validDepthFormat = Tool::GetSupportedDepthFormat(m_physicalDevice, &attDepthFormat);
    assert(validDepthFormat);

    attachmentInfo.format = attDepthFormat;
    attachmentInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    m_framebuffers.deferred->AddAttachment(attachmentInfo);

    // Create sampler to sample from the color attachments
    Tool::CheckResult(m_framebuffers.deferred->CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));

    // Create default renderpass for the framebuffer
    Tool::CheckResult(m_framebuffers.deferred->CreateRenderPass());
};

void Renderer::CreateBuffersDirLights()
{
    {
        VkDeviceSize bufferSize{m_lights.DirLights.size() * sizeof(DirectionalLight)};

        Buffer stagingBuffer;

        m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     &stagingBuffer, bufferSize, m_lights.DirLights.data());

        m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     &DirLightBuffer, bufferSize);

        VkCommandBuffer copyCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VkBufferCopy copyRegion{};
        copyRegion.size = bufferSize;
        vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, DirLightBuffer.buffer, 1, &copyRegion);
        DirLightBuffer.SetupDescriptor();
        m_vulkanDevice->FlushCommandBuffer(copyCmd, m_queues.graphicsQueue, true);

        stagingBuffer.Destroy();
    }
    {

        for (auto& light : m_lights.DirLights)
        {
            if (light.castShadow == 1)
            {
                m_shadowDirLightCount += 1;
            }
        }
        m_CSMPass.Cascades.resize(m_shadowDirLightCount);
        m_CSMPass.CascadeData.resize(m_shadowDirLightCount);
        VkDeviceSize bufferSize{m_shadowDirLightCount * sizeof(CSMPass::CascadeDataDesc)};
        Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                       &m_CSMPass.buffers.cascadeViewProjMatricesBuffer, bufferSize));
        Tool::CheckResult(m_CSMPass.buffers.cascadeViewProjMatricesBuffer.Map());
        /*Buffer stagingBuffer;

        m_vulkanDevice->CreateBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingBuffer,
            bufferSize,
            m_dirLightsMatrices.data());

        m_vulkanDevice->CreateBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &DirLightMatricesBufer,
            bufferSize);

        VkCommandBuffer copyCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VkBufferCopy copyRegion{};
        copyRegion.size = bufferSize;
        vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, DirLightMatricesBufer.buffer, 1, &copyRegion);

        m_vulkanDevice->FlushCommandBuffer(copyCmd, m_queues.graphicsQueue, true);

        stagingBuffer.Destroy();*/
    }

    UpdateUBOCSM();
}
void Renderer::InitLights()
{
    m_lights.InitLights(1, true, 1, true, 4, false);

    /*m_lightingPass.CBufferData.lights[0] = InitLight(glm::vec3(10.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.9f, 0.9f));
    m_lightingPass.CBufferData.lights[1] = InitLight(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.9f, 1.0f, 0.9f));
    m_lightingPass.CBufferData.lights[2] = InitLight(glm::vec3(-10.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.9f, 0.9f, 1.0f));*/
};
void Renderer::CreateDescriptorPool()
{
    // Pool
    std::vector<VkDescriptorPoolSize> poolSizes = {Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 50),
                                                   Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100), // 增加到100个以支持更多阶段
                                                   Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 200),
                                                   Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 30), // 增加到30个
                                                   Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 30), // 增加到30个
                                                   Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 30)};      // 添加独立采样器支持
    VkDescriptorPoolCreateInfo descriptorPoolInfo = Init::descriptorPoolCreateInfo(poolSizes, 100);                // 增加最大描述符集数到100
    Tool::CheckResult(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool));
}
void Renderer::AllocateDescriptorSetLighting()
{
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        // Binding 0: Vertex shader uniform buffer
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
        // Binding 1: Position texture
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
        // Binding 2: Normals texture
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
        // Binding 3: Albedo texture
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
        // Binding 4: Fragment shader uniform buffer
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
        // Binding 5: Shadow map
        // Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
        // irradiance cube
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 6),
        // BrdfLut
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 7),
        // prefilter cube
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 8),
        //
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 9),
        // shadow cubemap
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 10),
        // tiles buffer
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 11),
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 12),
        // AO
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 13),
        // Camera Buffer
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 14),
        // Dir light buffer
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 15),
        // Point light buffer
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 16),
        // Spot light buffer
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 17),
        // dir light matrices
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 18),

    };
    if (m_shadowDirLightCount > 0)
    {
        // dir light cascades
        setLayoutBindings.push_back(
            Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 19, m_shadowDirLightCount));
    }
    if (m_shadowPointLightCount > 0)
    {
        setLayoutBindings.push_back(
            Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 20, m_shadowPointLightCount));
    }
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_descriptorSetLayouts.composition));

    std::vector<VkWriteDescriptorSet> writeDescriptorSets;
    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_descriptorSetLayouts.composition, 1);
    // Deferred composition
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.composition));

    // Image descriptors for the offscreen color attachments
    VkDescriptorImageInfo texDescriptorPosition = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorNormal = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[1].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorAlbedo = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[2].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorMRAO = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[3].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorLighting = Init::descriptorImageInfo(
        m_framebuffers.lighting->sampler, m_framebuffers.lighting->attachments[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorBloomEnd = Init::descriptorImageInfo(
        m_framebuffers.bloom1->sampler, m_framebuffers.bloom1->attachments[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // VkDescriptorImageInfo texDescriptorShadowMap =
    //	Init::descriptorImageInfo(
    //		m_framebuffers.shadow->sampler,
    //		m_framebuffers.shadow->attachments[0].view,
    //		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorSSAO = Init::descriptorImageInfo(m_SSAOPass.frameBuffer->sampler, m_SSAOPass.frameBuffer->attachments[0].view,
                                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorHBAO = Init::descriptorImageInfo(m_HBAOPass.FrameBuffer->sampler, m_HBAOPass.FrameBuffer->attachments[0].view,
                                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorGTAO = Init::descriptorImageInfo(m_GTAOPass.FrameBuffer->sampler, m_GTAOPass.FrameBuffer->attachments[0].view,
                                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorCBF = Init::descriptorImageInfo(m_CBFPass.FrameBufferY->sampler, m_CBFPass.FrameBufferY->attachments[0].view,
                                                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkWriteDescriptorSet writeAO;
    if (Settings.AOSetting.UseAO == 0)
    {
        writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13,
                                           &m_defaultTextures.White.descriptor);
    }
    else if (Settings.AOSetting.UseCBFBlur == false)
    {
        switch (Settings.AOSetting.UseAO)
        {

        case 1: {
            writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13, &texDescriptorSSAO);
            break;
        }
        case 2: {
            writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13, &texDescriptorHBAO);
            break;
        }
        case 3: {
            writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13, &texDescriptorGTAO);
            break;
        }
        default: {
            writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13,
                                               &m_defaultTextures.White.descriptor);
            break;
        }
        }
    }
    else
    {
        writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13, &texDescriptorCBF);
    }

    writeDescriptorSets = {
        // Binding 1: World space position texture
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorPosition),
        // Binding 2: World space normals texture
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorNormal),
        // Binding 3: Albedo texture
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texDescriptorAlbedo),
        // Binding 4: Fragment shader uniform buffer
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4, &m_lightingPass.Buffers.ConstBuffer.descriptor),
        // Binding 5: Shadow map
        // Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, &texDescriptorShadowMap),
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6,
                                 &scene.textures.irradianceCube.descriptor),
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 7, &scene.textures.lutBrdf.descriptor),
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8,
                                 &scene.textures.prefilteredCube.descriptor),
        // Metallic Roughness A Occlusion
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 9, &texDescriptorMRAO),
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 11, &m_compute.buffers.tiles.descriptor),
        // Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 12, &m_compute.buffers.lights.descriptor),
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 14, &m_sharedBuffers.ConstBufferCamera.descriptor),
        writeAO,
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 15, &DirLightBuffer.descriptor),
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16, &PointLightBuffer.descriptor),
        Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 17, &SpotLightBuffer.descriptor),
    };
    std::vector<VkDescriptorImageInfo> shadowDirImageInfos;
    if (m_shadowDirLightCount > 0)
    {
        writeDescriptorSets.push_back(Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 18,
                                                               &m_CSMPass.buffers.cascadeViewProjMatricesBuffer.descriptor));

        for (int i = 0; i < m_shadowDirLightCount; ++i)
        {
            shadowDirImageInfos.emplace_back(
                VkDescriptorImageInfo{m_CSMPass.Depths[i].sampler, m_CSMPass.Depths[i].view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL});
        }
        VkWriteDescriptorSet shadowMapWrite{};
        shadowMapWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        shadowMapWrite.dstSet = m_descriptorSets.composition;
        shadowMapWrite.dstBinding = 19;
        shadowMapWrite.descriptorCount = m_shadowDirLightCount;
        shadowMapWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        shadowMapWrite.pImageInfo = shadowDirImageInfos.data();
        writeDescriptorSets.push_back(shadowMapWrite);
    }
    std::vector<VkDescriptorImageInfo> shadowImageInfos;
    if (m_shadowPointLightCount > 0)
    {

        for (int i = 0; i < m_shadowPointLightCount; ++i)
        {
            shadowImageInfos.emplace_back(
                VkDescriptorImageInfo{m_shadowOmniPass.Sampler, m_shadowOmniPass.CubeMaps[i].Tex.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        }
        VkWriteDescriptorSet shadowMapWrite{};
        shadowMapWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        shadowMapWrite.dstSet = m_descriptorSets.composition;
        shadowMapWrite.dstBinding = 20;
        shadowMapWrite.descriptorCount = m_shadowPointLightCount;
        shadowMapWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        shadowMapWrite.pImageInfo = shadowImageInfos.data();
        writeDescriptorSets.push_back(shadowMapWrite);
    }
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
void Renderer::UpdateDescritporSetLighting()
{
    VkDescriptorImageInfo texDescriptorSSAO = Init::descriptorImageInfo(m_SSAOPass.frameBuffer->sampler, m_SSAOPass.frameBuffer->attachments[0].view,
                                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorHBAO = Init::descriptorImageInfo(m_HBAOPass.FrameBuffer->sampler, m_HBAOPass.FrameBuffer->attachments[0].view,
                                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorGTAO = Init::descriptorImageInfo(m_GTAOPass.FrameBuffer->sampler, m_GTAOPass.FrameBuffer->attachments[0].view,
                                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo texDescriptorSdfAO = Init::descriptorImageInfo(sdfAOPass_.frameBuffer->sampler, sdfAOPass_.frameBuffer->attachments[0].view,
                                                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorCBF = Init::descriptorImageInfo(m_CBFPass.FrameBufferY->sampler, m_CBFPass.FrameBufferY->attachments[0].view,
                                                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkWriteDescriptorSet writeAO;
    if (Settings.AOSetting.UseAO == 0)
    {
        writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13,
                                           &m_defaultTextures.White.descriptor);
    }
    else if (Settings.AOSetting.UseCBFBlur == false)
    {
        switch (Settings.AOSetting.UseAO)
        {

        case 1: {
            writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13, &texDescriptorSSAO);
            break;
        }
        case 2: {
            writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13, &texDescriptorHBAO);
            break;
        }
        case 3: {
            writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13, &texDescriptorGTAO);
            break;
        }
        case 4: {
            writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13, &texDescriptorSdfAO);
            break;
        }
        default: {
            writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13,
                                               &m_defaultTextures.White.descriptor);
            break;
        }
        }
    }
    else
    {
        writeAO = Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 13, &texDescriptorCBF);
    }

    vkUpdateDescriptorSets(m_device, 1, &writeAO, 0, nullptr);
}
void Renderer::AllocateDescriptorSetSpotLightShadow()
{
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_spotLightPass.SetLayout));

    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_spotLightPass.SetLayout, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_spotLightPass.Set));
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        // Binding 0: Vertex shader uniform buffer
        Init::writeDescriptorSet(m_spotLightPass.Set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_spotLightPass.CBuffer.descriptor),
    };
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
void Renderer::SetupDescriptors()
{

    // Layout
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    // deferred model
    setLayoutBindings = {
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
    };
    descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_descriptorSetLayouts.deferedModel));
    // deferedTextures
    setLayoutBindings = {Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
                         Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
                         Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2)};
    descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_descriptorSetLayouts.deferedTextures));

    // FXAA
    setLayoutBindings = {// Binding 0: Vertex shader uniform buffer
                         Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
                         // Binding 1: Position texture
                         Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)};
    descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_descriptorSetLayouts.FXAA));

    setLayoutBindings = {// Binding 0: Param
                         Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
                         // Binding 1: lighting
                         Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
                         // high
                         Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2)};
    descriptorLayoutCI.pBindings = setLayoutBindings.data();
    descriptorLayoutCI.bindingCount = 3;
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_descriptorSetLayouts.toneMapping));

    // Sets
    std::vector<VkWriteDescriptorSet> writeDescriptorSets;
    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_descriptorSetLayouts.composition, 1);
    // Deferred composition

    // Image descriptors for the offscreen color attachments
    VkDescriptorImageInfo texDescriptorPosition = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorNormal = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[1].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorAlbedo = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[2].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorMRAO = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[3].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorLighting = Init::descriptorImageInfo(
        m_framebuffers.lighting->sampler, m_framebuffers.lighting->attachments[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorBloomEnd = Init::descriptorImageInfo(
        m_framebuffers.bloom1->sampler, m_framebuffers.bloom1->attachments[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // VkDescriptorImageInfo texDescriptorShadowMap =
    //	Init::descriptorImageInfo(
    //		m_framebuffers.shadow->sampler,
    //		m_framebuffers.shadow->attachments[0].view,
    //		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorToneMapping = Init::descriptorImageInfo(
        m_framebuffers.ToneMapping->sampler, m_framebuffers.ToneMapping->attachments[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorSkyBox = Init::descriptorImageInfo(m_framebuffers.SkyBox->sampler, m_framebuffers.SkyBox->attachments[0].view,
                                                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorSSAO = Init::descriptorImageInfo(m_SSAOPass.frameBuffer->sampler, m_SSAOPass.frameBuffer->attachments[0].view,
                                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorHBAO = Init::descriptorImageInfo(m_HBAOPass.FrameBuffer->sampler, m_HBAOPass.FrameBuffer->attachments[0].view,
                                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorGTAO = Init::descriptorImageInfo(m_GTAOPass.FrameBuffer->sampler, m_GTAOPass.FrameBuffer->attachments[0].view,
                                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // FXAA
    allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_descriptorSetLayouts.FXAA, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.FXAA));
    writeDescriptorSets = {Init::writeDescriptorSet(m_descriptorSets.FXAA, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_uniformBuffers.FXAA.descriptor),
                           Init::writeDescriptorSet(m_descriptorSets.FXAA, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorToneMapping)};
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    // offscreen
    allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_descriptorSetLayouts.deferedModel, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.deferedModel));

    writeDescriptorSets = {
        // Binding 0: Vertex shader uniform buffer
        Init::writeDescriptorSet(m_descriptorSets.deferedModel, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_uniformBuffers.defered.descriptor)};
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    // post
    allocInfo.pSetLayouts = &m_descriptorSetLayouts.toneMapping;
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.toneMapping));
    writeDescriptorSets = {
        Init::writeDescriptorSet(m_descriptorSets.toneMapping, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_uniformBuffers.postParam.descriptor),
        Init::writeDescriptorSet(m_descriptorSets.toneMapping, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorSkyBox),
        Init::writeDescriptorSet(m_descriptorSets.toneMapping, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorBloomEnd),
    };
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

    SetupBlurDescriptorSets();
}

void Renderer::SetupBlurDescriptorSets()
{
    if (m_pipelineLayouts.composition != nullptr)
    {
        vkDestroyPipelineLayout(m_device, m_pipelineLayouts.skyBox, nullptr);
    }
    if (m_pipelines.composition != nullptr)
    {
        vkDestroyPipeline(m_device, m_pipelines.skyBox, nullptr);
    }
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;

    setLayoutBindings = {
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT,
                                         0), // Binding 0: Fragment shader uniform buffer
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
                                         1) // Binding 1: Fragment shader image sampler
    };
    descriptorSetLayoutCreateInfo = Init::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayouts.blur));

    VkDescriptorImageInfo texDescriptorHighLight = Init::descriptorImageInfo(
        m_framebuffers.SkyBox->sampler, m_framebuffers.SkyBox->attachments[1].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorVert =
        Init::descriptorImageInfo(m_framebuffers.bloom->sampler, m_framebuffers.bloom->attachments[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // Sets
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo;
    std::vector<VkWriteDescriptorSet> writeDescriptorSets;

    // bloom vertical
    descriptorSetAllocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_descriptorSetLayouts.blur, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &descriptorSetAllocInfo, &m_descriptorSets.blurVert));
    writeDescriptorSets = {
        Init::writeDescriptorSet(m_descriptorSets.blurVert, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0,
                                 &m_uniformBuffers.blurParams.descriptor), // Binding 0: Fragment shader uniform buffer
        Init::writeDescriptorSet(m_descriptorSets.blurVert, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                 &texDescriptorHighLight), // Binding 1: Fragment shader texture sampler
    };
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    // bloom1 horizental
    descriptorSetAllocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_descriptorSetLayouts.blur, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &descriptorSetAllocInfo, &m_descriptorSets.blurHorz));
    writeDescriptorSets = {
        Init::writeDescriptorSet(m_descriptorSets.blurHorz, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0,
                                 &m_uniformBuffers.blurParams.descriptor), // Binding 0: Fragment shader uniform buffer
        Init::writeDescriptorSet(m_descriptorSets.blurHorz, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                 &texDescriptorVert), // Binding 1: Fragment shader texture sampler
    };
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
void Renderer::PreparePipelineSkyBox()
{
    // Layout skybox
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&m_descriptorSetLayouts.skyBox, 1);
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.skyBox));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    std::array<VkPipelineColorBlendAttachmentState, 2> blendAttachmentStates = {Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
                                                                                Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE)};
    VkPipelineColorBlendStateCreateInfo colorBlendState =
        Init::pipelineColorBlendStateCreateInfo(blendAttachmentStates.size(), blendAttachmentStates.data());
    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "skybox/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "skybox/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_pipelineLayouts.skyBox, m_framebuffers.SkyBox->renderPass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position});

    uint32_t useSkyBox = Settings.PBRSetting.SkyBoxIndex == 0 ? 0 : 1;
    VkSpecializationMapEntry specializationMapEntry = Init::specializationMapEntry(0, 0, sizeof(uint32_t));
    VkSpecializationInfo specializationInfo = Init::specializationInfo(1, &specializationMapEntry, sizeof(uint32_t), &useSkyBox);
    shaderStages[1].pSpecializationInfo = &specializationInfo;

    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_pipelines.skyBox));
}
void Renderer::PreparePilineSpotLightShadow()
{
    // VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&m_descriptorSetLayouts.deferedModel, 1);
    // Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.shadow));

    // VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0,
    // VK_FALSE); VkPipelineRasterizationStateCreateInfo rasterizationState = Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL,
    // VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0); std::array<VkPipelineColorBlendAttachmentState, 2> lightingBlendAttachmentStates =
    // {
    //	Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE)
    // };

    // VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, lightingBlendAttachmentStates.data());
    // VkPipelineDepthStencilStateCreateInfo depthStencilState = Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE,
    // VK_COMPARE_OP_LESS_OR_EQUAL); VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1, 0);
    // VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    // std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    // VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    // std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    // VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_pipelineLayouts.composition, m_framebuffers.lighting->renderPass);
    // pipelineCI.pInputAssemblyState = &inputAssemblyState;
    // pipelineCI.pRasterizationState = &rasterizationState;
    // pipelineCI.pColorBlendState = &colorBlendState;
    // pipelineCI.pMultisampleState = &multisampleState;
    // pipelineCI.pViewportState = &viewportState;
    // pipelineCI.pDepthStencilState = &depthStencilState;
    // pipelineCI.pDynamicState = &dynamicState;
    // pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    // pipelineCI.pStages = shaderStages.data();

    //// Shadow mapping pipeline
    //	// The shadow mapping pipeline uses geometry shader instancing (invocations layout modifier) to output
    //	// shadow maps for multiple lights sources into the different shadow map layers in one single render pass
    // depthStencilState.depthWriteEnable = VK_TRUE;
    // depthStencilState.depthTestEnable = VK_TRUE;
    // pipelineCI.layout = m_pipelineLayouts.defered;

    // std::array<VkPipelineShaderStageCreateInfo, 2> shadowStages;
    // shadowStages[0] = LoadShader(Tool::GetShadersPath() + "Deferedshadows/Shadow.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    // shadowStages[1] = LoadShader(Tool::GetShadersPath() + "Deferedshadows/Shadow.Geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    // uint32_t lightCount = 3;
    // VkSpecializationMapEntry specializationMapEntry = Init::specializationMapEntry(0, 0, sizeof(uint32_t));
    // VkSpecializationInfo specializationInfo = Init::specializationInfo(1, &specializationMapEntry, sizeof(uint32_t), &lightCount);
    // shaderStages[1].pSpecializationInfo = &specializationInfo;

    // pipelineCI.pStages = shadowStages.data();
    // pipelineCI.stageCount = static_cast<uint32_t>(shadowStages.size());

    //// Shadow pass doesn't use any color attachments
    // colorBlendState.attachmentCount = 0;
    // colorBlendState.pAttachments = nullptr;
    //// Cull front faces
    // rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
    // depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    //// Enable depth bias
    // rasterizationState.depthBiasEnable = VK_TRUE;
    //// Add depth bias to dynamic state, so we can change it at runtime
    // dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    // dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    //// Reset blend attachment state
    // pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position });
    // pipelineCI.renderPass = m_framebuffers.shadow->renderPass;
    // Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_pipelines.shadow));
}
void Renderer::PreparePipelines()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&m_descriptorSetLayouts.composition, 1);
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;

    // model
    std::array<VkDescriptorSetLayout, 2> setLayouts = {m_descriptorSetLayouts.deferedModel, m_descriptorSetLayouts.deferedTextures};
    pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(setLayouts.data(), 2);
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.defered));

    // post
    pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&m_descriptorSetLayouts.toneMapping, 1);
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.toneMapping));

    // bloom
    pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&m_descriptorSetLayouts.blur, 1);
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.blur));

    pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&m_descriptorSetLayouts.FXAA, 1);
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.FXAA));
    // Pipelines

    // bloom
    {
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI =
            Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
        VkPipelineRasterizationStateCreateInfo rasterizationStateCI =
            Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
        VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
        VkPipelineColorBlendStateCreateInfo colorBlendStateCI = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
        VkPipelineDepthStencilStateCreateInfo depthStencilStateCI =
            Init::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
        VkPipelineViewportStateCreateInfo viewportStateCI = Init::pipelineViewportStateCreateInfo(1, 1, 0);
        VkPipelineMultisampleStateCreateInfo multisampleStateCI = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
        std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicStateCI = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

        VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_pipelineLayouts.blur, m_framebuffers.bloom->renderPass, 0);
        pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
        pipelineCI.pRasterizationState = &rasterizationStateCI;
        pipelineCI.pColorBlendState = &colorBlendStateCI;
        pipelineCI.pMultisampleState = &multisampleStateCI;
        pipelineCI.pViewportState = &viewportStateCI;
        pipelineCI.pDepthStencilState = &depthStencilStateCI;
        pipelineCI.pDynamicState = &dynamicStateCI;
        pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCI.pStages = shaderStages.data();

        // Blur pipelines
        shaderStages[0] = LoadShader(Tool::GetShadersPath() + "post/GaussBlur.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = LoadShader(Tool::GetShadersPath() + "post/GaussBlur.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        // Empty vertex input state
        VkPipelineVertexInputStateCreateInfo emptyInputState = Init::pipelineVertexInputStateCreateInfo();
        pipelineCI.pVertexInputState = &emptyInputState;
        // Additive blending
        blendAttachmentState.colorWriteMask = 0xF;
        blendAttachmentState.blendEnable = VK_TRUE;
        blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
        blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;

        // Use specialization constants to select between horizontal and vertical blur
        uint32_t blurdirection = 0;
        VkSpecializationMapEntry specializationMapEntry = Init::specializationMapEntry(0, 0, sizeof(uint32_t));
        VkSpecializationInfo specializationInfo = Init::specializationInfo(1, &specializationMapEntry, sizeof(uint32_t), &blurdirection);
        shaderStages[1].pSpecializationInfo = &specializationInfo;
        // Vertical blur pipeline
        Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_pipelines.blurVert));

        blurdirection = 1;
        pipelineCI.renderPass = m_framebuffers.bloom1->renderPass;
        Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_pipelines.blurHorz));
    }

    // light composition
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    std::array<VkPipelineColorBlendAttachmentState, 2> lightingBlendAttachmentStates = {Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE)};

    VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, lightingBlendAttachmentStates.data());
    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_pipelineLayouts.composition, m_framebuffers.lighting->renderPass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();

    // Final fullscreen composition pass pipeline
    rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;

    // Empty vertex input state, vertices are generated by the vertex shader
    VkPipelineVertexInputStateCreateInfo emptyInputState = Init::pipelineVertexInputStateCreateInfo();
    pipelineCI.pVertexInputState = &emptyInputState;

    // Tonemapping pipeline
    colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, lightingBlendAttachmentStates.data());
    pipelineCI.layout = m_pipelineLayouts.toneMapping;
    pipelineCI.renderPass = m_framebuffers.ToneMapping->renderPass;
    depthStencilState.depthWriteEnable = VK_FALSE;
    depthStencilState.depthTestEnable = VK_FALSE;
    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Post/ToneMapping.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Post/ToneMapping.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_pipelines.toneMapping));

    colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, lightingBlendAttachmentStates.data());
    pipelineCI.layout = m_pipelineLayouts.FXAA;
    pipelineCI.renderPass = m_finalPass;
    depthStencilState.depthWriteEnable = VK_FALSE;
    depthStencilState.depthTestEnable = VK_FALSE;
    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Post/ToneMapping.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Post/FXAA.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_pipelines.FXAA));

    // Offscreen pipeline
    std::array<VkPipelineColorBlendAttachmentState, 4> blendAttachmentStates = {
        Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE), Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
        Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE), Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE)};
    colorBlendState.pAttachments = blendAttachmentStates.data();
    // defered model
    colorBlendState.attachmentCount = 4;
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthTestEnable = VK_TRUE;
    pipelineCI.layout = m_pipelineLayouts.defered;
    pipelineCI.renderPass = m_framebuffers.deferred->renderPass;

    // Blend attachment states required for all color attachments
    // This is important, as color write mask will otherwise be 0x0 and you
    // won't see anything rendered to the attachment
    pipelineCI.pVertexInputState =
        vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color,
                                                     vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Tangent});

    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Deferedshadows/Model.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Deferedshadows/Model.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_pipelines.defered));

    PreparePipelineLighting();
    PreparePipelineSkyBox();
    PreparePipelineSSAO();
    PreparePipelineSdfAO();
    PreparePipelineCSM();
    PreparePipelineHBAO();
    PreparePipelineGTAO();
    PreparePipelineCBF();
}
void Renderer::PreparePipelineLighting()
{
    // std::vector<VkPushConstantRange> pushConstantRanges = {
    //	Init::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(LightingPass::PushBlock), 0),
    // };

    if (m_pipelineLayouts.composition != nullptr)
    {
        vkDestroyPipelineLayout(m_device, m_pipelineLayouts.composition, nullptr);
    }
    if (m_pipelines.composition != nullptr)
    {
        vkDestroyPipeline(m_device, m_pipelines.composition, nullptr);
    }

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&m_descriptorSetLayouts.composition, 1);
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.composition));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    std::array<VkPipelineColorBlendAttachmentState, 2> lightingBlendAttachmentStates = {Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE)};

    VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, lightingBlendAttachmentStates.data());
    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_pipelineLayouts.composition, m_framebuffers.lighting->renderPass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();

    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Main/Composition.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Main/Composition.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    uint32_t useAO = Settings.AOSetting.UseAO == 0 ? 0 : 1;
    uint32_t useIBL{0};
    if (Settings.PBRSetting.SkyBoxIndex != 0 && Settings.PBRSetting.UseIBL)
    {
        useIBL = 1;
    }
    uint32_t showDirLight = Settings.ShadowSetting.DirLight == true ? 1 : 0;
    uint32_t showDirLightShadow = Settings.ShadowSetting.DirLightShadow == true ? 1 : 0;
    uint32_t dirLightShadowPCF = Settings.ShadowSetting.DirLightPCF == true ? 1 : 0;
    uint32_t showPointLight = Settings.ShadowSetting.PointLight == true ? 1 : 0;
    uint32_t showPointLightShadow = Settings.ShadowSetting.PointLightShadow == true ? 1 : 0;
    uint32_t showSpotLight = Settings.ShadowSetting.SpotLight == true ? 1 : 0;
    struct Data
    {
        uint32_t UseAO;
        uint32_t UseIBL;
        uint32_t ShowDirLight;
        uint32_t ShowDirLightShadow;
        uint32_t ShowPointLight;
        uint32_t ShowPointLightShadow;
        uint32_t ShowSpotLight;
        uint32_t DirLightShadowPCF;
    } data;
    data.UseAO = useAO;
    data.UseIBL = useIBL;
    data.ShowDirLight = showDirLight;
    data.ShowDirLightShadow = showDirLightShadow;
    data.ShowPointLight = showPointLight;
    data.ShowPointLightShadow = showPointLightShadow;
    data.ShowSpotLight = showSpotLight;
    data.DirLightShadowPCF = dirLightShadowPCF;
    std::vector<VkSpecializationMapEntry> specializations = {Init::specializationMapEntry(0, offsetof(Data, UseAO), sizeof(uint32_t)),
                                                             Init::specializationMapEntry(1, offsetof(Data, UseIBL), sizeof(uint32_t)),
                                                             Init::specializationMapEntry(2, offsetof(Data, ShowDirLight), sizeof(uint32_t)),
                                                             Init::specializationMapEntry(3, offsetof(Data, ShowDirLightShadow), sizeof(uint32_t)),
                                                             Init::specializationMapEntry(4, offsetof(Data, ShowPointLight), sizeof(uint32_t)),
                                                             Init::specializationMapEntry(5, offsetof(Data, ShowPointLightShadow), sizeof(uint32_t)),
                                                             Init::specializationMapEntry(6, offsetof(Data, ShowSpotLight), sizeof(uint32_t)),
                                                             Init::specializationMapEntry(7, offsetof(Data, DirLightShadowPCF), sizeof(uint32_t))};

    VkSpecializationInfo specializationInfo = Init::specializationInfo(specializations.size(), specializations.data(), sizeof(data), &data);
    shaderStages[1].pSpecializationInfo = &specializationInfo;

    // Empty vertex input state, vertices are generated by the vertex shader
    VkPipelineVertexInputStateCreateInfo emptyInputState = Init::pipelineVertexInputStateCreateInfo();
    pipelineCI.pVertexInputState = &emptyInputState;
    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_pipelines.composition));
}
/// @brief 全屏三角形->scene.textures.lutBrdf.image
void Renderer::GenerateBRDFLUT()
{
    auto tStart = std::chrono::high_resolution_clock::now();

    const VkFormat format = VK_FORMAT_R16G16_SFLOAT;
    const int32_t dim = 512;
    // Image
    VkImageCreateInfo imageCI = Init::imageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = dim;
    imageCI.extent.height = dim;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    Tool::CheckResult(vkCreateImage(m_device, &imageCI, nullptr, &scene.textures.lutBrdf.image));
    VkMemoryAllocateInfo memAlloc = Init::memoryAllocateInfo();
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, scene.textures.lutBrdf.image, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Tool::CheckResult(vkAllocateMemory(m_device, &memAlloc, nullptr, &scene.textures.lutBrdf.deviceMemory));
    Tool::CheckResult(vkBindImageMemory(m_device, scene.textures.lutBrdf.image, scene.textures.lutBrdf.deviceMemory, 0));

    // Image view
    VkImageViewCreateInfo viewCI = Init::imageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format = format;
    viewCI.subresourceRange = {};
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = 1;
    viewCI.subresourceRange.layerCount = 1;
    viewCI.image = scene.textures.lutBrdf.image;
    Tool::CheckResult(vkCreateImageView(m_device, &viewCI, nullptr, &scene.textures.lutBrdf.view));

    // Sampler
    VkSamplerCreateInfo samplerCI = Init::samplerCreateInfo();
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 1.0f;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    Tool::CheckResult(vkCreateSampler(m_device, &samplerCI, nullptr, &scene.textures.lutBrdf.sampler));

    scene.textures.lutBrdf.descriptor.imageView = scene.textures.lutBrdf.view;
    scene.textures.lutBrdf.descriptor.sampler = scene.textures.lutBrdf.sampler;
    scene.textures.lutBrdf.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    scene.textures.lutBrdf.device = m_vulkanDevice;

    // FB, Att, RP, Pipe, etc.
    VkAttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassCI = Init::renderPassCreateInfo();
    renderPassCI.attachmentCount = 1;
    renderPassCI.pAttachments = &attDesc;
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 2;
    renderPassCI.pDependencies = dependencies.data();

    VkRenderPass renderpass;
    Tool::CheckResult(vkCreateRenderPass(m_device, &renderPassCI, nullptr, &renderpass));

    VkFramebufferCreateInfo framebufferCI = Init::framebufferCreateInfo();
    framebufferCI.renderPass = renderpass;
    framebufferCI.attachmentCount = 1;
    framebufferCI.pAttachments = &scene.textures.lutBrdf.view;
    framebufferCI.width = dim;
    framebufferCI.height = dim;
    framebufferCI.layers = 1;

    VkFramebuffer framebuffer;
    Tool::CheckResult(vkCreateFramebuffer(m_device, &framebufferCI, nullptr, &framebuffer));

    // Descriptors
    VkDescriptorSetLayout descriptorsetlayout;
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {};
    VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

    // Descriptor Pool
    std::vector<VkDescriptorPoolSize> poolSizes = {Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)};
    VkDescriptorPoolCreateInfo descriptorPoolCI = Init::descriptorPoolCreateInfo(poolSizes, 2);
    VkDescriptorPool descriptorpool;
    Tool::CheckResult(vkCreateDescriptorPool(m_device, &descriptorPoolCI, nullptr, &descriptorpool));

    // Descriptor sets
    VkDescriptorSet descriptorset;
    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorset));

    // Pipeline layout
    VkPipelineLayout pipelinelayout;
    VkPipelineLayoutCreateInfo pipelineLayoutCI = Init::pipelineLayoutCreateInfo(&descriptorsetlayout, 1);
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &pipelinelayout));

    // Pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    VkPipelineVertexInputStateCreateInfo emptyInputState = Init::pipelineVertexInputStateCreateInfo();
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(pipelinelayout, renderpass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &emptyInputState;

    // Look-up-table (from BRDF) pipeline
    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "pbribl/genbrdflut.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "pbribl/genbrdflut.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipeline pipeline;
    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

    // Render
    VkClearValue clearValues[1];
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo renderPassBeginInfo = Init::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = renderpass;
    renderPassBeginInfo.renderArea.extent.width = dim;
    renderPassBeginInfo.renderArea.extent.height = dim;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues;
    renderPassBeginInfo.framebuffer = framebuffer;

    VkCommandBuffer cmdBuf = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport = Init::viewport((float)dim, (float)dim, 0.0f, 1.0f);
    VkRect2D scissor = Init::rect2D(dim, dim, 0, 0);
    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmdBuf);
    m_vulkanDevice->FlushCommandBuffer(cmdBuf, m_queues.graphicsQueue);

    vkQueueWaitIdle(m_queues.graphicsQueue);

    vkDestroyPipeline(m_device, pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, pipelinelayout, nullptr);
    vkDestroyRenderPass(m_device, renderpass, nullptr);
    vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    vkDestroyDescriptorSetLayout(m_device, descriptorsetlayout, nullptr);
    vkDestroyDescriptorPool(m_device, descriptorpool, nullptr);

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    // std::cout << "Generating BRDF LUT took " << tDiff << " ms" << std::endl;
}

/// @brief 输入:environmentcube 输出: irradiance cubemap 中间创建了临时图片用作RT和transfer src
void Renderer::GenerateIrradianceCube()
{
    auto tStart = std::chrono::high_resolution_clock::now();

    const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
    const int32_t dim = 64;
    const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

    // Pre-filtered cube map
    // Image
    VkImageCreateInfo imageCI = Init::imageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = dim;
    imageCI.extent.height = dim;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = numMips;
    imageCI.arrayLayers = 6;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    /*imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;*/
    Tool::CheckResult(vkCreateImage(m_device, &imageCI, nullptr, &scene.textures.irradianceCube.image));
    VkMemoryAllocateInfo memAlloc = Init::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    vkGetImageMemoryRequirements(m_device, scene.textures.irradianceCube.image, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Tool::CheckResult(vkAllocateMemory(m_device, &memAlloc, nullptr, &scene.textures.irradianceCube.deviceMemory));
    Tool::CheckResult(vkBindImageMemory(m_device, scene.textures.irradianceCube.image, scene.textures.irradianceCube.deviceMemory, 0));

    // Image view
    VkImageViewCreateInfo viewCI = Init::imageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCI.format = format;
    viewCI.subresourceRange = {};
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = numMips;
    viewCI.subresourceRange.layerCount = 6;
    viewCI.image = scene.textures.irradianceCube.image;
    Tool::CheckResult(vkCreateImageView(m_device, &viewCI, nullptr, &scene.textures.irradianceCube.view));

    // Sampler
    VkSamplerCreateInfo samplerCI = Init::samplerCreateInfo();
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = static_cast<float>(numMips);
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    Tool::CheckResult(vkCreateSampler(m_device, &samplerCI, nullptr, &scene.textures.irradianceCube.sampler));

    scene.textures.irradianceCube.descriptor.imageView = scene.textures.irradianceCube.view;
    scene.textures.irradianceCube.descriptor.sampler = scene.textures.irradianceCube.sampler;
    scene.textures.irradianceCube.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // FB, Att, RP, Pipe, etc.
    VkAttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Renderpass
    VkRenderPassCreateInfo renderPassCI = Init::renderPassCreateInfo();
    renderPassCI.attachmentCount = 1;
    renderPassCI.pAttachments = &attDesc;
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 2;
    renderPassCI.pDependencies = dependencies.data();
    VkRenderPass renderpass;
    Tool::CheckResult(vkCreateRenderPass(m_device, &renderPassCI, nullptr, &renderpass));

    struct
    {
        VkImage image;
        VkImageView view;
        VkDeviceMemory memory;
        VkFramebuffer framebuffer;
    } offscreen;

    // Offscreen framebuffer
    {
        // Color attachment
        VkImageCreateInfo imageCreateInfo = Init::imageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.extent.width = dim;
        imageCreateInfo.extent.height = dim;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Tool::CheckResult(vkCreateImage(m_device, &imageCreateInfo, nullptr, &offscreen.image));

        VkMemoryAllocateInfo memAlloc = Init::memoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(m_device, offscreen.image, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Tool::CheckResult(vkAllocateMemory(m_device, &memAlloc, nullptr, &offscreen.memory));
        Tool::CheckResult(vkBindImageMemory(m_device, offscreen.image, offscreen.memory, 0));

        VkImageViewCreateInfo colorImageView = Init::imageViewCreateInfo();
        colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorImageView.format = format;
        colorImageView.flags = 0;
        colorImageView.subresourceRange = {};
        colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorImageView.subresourceRange.baseMipLevel = 0;
        colorImageView.subresourceRange.levelCount = 1;
        colorImageView.subresourceRange.baseArrayLayer = 0;
        colorImageView.subresourceRange.layerCount = 1;
        colorImageView.image = offscreen.image;
        Tool::CheckResult(vkCreateImageView(m_device, &colorImageView, nullptr, &offscreen.view));

        VkFramebufferCreateInfo fbufCreateInfo = Init::framebufferCreateInfo();
        fbufCreateInfo.renderPass = renderpass;
        fbufCreateInfo.attachmentCount = 1;
        fbufCreateInfo.pAttachments = &offscreen.view;
        fbufCreateInfo.width = dim;
        fbufCreateInfo.height = dim;
        fbufCreateInfo.layers = 1;
        Tool::CheckResult(vkCreateFramebuffer(m_device, &fbufCreateInfo, nullptr, &offscreen.framebuffer));

        VkCommandBuffer layoutCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        Tool::SetImageLayout(layoutCmd, offscreen.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        m_vulkanDevice->FlushCommandBuffer(layoutCmd, m_queues.graphicsQueue, true);
    }
    // Descriptors
    VkDescriptorSetLayout descriptorsetlayout;
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

    // Descriptor Pool
    std::vector<VkDescriptorPoolSize> poolSizes = {Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)};
    VkDescriptorPoolCreateInfo descriptorPoolCI = Init::descriptorPoolCreateInfo(poolSizes, 2);
    VkDescriptorPool descriptorpool;
    Tool::CheckResult(vkCreateDescriptorPool(m_device, &descriptorPoolCI, nullptr, &descriptorpool));

    // Descriptor sets
    VkDescriptorSet descriptorset;
    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorset));
    VkWriteDescriptorSet writeDescriptorSet =
        Init::writeDescriptorSet(descriptorset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &scene.textures.environmentCube.descriptor);
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

    // Pipeline layout
    struct PushBlock
    {
        glm::mat4 mvp;
        // Sampling deltas
        float deltaPhi = (2.0f * float(PI)) / 180.0f;
        float deltaTheta = (0.5f * float(PI)) / 64.0f;
    } pushBlock;
    VkPipelineLayout pipelinelayout;
    std::vector<VkPushConstantRange> pushConstantRanges = {
        Init::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushBlock), 0),
    };
    VkPipelineLayoutCreateInfo pipelineLayoutCI = Init::pipelineLayoutCreateInfo(&descriptorsetlayout, 1);
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &pipelinelayout));

    // Pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(pipelinelayout, renderpass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.renderPass = renderpass;
    pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position});

    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "pbribl/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "pbribl/irradiancecube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipeline pipeline;
    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

    // Render

    VkClearValue clearValues[1];
    clearValues[0].color = {{0.0f, 0.0f, 0.2f, 0.0f}};

    VkRenderPassBeginInfo renderPassBeginInfo = Init::renderPassBeginInfo();
    // Reuse render pass from example pass
    renderPassBeginInfo.renderPass = renderpass;
    renderPassBeginInfo.framebuffer = offscreen.framebuffer;
    renderPassBeginInfo.renderArea.extent.width = dim;
    renderPassBeginInfo.renderArea.extent.height = dim;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues;

    std::vector<glm::mat4> matrices = {
        // POSITIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f),
                    glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f),
                    glm::vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    };

    VkCommandBuffer cmdBuf = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkViewport viewport = Init::viewport((float)dim, (float)dim, 0.0f, 1.0f);
    VkRect2D scissor = Init::rect2D(dim, dim, 0, 0);

    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = numMips;
    subresourceRange.layerCount = 6;

    // Change image layout for all cubemap faces to transfer destination
    Tool::SetImageLayout(cmdBuf, scene.textures.irradianceCube.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         subresourceRange);

    for (uint32_t m = 0; m < numMips; m++)
    {
        for (uint32_t f = 0; f < 6; f++)
        {
            viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
            viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
            vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

            // Render scene from cube face's point of view
            vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Update shader push constant block
            pushBlock.mvp = glm::perspective((float)(PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

            vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

            vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL);

            scene.skybox.Draw(cmdBuf);

            vkCmdEndRenderPass(cmdBuf);

            Tool::SetImageLayout(cmdBuf, offscreen.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            // Copy region for transfer from framebuffer to cube face
            VkImageCopy copyRegion = {};

            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset = {0, 0, 0};

            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = f;
            copyRegion.dstSubresource.mipLevel = m;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstOffset = {0, 0, 0};

            copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
            copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
            copyRegion.extent.depth = 1;

            vkCmdCopyImage(cmdBuf, offscreen.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, scene.textures.irradianceCube.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            // Transform framebuffer color attachment back
            Tool::SetImageLayout(cmdBuf, offscreen.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
    }

    Tool::SetImageLayout(cmdBuf, scene.textures.irradianceCube.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         subresourceRange);

    m_vulkanDevice->FlushCommandBuffer(cmdBuf, m_queues.graphicsQueue);

    vkDestroyRenderPass(m_device, renderpass, nullptr);
    vkDestroyFramebuffer(m_device, offscreen.framebuffer, nullptr);
    vkFreeMemory(m_device, offscreen.memory, nullptr);
    vkDestroyImageView(m_device, offscreen.view, nullptr);
    vkDestroyImage(m_device, offscreen.image, nullptr);
    vkDestroyDescriptorPool(m_device, descriptorpool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, descriptorsetlayout, nullptr);
    vkDestroyPipeline(m_device, pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, pipelinelayout, nullptr);

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    // std::cout << "Generating irradiance cube with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
}
void Renderer::GeneratePrefilteredCube()
{
    auto tStart = std::chrono::high_resolution_clock::now();

    const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
    const int32_t dim = 512;
    const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

    // Pre-filtered cube map
    // Image
    VkImageCreateInfo imageCI = Init::imageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = dim;
    imageCI.extent.height = dim;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = numMips;
    imageCI.arrayLayers = 6;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    Tool::CheckResult(vkCreateImage(m_device, &imageCI, nullptr, &scene.textures.prefilteredCube.image));
    VkMemoryAllocateInfo memAlloc = Init::memoryAllocateInfo();
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, scene.textures.prefilteredCube.image, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Tool::CheckResult(vkAllocateMemory(m_device, &memAlloc, nullptr, &scene.textures.prefilteredCube.deviceMemory));
    Tool::CheckResult(vkBindImageMemory(m_device, scene.textures.prefilteredCube.image, scene.textures.prefilteredCube.deviceMemory, 0));

    // Image view
    VkImageViewCreateInfo viewCI = Init::imageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCI.format = format;
    viewCI.subresourceRange = {};
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = numMips;
    viewCI.subresourceRange.layerCount = 6;
    viewCI.image = scene.textures.prefilteredCube.image;
    Tool::CheckResult(vkCreateImageView(m_device, &viewCI, nullptr, &scene.textures.prefilteredCube.view));

    // Sampler
    VkSamplerCreateInfo samplerCI = Init::samplerCreateInfo();
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = static_cast<float>(numMips);
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    Tool::CheckResult(vkCreateSampler(m_device, &samplerCI, nullptr, &scene.textures.prefilteredCube.sampler));

    scene.textures.prefilteredCube.descriptor.imageView = scene.textures.prefilteredCube.view;
    scene.textures.prefilteredCube.descriptor.sampler = scene.textures.prefilteredCube.sampler;
    scene.textures.prefilteredCube.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    scene.textures.prefilteredCube.device = m_vulkanDevice;

    // FB, Att, RP, Pipe, etc.
    VkAttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Renderpass
    VkRenderPassCreateInfo renderPassCI = Init::renderPassCreateInfo();
    renderPassCI.attachmentCount = 1;
    renderPassCI.pAttachments = &attDesc;
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 2;
    renderPassCI.pDependencies = dependencies.data();

    VkRenderPass renderpass;
    Tool::CheckResult(vkCreateRenderPass(m_device, &renderPassCI, nullptr, &renderpass));

    struct
    {
        VkImage image;
        VkImageView view;
        VkDeviceMemory memory;
        VkFramebuffer framebuffer;
    } offscreen;

    // Offfscreen framebuffer 将创建image,view等,同时改变layout
    {
        // Color attachment
        VkImageCreateInfo imageCreateInfo = Init::imageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.extent.width = dim;
        imageCreateInfo.extent.height = dim;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Tool::CheckResult(vkCreateImage(m_device, &imageCreateInfo, nullptr, &offscreen.image));

        VkMemoryAllocateInfo memAlloc = Init::memoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(m_device, offscreen.image, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Tool::CheckResult(vkAllocateMemory(m_device, &memAlloc, nullptr, &offscreen.memory));
        Tool::CheckResult(vkBindImageMemory(m_device, offscreen.image, offscreen.memory, 0));

        VkImageViewCreateInfo colorImageView = Init::imageViewCreateInfo();
        colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorImageView.format = format;
        colorImageView.flags = 0;
        colorImageView.subresourceRange = {};
        colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorImageView.subresourceRange.baseMipLevel = 0;
        colorImageView.subresourceRange.levelCount = 1;
        colorImageView.subresourceRange.baseArrayLayer = 0;
        colorImageView.subresourceRange.layerCount = 1;
        colorImageView.image = offscreen.image;
        Tool::CheckResult(vkCreateImageView(m_device, &colorImageView, nullptr, &offscreen.view));

        VkFramebufferCreateInfo fbufCreateInfo = Init::framebufferCreateInfo();
        fbufCreateInfo.renderPass = renderpass;
        fbufCreateInfo.attachmentCount = 1;
        fbufCreateInfo.pAttachments = &offscreen.view;
        fbufCreateInfo.width = dim;
        fbufCreateInfo.height = dim;
        fbufCreateInfo.layers = 1;
        Tool::CheckResult(vkCreateFramebuffer(m_device, &fbufCreateInfo, nullptr, &offscreen.framebuffer));

        VkCommandBuffer layoutCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        Tool::SetImageLayout(layoutCmd, offscreen.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        m_vulkanDevice->FlushCommandBuffer(layoutCmd, m_queues.graphicsQueue, true);
    }

    // Descriptors
    VkDescriptorSetLayout descriptorsetlayout;
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

    // Descriptor Pool
    std::vector<VkDescriptorPoolSize> poolSizes = {Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)};
    VkDescriptorPoolCreateInfo descriptorPoolCI = Init::descriptorPoolCreateInfo(poolSizes, 2);
    VkDescriptorPool descriptorpool;
    Tool::CheckResult(vkCreateDescriptorPool(m_device, &descriptorPoolCI, nullptr, &descriptorpool));

    // Descriptor sets
    VkDescriptorSet descriptorset;
    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorset));
    VkWriteDescriptorSet writeDescriptorSet =
        Init::writeDescriptorSet(descriptorset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &scene.textures.environmentCube.descriptor);
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

    // Pipeline layout
    struct PushBlock
    {
        glm::mat4 mvp;
        float roughness;
        uint32_t numSamples = 32u;
    } pushBlock;

    VkPipelineLayout pipelinelayout;
    std::vector<VkPushConstantRange> pushConstantRanges = {
        Init::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushBlock), 0),
    };
    VkPipelineLayoutCreateInfo pipelineLayoutCI = Init::pipelineLayoutCreateInfo(&descriptorsetlayout, 1);
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &pipelinelayout));

    // Pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(pipelinelayout, renderpass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.renderPass = renderpass;
    pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position});

    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "pbribl/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "pbribl/prefilterenvmap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipeline pipeline;
    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

    // Render

    VkClearValue clearValues[1];
    clearValues[0].color = {{0.0f, 0.0f, 0.2f, 0.0f}};

    VkRenderPassBeginInfo renderPassBeginInfo = Init::renderPassBeginInfo();
    // Reuse render pass from example pass
    renderPassBeginInfo.renderPass = renderpass;
    renderPassBeginInfo.framebuffer = offscreen.framebuffer;
    renderPassBeginInfo.renderArea.extent.width = dim;
    renderPassBeginInfo.renderArea.extent.height = dim;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues;

    std::vector<glm::mat4> matrices = {
        // POSITIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f),
                    glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f),
                    glm::vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    };

    VkCommandBuffer cmdBuf = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkViewport viewport = Init::viewport((float)dim, (float)dim, 0.0f, 1.0f);
    VkRect2D scissor = Init::rect2D(dim, dim, 0, 0);

    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = numMips;
    subresourceRange.layerCount = 6;

    // Change image layout for all cubemap faces to transfer destination
    Tool::SetImageLayout(cmdBuf, scene.textures.prefilteredCube.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         subresourceRange);
    for (uint32_t m = 0; m < numMips; m++)
    {
        pushBlock.roughness = (float)m / (float)(numMips - 1);
        for (uint32_t f = 0; f < 6; f++)
        {
            viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
            viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
            vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

            // Render scene from cube face's point of view
            vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Update shader push constant block
            pushBlock.mvp = glm::perspective((float)(PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

            vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

            vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL);

            scene.skybox.Draw(cmdBuf);

            vkCmdEndRenderPass(cmdBuf);

            Tool::SetImageLayout(cmdBuf, offscreen.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            // Copy region for transfer from framebuffer to cube face
            VkImageCopy copyRegion = {};

            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset = {0, 0, 0};

            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = f;
            copyRegion.dstSubresource.mipLevel = m;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstOffset = {0, 0, 0};

            copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
            copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
            copyRegion.extent.depth = 1;

            vkCmdCopyImage(cmdBuf, offscreen.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, scene.textures.prefilteredCube.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            // Transform framebuffer color attachment back
            Tool::SetImageLayout(cmdBuf, offscreen.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
    }
    Tool::SetImageLayout(cmdBuf, scene.textures.prefilteredCube.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         subresourceRange);

    m_vulkanDevice->FlushCommandBuffer(cmdBuf, m_queues.graphicsQueue);

    vkDestroyRenderPass(m_device, renderpass, nullptr);
    vkDestroyFramebuffer(m_device, offscreen.framebuffer, nullptr);
    vkFreeMemory(m_device, offscreen.memory, nullptr);
    vkDestroyImageView(m_device, offscreen.view, nullptr);
    vkDestroyImage(m_device, offscreen.image, nullptr);
    vkDestroyDescriptorPool(m_device, descriptorpool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, descriptorsetlayout, nullptr);
    vkDestroyPipeline(m_device, pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, pipelinelayout, nullptr);

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    // std::cout << "Generating pre-filtered enivornment cube with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
}

void Renderer::UpdateUniformBuffersBlur()
{
    m_ubos.blurParams.blurScale = Settings.PostSetting.BloomScale;
    m_ubos.blurParams.blurStrength = Settings.PostSetting.BloomStength;
    memcpy(m_uniformBuffers.blurParams.mapped, &m_ubos.blurParams, sizeof(m_ubos.blurParams));
}
void Renderer::CreateBuffersPointLights()
{
    if (m_lights.PointLights.size() != 0)
    {
        VkDeviceSize bufferSize{m_lights.PointLights.size() * sizeof(PointLight)};
        Buffer stagingBuffer;

        m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     &stagingBuffer, bufferSize, m_lights.PointLights.data());

        m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     &PointLightBuffer, bufferSize);

        VkCommandBuffer copyCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VkBufferCopy copyRegion{};
        copyRegion.size = bufferSize;
        vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, PointLightBuffer.buffer, 1, &copyRegion);

        m_vulkanDevice->FlushCommandBuffer(copyCmd, m_queues.graphicsQueue, true);

        stagingBuffer.Destroy();
    }

    for (auto& light : m_lights.PointLights)
    {
        if (light.castShadow == 1)
        {
            m_shadowPointLightCount += 1;
        }
    }
}
void Renderer::CreateBuffersShadowOmni()
{
    if (m_shadowPointLightCount != 0)
    {
        Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                       &m_shadowOmniPass.CBuffer, m_shadowPointLightCount * sizeof(ShadowOmniPass::CBufferDesc)));

        Tool::CheckResult(m_shadowOmniPass.CBuffer.Map());
    }

    // VkDeviceSize bufferSize{ m_shadowPointLightCount * sizeof(glm::vec4) };

    // Buffer stagingBuffer;

    // m_vulkanDevice->CreateBuffer(
    //	VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    //	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    //	&stagingBuffer,
    //	bufferSize,
    //	kernels.data());

    // m_vulkanDevice->CreateBuffer(
    //	// The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
    //	VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    //	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    //	&m_shadowOmniPass.LightMatricesBuffer,
    //	bufferSize);

    // VkCommandBuffer copyCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    // VkBufferCopy copyRegion{};
    // copyRegion.size = bufferSize;
    // vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, m_SSAOPass.buffers.kernels.buffer, 1, &copyRegion);

    // m_vulkanDevice->FlushCommandBuffer(copyCmd, m_queues.graphicsQueue, true);

    // stagingBuffer.Destroy();
}
void Renderer::PreparePipelineShadowOmni()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&m_shadowOmniPass.SetLayout, 1);
    VkPushConstantRange pushConstantRange = Init::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(ShadowOmniPass::PushBlockDesc), 0);

    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_shadowOmniPass.PipelineLayout));

    // Pipelines
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        Init::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);

    // Load shaders
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    // Offscreen pipeline
    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "shadowmappingomni/offscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "shadowmappingomni/offscreen.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_shadowOmniPass.PipelineLayout, m_shadowOmniPass.RenderPass, 0);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position});

    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_shadowOmniPass.Pipeline));
}
void Renderer::AllocateDescriptorSetShadowOmni()
{
    if (m_shadowPointLightCount != 0)
    {
        // 用于计算阴影
        // Layout
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            // Binding 0 : Vertex shader uniform buffer
            Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
        };

        VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
        Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_shadowOmniPass.SetLayout));

        // Set
        VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_shadowOmniPass.SetLayout, 1);

        Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_shadowOmniPass.Set));
        std::vector<VkWriteDescriptorSet> offScreenWriteDescriptorSets = {
            // Binding 0 : Vertex shader uniform buffer
            Init::writeDescriptorSet(m_shadowOmniPass.Set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_shadowOmniPass.CBuffer.descriptor),
        };
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(offScreenWriteDescriptorSets.size()), offScreenWriteDescriptorSets.data(), 0, nullptr);
    }
}

// 创建图片,
void Renderer::SetupPassShadowOmni()
{
    CreateBuffersShadowOmni();

    m_shadowOmniPass.FrameBuffers.resize(m_shadowPointLightCount);
    m_shadowOmniPass.CubeMaps.resize(m_shadowPointLightCount);

    std::vector<int> index;
    index.reserve(m_shadowPointLightCount);
    for (int i = 0; i < m_lights.PointLights.size(); ++i)
    {
        if (m_lights.PointLights[i].castShadow == 1)
            index.emplace_back(i);
    }

    const VkFormat offscreenImageFormat{VK_FORMAT_R32_SFLOAT};
    const uint32_t offscreenImageSize{1280};
    VkFormat offscreenDepthFormat{VK_FORMAT_UNDEFINED};
    uint32_t width = 1280;
    uint32_t height = 1280;

    // Cube map image description
    VkImageCreateInfo imageCreateInfo = Init::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = VK_FORMAT_R32_SFLOAT;
    imageCreateInfo.extent = {width, height, 1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 6;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VkMemoryAllocateInfo memAllocInfo = Init::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    VkCommandBuffer layoutCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Create cube map image

    for (int i = 0; i < m_shadowPointLightCount; ++i)
    {
        Tool::CheckResult(vkCreateImage(m_device, &imageCreateInfo, nullptr, &m_shadowOmniPass.CubeMaps[i].Tex.image));
    }

    if (m_shadowPointLightCount != 0)
        vkGetImageMemoryRequirements(m_device, m_shadowOmniPass.CubeMaps[0].Tex.image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    for (int i = 0; i < m_shadowPointLightCount; ++i)
    {
        Tool::CheckResult(vkAllocateMemory(m_device, &memAllocInfo, nullptr, &m_shadowOmniPass.CubeMaps[i].Tex.deviceMemory));
        Tool::CheckResult(vkBindImageMemory(m_device, m_shadowOmniPass.CubeMaps[i].Tex.image, m_shadowOmniPass.CubeMaps[i].Tex.deviceMemory, 0));
    }

    // Image barrier for optimal image (target)
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 6;
    for (int i = 0; i < m_shadowPointLightCount; ++i)
    {
        Tool::SetImageLayout(layoutCmd, m_shadowOmniPass.CubeMaps[i].Tex.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             subresourceRange);
    }
    m_vulkanDevice->FlushCommandBuffer(layoutCmd, m_queues.graphicsQueue, true);

    // Create sampler
    VkSamplerCreateInfo sampler = Init::samplerCreateInfo();
    sampler.magFilter = VK_FILTER_NEAREST;
    sampler.minFilter = VK_FILTER_NEAREST;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler.addressModeV = sampler.addressModeU;
    sampler.addressModeW = sampler.addressModeU;
    sampler.mipLodBias = 0.0f;
    sampler.maxAnisotropy = 1.0f;
    sampler.compareOp = VK_COMPARE_OP_NEVER;
    sampler.minLod = 0.0f;
    sampler.maxLod = 1.0f;
    sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    Tool::CheckResult(vkCreateSampler(m_device, &sampler, nullptr, &m_shadowOmniPass.Sampler));
    // MARK

    // Create image view
    VkImageViewCreateInfo view = Init::imageViewCreateInfo();
    view.image = VK_NULL_HANDLE;
    view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    view.format = offscreenImageFormat;
    view.components = {VK_COMPONENT_SWIZZLE_R};
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    view.subresourceRange.layerCount = 6;
    for (int i = 0; i < m_shadowPointLightCount; ++i)
    {

        view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        view.subresourceRange.layerCount = 6;
        view.subresourceRange.baseArrayLayer = 0;
        view.image = m_shadowOmniPass.CubeMaps[i].Tex.image;
        Tool::CheckResult(vkCreateImageView(m_device, &view, nullptr, &m_shadowOmniPass.CubeMaps[i].Tex.view));

        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.subresourceRange.layerCount = 1;
        // view.image = shadowCubeMap.texture.image;

        for (uint32_t j = 0; j < 6; j++)
        {
            view.subresourceRange.baseArrayLayer = j;
            Tool::CheckResult(vkCreateImageView(m_device, &view, nullptr, &m_shadowOmniPass.CubeMaps[i].FaceViews[j]));
        }
    }

    AllocateDescriptorSetShadowOmni();

    // Set up a separate render pass for the offscreen frame buffer
    // This is necessary as the offscreen frame buffer attachments
    // use formats different to the ones from the visible frame buffer
    // an at least the depth one may not be compatible

    VkAttachmentDescription osAttachments[2] = {};

    // Find a suitable depth format for
    VkBool32 validDepthFormat = Tool::GetSupportedDepthFormat(m_physicalDevice, &offscreenDepthFormat);
    assert(validDepthFormat);

    osAttachments[0].format = offscreenImageFormat;
    osAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    osAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    osAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    osAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    osAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    osAttachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    osAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Depth attachment
    osAttachments[1].format = offscreenDepthFormat;
    osAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    osAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    osAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    osAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    osAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    osAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    osAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    subpass.pDepthStencilAttachment = &depthReference;

    VkRenderPassCreateInfo renderPassCreateInfo = Init::renderPassCreateInfo();
    renderPassCreateInfo.attachmentCount = 2;
    renderPassCreateInfo.pAttachments = osAttachments;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;

    Tool::CheckResult(vkCreateRenderPass(m_device, &renderPassCreateInfo, nullptr, &m_shadowOmniPass.RenderPass));

    PreparePipelineShadowOmni();

    // Prepare *many* new framebuffer for offscreen rendering
    // The contents of this framebuffer are then
    // copied to the different cube map faces

    // offscreenPass.width = offscreenImageSize;
    // offscreenPass.height = offscreenImageSize;

    // Depth attachment
    imageCreateInfo = Init::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = offscreenDepthFormat;
    imageCreateInfo.extent.width = offscreenImageSize;
    imageCreateInfo.extent.height = offscreenImageSize;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    Tool::CheckResult(vkCreateImage(m_device, &imageCreateInfo, nullptr, &m_shadowOmniPass.Depth.image));

    layoutCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Depth stencil attachment

    VkImageViewCreateInfo depthStencilView = Init::imageViewCreateInfo();
    depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthStencilView.format = offscreenDepthFormat;
    depthStencilView.flags = 0;
    depthStencilView.subresourceRange = {};
    depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (offscreenDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
    {
        depthStencilView.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    depthStencilView.subresourceRange.baseMipLevel = 0;
    depthStencilView.subresourceRange.levelCount = 1;
    depthStencilView.subresourceRange.baseArrayLayer = 0;
    depthStencilView.subresourceRange.layerCount = 1;

    vkGetImageMemoryRequirements(m_device, m_shadowOmniPass.Depth.image, &memReqs);

    VkMemoryAllocateInfo memAlloc = Init::memoryAllocateInfo();
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Tool::CheckResult(vkAllocateMemory(m_device, &memAlloc, nullptr, &m_shadowOmniPass.Depth.memory));
    Tool::CheckResult(vkBindImageMemory(m_device, m_shadowOmniPass.Depth.image, m_shadowOmniPass.Depth.memory, 0));

    Tool::SetImageLayout(layoutCmd, m_shadowOmniPass.Depth.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    m_vulkanDevice->FlushCommandBuffer(layoutCmd, m_queues.graphicsQueue, true);

    depthStencilView.image = m_shadowOmniPass.Depth.image;
    Tool::CheckResult(vkCreateImageView(m_device, &depthStencilView, nullptr, &m_shadowOmniPass.Depth.view));

    VkImageView attachments[2];
    attachments[1] = m_shadowOmniPass.Depth.view;

    VkFramebufferCreateInfo fbufCreateInfo = Init::framebufferCreateInfo();
    fbufCreateInfo.renderPass = m_shadowOmniPass.RenderPass;
    fbufCreateInfo.attachmentCount = 2;
    fbufCreateInfo.pAttachments = attachments;
    fbufCreateInfo.width = offscreenImageSize;
    fbufCreateInfo.height = offscreenImageSize;
    fbufCreateInfo.layers = 1;

    for (uint32_t j = 0; j < m_shadowPointLightCount; j++)
    {
        for (uint32_t i = 0; i < 6; i++)
        {
            attachments[0] = m_shadowOmniPass.CubeMaps[j].FaceViews[i];
            Tool::CheckResult(vkCreateFramebuffer(m_device, &fbufCreateInfo, nullptr, &m_shadowOmniPass.FrameBuffers[j][i]));
        }
    }
    m_shadowOmniPass.CBufferData.reserve(m_shadowPointLightCount);
    for (int i = 0; i < index.size(); ++i)
    {
        int curIndex = index[i];
        glm::vec4& lightPos = m_lights.PointLights[curIndex].position;
        glm::mat4x4 proj = glm::perspective((float)(PI / 2.0), 1.0f, m_camera.znear, m_camera.zfar);
        proj[1][1] *= -1;
        m_shadowOmniPass.CBufferData.emplace_back(proj, glm::mat4(1.0f),
                                                  glm::translate(glm::mat4(1.0f), glm::vec3(-lightPos.x, -lightPos.y, -lightPos.z)), lightPos);
    }

    memcpy(m_shadowOmniPass.CBuffer.mapped, m_shadowOmniPass.CBufferData.data(), m_shadowPointLightCount * sizeof(ShadowOmniPass::CBufferDesc));

    // VkDescriptorImageInfo texDescriptor =
    //	Init::descriptorImageInfo(
    //		shadowCubeMap.texture.sampler,
    //		shadowCubeMap.texture.view,
    //		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // std::vector<VkWriteDescriptorSet> writeDescriptorSets;
    // writeDescriptorSets = { Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10, &texDescriptor)
    // }; vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
void Renderer::RenderToCube(const vkglTF::Model& model, const glm::vec3& pos, const std::string& savePath)
{
    VkCommandBuffer cmdBuffer = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo renderPassBeginInfo = Init::renderPassBeginInfo();
    renderPassBeginInfo = Init::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = m_DepthCubePass.RenderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = 64;
    renderPassBeginInfo.renderArea.extent.height = 64;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues.data();
    VkViewport viewport = Init::viewport(64.0f, 64.0f, 0.0f, 1.0f);
    VkRect2D scissor = Init::rect2D(64, 64, 0, 0);

    DepthCubeMapPass::PushBlockDesc data;
    glm::vec3 eye = glm::vec3(0.0f);
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    data.pos = pos;
    data.model = glm::translate(modelMatrix, -pos);
    for (int i = 0; i < 6; ++i)
    {
        renderPassBeginInfo.framebuffer = m_DepthCubePass.frameBuffer[i];
        switch (i)
        {
        case 0: // POSITIVE_X
            viewMatrix = glm::lookAt(eye, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            viewMatrix[2] = -viewMatrix[2];
            break;
        case 1: // NEGATIVE_X
            viewMatrix = glm::lookAt(eye, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            viewMatrix[2] = -viewMatrix[2];
            break;
        case 2: // POSITIVE_Y
            viewMatrix = glm::lookAt(eye, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            viewMatrix[2] = -viewMatrix[2];
            break;
        case 3: // NEGATIVE_Y
            viewMatrix = glm::lookAt(eye, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
            viewMatrix[2] = -viewMatrix[2];
            break;
        case 4: // POSITIVE_Z
            viewMatrix = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            viewMatrix[0] = -viewMatrix[0]; // 反转x轴,适应采样.
            break;
        case 5: // NEGATIVE_Z
            viewMatrix = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            viewMatrix[0] = -viewMatrix[0]; // 反转x轴,适应采样.
            break;
        }
        data.view = viewMatrix;
        vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
        vkCmdPushConstants(cmdBuffer, m_DepthCubePass.PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DepthCubeMapPass::PushBlockDesc), &data);
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DepthCubePass.Pipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DepthCubePass.PipelineLayout, 0, 1, &m_DepthCubePass.Set, 0, NULL);
        m_glTFModel.Draw(cmdBuffer);

        vkCmdEndRenderPass(cmdBuffer);
    }

    m_vulkanDevice->FlushCommandBuffer(cmdBuffer, m_queues.graphicsQueue, true);

    SaveToImage(m_DepthCubePass.cubeMap.Tex, savePath);
}
void Renderer::SaveToImage(const Texture& tex, const std::string& savePath)
{
    size_t pixelSize = 0;
    if (tex.format == VK_FORMAT_R32_SFLOAT)
    {
        pixelSize = sizeof(float); // 深度通常是 float
    }
    else
    {
        throw std::runtime_error("unsupported output image format");
    }
    VkDeviceSize bufferSize = pixelSize * tex.height * tex.width * tex.layerCount;

    Buffer stagingBuffer;

    m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 &stagingBuffer, bufferSize);

    VkCommandBuffer cmdBuffer = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, tex.layerCount};
    Tool::SetImageLayout(cmdBuffer, tex.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, range);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;   // 0 表示紧密打包
    region.bufferImageHeight = 0; // 0 表示紧密打包
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = tex.layerCount;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {tex.width, tex.height, 1};

    vkCmdCopyImageToBuffer(cmdBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.buffer, 1, &region);
    Tool::SetImageLayout(cmdBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // 转换回只读，以便后续使用
                         {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, tex.layerCount});
    m_vulkanDevice->FlushCommandBuffer(cmdBuffer, m_queues.graphicsQueue, true);
    void* data;
    vkMapMemory(m_device, stagingBuffer.memory, 0, bufferSize, 0, &data);
    float* depthData = static_cast<float*>(data); // 假设是 VK_FORMAT_D32_SFLOAT

    const uint32_t& imageWidth = tex.width;
    const uint32_t& imageHeight = tex.height;
    for (uint32_t layer = 0; layer < tex.layerCount; ++layer)
    {
        std::string layerSavePath = savePath + "_face_" + std::to_string(layer) + ".png";
        uint8_t* rgbaPixels = new uint8_t[imageWidth * imageHeight * 4]; // RGBA 8-bit

        for (uint32_t y = 0; y < imageHeight; ++y)
        {
            for (uint32_t x = 0; x < imageWidth; ++x)
            {
                float depthValue = depthData[layer * (imageWidth * imageHeight) + y * imageWidth + x];

                // 将深度值映射到 0-255 范围 (简单线性映射，可能需要更复杂的映射)
                // 假设深度值在 [0, 1] 范围
                uint8_t normalizedDepth = static_cast<uint8_t>(glm::clamp(depthValue, 0.0f, 1.0f) * 255.0f);

                // For a single-channel depth, you might save to R channel, or to all R, G, B for visualization
                // Here, let's put it into an RGBA image for easy PNG save
                rgbaPixels[(y * imageWidth + x) * 4 + 0] = normalizedDepth; // R
                rgbaPixels[(y * imageWidth + x) * 4 + 1] = normalizedDepth; // G
                rgbaPixels[(y * imageWidth + x) * 4 + 2] = normalizedDepth; // B
                rgbaPixels[(y * imageWidth + x) * 4 + 3] = 255;             // A (opaque)
            }
        }
        // 使用 stb_image_write 保存 PNG
        // #define STB_IMAGE_WRITE_IMPLEMENTATION
        // #include "stb_image_write.h"
        stbi_write_png(layerSavePath.c_str(), imageWidth, imageHeight, 4, rgbaPixels, imageWidth * 4);
        delete[] rgbaPixels;
    }

    vkUnmapMemory(m_device, stagingBuffer.memory);
    vkFreeMemory(m_device, stagingBuffer.memory, nullptr);
}
void Renderer::SetupPassDepthCubeMap()
{
    // Create Buffer
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &m_DepthCubePass.CBuffer, sizeof(m_DepthCubePass.CBufferData)));
    Tool::CheckResult(m_DepthCubePass.CBuffer.Map());

    // DescriptorSet
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        // Binding 0 : Vertex shader uniform buffer
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_DepthCubePass.SetLayout));
    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_DepthCubePass.SetLayout, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_DepthCubePass.Set));
    std::vector<VkWriteDescriptorSet> offScreenWriteDescriptorSets = {
        // Binding 0 : Vertex shader uniform buffer
        Init::writeDescriptorSet(m_DepthCubePass.Set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_DepthCubePass.CBuffer.descriptor),
    };
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(offScreenWriteDescriptorSets.size()), offScreenWriteDescriptorSets.data(), 0, nullptr);

    // attachments
    VkAttachmentDescription osAttachments[2] = {};
    // Find a suitable depth format for
    const VkFormat offscreenImageFormat{VK_FORMAT_R32_SFLOAT};
    const uint32_t offscreenImageSize{512};
    VkFormat offscreenDepthFormat{VK_FORMAT_UNDEFINED};
    VkBool32 validDepthFormat = Tool::GetSupportedDepthFormat(m_physicalDevice, &offscreenDepthFormat);
    assert(validDepthFormat);
    osAttachments[0].format = offscreenImageFormat;
    osAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    osAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    osAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    osAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    osAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    osAttachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    osAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // Depth attachment
    osAttachments[1].format = offscreenDepthFormat;
    osAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    osAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    osAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    osAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    osAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    osAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    osAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    subpass.pDepthStencilAttachment = &depthReference;
    VkRenderPassCreateInfo renderPassCreateInfo = Init::renderPassCreateInfo();
    renderPassCreateInfo.attachmentCount = 2;
    renderPassCreateInfo.pAttachments = osAttachments;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    Tool::CheckResult(vkCreateRenderPass(m_device, &renderPassCreateInfo, nullptr, &m_DepthCubePass.RenderPass));

    // Pipeline
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&m_DepthCubePass.SetLayout, 1);
    VkPushConstantRange pushConstantRange = Init::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(DepthCubeMapPass::PushBlockDesc), 0);
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_DepthCubePass.PipelineLayout));
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        Init::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    // Load shaders
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    // Offscreen pipeline
    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "shadowmappingomni/DepthCube.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "shadowmappingomni/DepthCube.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_DepthCubePass.PipelineLayout, m_DepthCubePass.RenderPass, 0);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position});
    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_DepthCubePass.Pipeline));

    // Color Image (depth)	VkImageCreateInfo imageCreateInfo = Init::imageCreateInfo();
    VkImageCreateInfo colorImageCI = Init::imageCreateInfo();
    colorImageCI.imageType = VK_IMAGE_TYPE_2D;
    colorImageCI.format = VK_FORMAT_R32_SFLOAT;
    colorImageCI.extent = {offscreenImageSize, offscreenImageSize, 1};
    colorImageCI.mipLevels = 1;
    colorImageCI.arrayLayers = 6;
    colorImageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    colorImageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    colorImageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    colorImageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VkMemoryAllocateInfo memAllocInfo = Init::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    Tool::CheckResult(vkCreateImage(m_device, &colorImageCI, nullptr, &m_DepthCubePass.cubeMap.Tex.image));
    vkGetImageMemoryRequirements(m_device, m_DepthCubePass.cubeMap.Tex.image, &memReqs);
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Tool::CheckResult(vkAllocateMemory(m_device, &memAllocInfo, nullptr, &m_DepthCubePass.cubeMap.Tex.deviceMemory));
    Tool::CheckResult(vkBindImageMemory(m_device, m_DepthCubePass.cubeMap.Tex.image, m_DepthCubePass.cubeMap.Tex.deviceMemory, 0));

    m_DepthCubePass.cubeMap.Tex.height = m_DepthCubePass.cubeMap.Tex.width = offscreenImageSize;
    m_DepthCubePass.cubeMap.Tex.format = offscreenImageFormat;
    m_DepthCubePass.cubeMap.Tex.layerCount = 6;
    // Create sampler
    VkSamplerCreateInfo sampler = Init::samplerCreateInfo();
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler.addressModeV = sampler.addressModeU;
    sampler.addressModeW = sampler.addressModeU;
    sampler.mipLodBias = 0.0f;
    sampler.maxAnisotropy = 1.0f;
    sampler.compareOp = VK_COMPARE_OP_NEVER;
    sampler.minLod = 0.0f;
    sampler.maxLod = 1.0f;
    sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    Tool::CheckResult(vkCreateSampler(m_device, &sampler, nullptr, &m_DepthCubePass.Sampler));
    // Depth Image
    VkImageCreateInfo imageCreateInfo = Init::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = offscreenDepthFormat;
    imageCreateInfo.extent.width = offscreenImageSize;
    imageCreateInfo.extent.height = offscreenImageSize;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    Tool::CheckResult(vkCreateImage(m_device, &imageCreateInfo, nullptr, &m_DepthCubePass.Depth.image));

    VkMemoryAllocateInfo memAlloc = Init::memoryAllocateInfo();

    vkGetImageMemoryRequirements(m_device, m_DepthCubePass.Depth.image, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Tool::CheckResult(vkAllocateMemory(m_device, &memAlloc, nullptr, &m_DepthCubePass.Depth.memory));
    Tool::CheckResult(vkBindImageMemory(m_device, m_DepthCubePass.Depth.image, m_DepthCubePass.Depth.memory, 0));

    VkCommandBuffer layoutCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    Tool::SetImageLayout(layoutCmd, m_DepthCubePass.Depth.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    // m_vulkanDevice->FlushCommandBuffer(layoutCmd, m_queues.graphicsQueue, true);

    // Image barrier for optimal image (target)
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 6;
    Tool::SetImageLayout(layoutCmd, m_DepthCubePass.cubeMap.Tex.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         subresourceRange);

    m_vulkanDevice->FlushCommandBuffer(layoutCmd, m_queues.graphicsQueue, true);
    // Create image view
    VkImageViewCreateInfo view = Init::imageViewCreateInfo();
    view.image = VK_NULL_HANDLE;
    view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    view.format = offscreenImageFormat;
    view.components = {VK_COMPONENT_SWIZZLE_R};
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    view.subresourceRange.layerCount = 6;
    view.image = m_DepthCubePass.cubeMap.Tex.image;
    Tool::CheckResult(vkCreateImageView(m_device, &view, nullptr, &m_DepthCubePass.cubeMap.Tex.view));
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.subresourceRange.layerCount = 1;
    // view.image = shadowCubeMap.texture.image;
    for (uint32_t j = 0; j < 6; j++)
    {
        view.subresourceRange.baseArrayLayer = j;
        Tool::CheckResult(vkCreateImageView(m_device, &view, nullptr, &m_DepthCubePass.cubeMap.FaceViews[j]));
    }

    VkImageViewCreateInfo depthStencilView = Init::imageViewCreateInfo();
    depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthStencilView.format = offscreenDepthFormat;
    depthStencilView.flags = 0;
    depthStencilView.subresourceRange = {};
    depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (offscreenDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
    {
        depthStencilView.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    depthStencilView.subresourceRange.baseMipLevel = 0;
    depthStencilView.subresourceRange.levelCount = 1;
    depthStencilView.subresourceRange.baseArrayLayer = 0;
    depthStencilView.subresourceRange.layerCount = 1;
    depthStencilView.image = m_DepthCubePass.Depth.image;
    Tool::CheckResult(vkCreateImageView(m_device, &depthStencilView, nullptr, &m_DepthCubePass.Depth.view));

    m_DepthCubePass.CBufferData.Proj = glm::perspective((float)(PI / 2.0), 1.0f, m_camera.znear, m_camera.zfar);

    memcpy(m_DepthCubePass.CBuffer.mapped, &m_DepthCubePass.CBufferData, sizeof(m_DepthCubePass.CBufferData));

    VkImageView attachments[2];
    attachments[1] = m_DepthCubePass.Depth.view;

    VkFramebufferCreateInfo fbufCreateInfo = Init::framebufferCreateInfo();
    fbufCreateInfo.renderPass = m_DepthCubePass.RenderPass;
    fbufCreateInfo.attachmentCount = 2;
    fbufCreateInfo.pAttachments = attachments;
    fbufCreateInfo.width = offscreenImageSize;
    fbufCreateInfo.height = offscreenImageSize;
    fbufCreateInfo.layers = 1;

    for (uint32_t i = 0; i < 6; i++)
    {
        attachments[0] = m_DepthCubePass.cubeMap.FaceViews[i];
        Tool::CheckResult(vkCreateFramebuffer(m_device, &fbufCreateInfo, nullptr, &m_DepthCubePass.frameBuffer[i]));
    }
}

// Updates a single cube map face
// Renders the scene with face's view directly to the cubemap layer `faceIndex`
// Uses push constants for quick update of view matrix for the current cube map face
void Renderer::UpdateCubeFace(uint32_t lightIndex, uint32_t faceIndex, VkCommandBuffer commandBuffer, VkDescriptorSet set)
{
    VkClearValue clearValues[2];
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassBeginInfo = Init::renderPassBeginInfo();
    // Reuse render pass from example pass
    renderPassBeginInfo.renderPass = m_shadowOmniPass.RenderPass;
    renderPassBeginInfo.framebuffer = m_shadowOmniPass.FrameBuffers[lightIndex][faceIndex];
    renderPassBeginInfo.renderArea.extent.width = ShadowOmniPass::WIDTH;
    renderPassBeginInfo.renderArea.extent.height = ShadowOmniPass::HEIGHT;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    // Update view matrix via push constant
    ShadowOmniPass::PushBlockDesc data;
    data.index = lightIndex;
    glm::vec3 eye = glm::vec3(0.0f);
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    switch (faceIndex)
    {
    case 0: // POSITIVE_X
        viewMatrix = glm::lookAt(eye, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        viewMatrix[2] = -viewMatrix[2];
        break;
    case 1: // NEGATIVE_X
        viewMatrix = glm::lookAt(eye, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        viewMatrix[2] = -viewMatrix[2];
        break;
    case 2: // POSITIVE_Y
        viewMatrix = glm::lookAt(eye, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        viewMatrix[2] = -viewMatrix[2];
        break;
    case 3: // NEGATIVE_Y
        viewMatrix = glm::lookAt(eye, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
        viewMatrix[2] = -viewMatrix[2];
        break;
    case 4: // POSITIVE_Z
        viewMatrix = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        viewMatrix[0] = -viewMatrix[0]; // 反转x轴,适应采样.
        break;
    case 5: // NEGATIVE_Z
        viewMatrix = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        viewMatrix[0] = -viewMatrix[0]; // 反转x轴,适应采样.
        break;
    }
    data.view = viewMatrix;
    // Render scene from cube face's point of view
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Update shader push constant block
    // Contains current face view matrix
    vkCmdPushConstants(commandBuffer, m_shadowOmniPass.PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowOmniPass::PushBlockDesc), &data);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowOmniPass.Pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowOmniPass.PipelineLayout, 0, 1, &set, 0, NULL);
    m_glTFModel.Draw(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);
}
/// @brief 将点光源buffer移送到computeBuffer 创建TilesBuffer
void Renderer::PrepareLightCullingBuffers()
{
    // std::default_random_engine rndEngine((unsigned)time(nullptr));
    // std::uniform_real_distribution<float> rndPos(-10.0f, 10.0f);
    // std::uniform_real_distribution<float> rndColor(-1.0f, 1.0f);

    // std::vector<PointLight> pointLightsData;
    // pointLightsData.reserve(100);

    //// construct PointLights
    // for (int i = 0; i < 100; ++i)
    //{
    //	pointLightsData.emplace_back(
    //		glm::vec4(rndPos(rndEngine), rndPos(rndEngine), rndPos(rndEngine), 1.0f),
    //		glm::vec4(rndColor(rndEngine), rndColor(rndEngine), rndColor(rndEngine), 1.0f),
    //		3.0f,
    //		1.0f
    //	);
    // }
    // VkDeviceSize lightBufferSize{ pointLightsData.size() * sizeof(PointLight) };

    // Buffer stagingBuffer1;
    Buffer stagingBuffer2;

    //// LightBuffer Init
    // m_vulkanDevice->CreateBuffer(
    //	VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    //	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    //	&stagingBuffer1,
    //	lightBufferSize,
    //	pointLightsData.data());

    // m_vulkanDevice->CreateBuffer(
    //	// The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
    //	VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    //	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    //	&m_compute.buffers.lights,
    //	lightBufferSize);

    // TileBuffer Init
    m_compute.buffers.tileData.resize((m_width + m_compute.tileSize - 1) / m_compute.tileSize * (m_height + m_compute.tileSize - 1) /
                                      m_compute.tileSize);
    VkDeviceSize tileBufferSize{m_compute.buffers.tileData.size() * sizeof(LightCullingPass::Tile)};

    m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 &stagingBuffer2, tileBufferSize, m_compute.buffers.tileData.data());

    m_vulkanDevice->CreateBuffer(
        // The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m_compute.buffers.tiles,
        tileBufferSize);

    // Copy
    VkCommandBuffer copyCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkBufferCopy copyRegion{};
    // copyRegion.size = lightBufferSize;
    // vkCmdCopyBuffer(copyCmd, stagingBuffer1.buffer, m_compute.buffers.lights.buffer, 1, &copyRegion);
    copyRegion.size = tileBufferSize;
    vkCmdCopyBuffer(copyCmd, stagingBuffer2.buffer, m_compute.buffers.tiles.buffer, 1, &copyRegion);

    // acquire light buffer and tilesBuffer
    if (m_index.compute != m_index.graphics)
    {
        std::vector<VkBufferMemoryBarrier> bufferBarriers;

        VkBufferMemoryBarrier bufferBarrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, 0, m_index.graphics, m_index.compute,
            m_compute.buffers.tiles.buffer,          0,       m_compute.buffers.tiles.size};
        bufferBarriers.push_back(bufferBarrier);

        bufferBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                         nullptr,
                         VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                         0,
                         m_index.graphics,
                         m_index.compute,
                         PointLightBuffer.buffer,
                         0,
                         PointLightBuffer.size};
        bufferBarriers.push_back(bufferBarrier);

        vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, bufferBarriers.size(),
                             bufferBarriers.data(), 0, nullptr);
    }
    m_vulkanDevice->FlushCommandBuffer(copyCmd, m_queues.graphicsQueue, true);
    stagingBuffer2.Destroy();

    // UBO Create
    m_compute.buffers.uniformBufferData.numLights = m_lights.PointLights.size();
    // m_compute.buffers.uniformBufferData.proj = m_camera.matrices.perspective;
    // m_compute.buffers.uniformBufferData.view = m_camera.matrices.view;
    // m_compute.buffers.uniformBufferData.screenSize = glm::vec2{ m_width,m_height };
    m_compute.buffers.uniformBufferData.numTilesX = (m_width + m_compute.tileSize - 1) / m_compute.tileSize;
    m_compute.buffers.uniformBufferData.numTilesY = (m_height + m_compute.tileSize - 1) / m_compute.tileSize;
    m_compute.buffers.uniformBufferData.tileSize = m_compute.tileSize;
    m_compute.buffers.uniformBufferData.screenSize = glm::vec2{m_width, m_height};
    m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 &m_compute.buffers.uniformBuffer, sizeof(LightCullingPass::UniformComputeCullingData));
    Tool::CheckResult(m_compute.buffers.uniformBuffer.Map());

    UpdateLightCullingUBO();
}
void Renderer::UpdateLightCullingUBO()
{

    m_compute.buffers.uniformBufferData.proj = m_camera.matrices.perspective;
    m_compute.buffers.uniformBufferData.view = m_camera.matrices.view;
    memcpy(m_compute.buffers.uniformBuffer.mapped, &m_compute.buffers.uniformBufferData, sizeof(LightCullingPass::UniformComputeCullingData));
}
void Renderer::SetupTileBasedLightingPass()
{
    m_compute.queueFamilyIndex = m_vulkanDevice->queueFamilyIndices.compute;

    PrepareLightCullingBuffers();

    vkGetDeviceQueue(m_device, m_compute.queueFamilyIndex, 0, &m_compute.queue);

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        // Binding 0 : lights list
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
        // Binding 1 : Uniform buffer
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
        // Tile Buffer
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
    };

    VkDescriptorSetLayoutCreateInfo descriptorLayout = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_compute.descriptorSetLayout));

    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_compute.descriptorSetLayout, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_compute.descriptorSet));

    std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
        // Binding 0 : light buffer
        Init::writeDescriptorSet(m_compute.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &PointLightBuffer.descriptor),
        // Binding 1 : Uniform buffer
        Init::writeDescriptorSet(m_compute.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &m_compute.buffers.uniformBuffer.descriptor),
        // tiles buffer
        Init::writeDescriptorSet(m_compute.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &m_compute.buffers.tiles.descriptor)};
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, NULL);

    // Pipeline
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&m_compute.descriptorSetLayout, 1);

    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_compute.pipelineLayout));
    VkComputePipelineCreateInfo computePipelineCreateInfo = Init::computePipelineCreateInfo(m_compute.pipelineLayout, 0);

    computePipelineCreateInfo.stage = LoadShader(Tool::GetShadersPath() + "Main/TileBasedLighting.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    VkSpecializationMapEntry entry{0, 0, sizeof(uint32_t)};
    VkSpecializationInfo specIF{1, &entry, sizeof(uint32_t), &m_compute.MAX_LIGHTS_PER_TILE};

    computePipelineCreateInfo.stage.pSpecializationInfo = &specIF;

    Tool::CheckResult(vkCreateComputePipelines(m_device, m_pipelineCache, 1, &computePipelineCreateInfo, nullptr, &m_compute.pipeline));

    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = m_compute.queueFamilyIndex;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    Tool::CheckResult(vkCreateCommandPool(m_device, &cmdPoolInfo, nullptr, &m_compute.commandPool));

    m_compute.commandBuffer = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, m_compute.commandPool);

    // Semaphore for compute & graphics sync
    VkSemaphoreCreateInfo semaphoreCreateInfo = Init::semaphoreCreateInfo();
    Tool::CheckResult(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_compute.semaphore));

    BuildTileBasedLightingCommandBuffer();
}
/// @brief 创建TileBasedLighting 的CommandBuffer
void Renderer::BuildTileBasedLightingCommandBuffer()
{
    VkCommandBufferBeginInfo cmdBufInfo = Init::commandBufferBeginInfo();

    Tool::CheckResult(vkBeginCommandBuffer(m_compute.commandBuffer, &cmdBufInfo));

    BeginDebugLabel(m_compute.commandBuffer, "Tile Based Lights Culling");
    // Add memory barrier to ensure that the (graphics) vertex shader has fetched attributes before compute starts to write to the buffer
    // acquire
    if (m_index.compute != m_index.graphics)
    {
        std::vector<VkBufferMemoryBarrier> bufferBarriers;

        VkBufferMemoryBarrier bufferBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                               nullptr,
                                               0,
                                               VK_ACCESS_SHADER_WRITE_BIT,
                                               m_index.graphics,
                                               m_index.compute,
                                               m_compute.buffers.tiles.buffer,
                                               0,
                                               m_compute.buffers.tiles.size};
        bufferBarriers.push_back(bufferBarrier);
        bufferBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                         nullptr,
                         0,
                         VK_ACCESS_SHADER_READ_BIT,
                         m_index.graphics,
                         m_index.compute,
                         PointLightBuffer.buffer,
                         0,
                         PointLightBuffer.size};

        vkCmdPipelineBarrier(m_compute.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                             bufferBarriers.size(), bufferBarriers.data(), 0, nullptr);
    }

    // Dispatch the compute job
    vkCmdBindPipeline(m_compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute.pipeline);
    vkCmdBindDescriptorSets(m_compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute.pipelineLayout, 0, 1, &m_compute.descriptorSet, 0, 0);
    vkCmdDispatch(m_compute.commandBuffer, m_width / m_compute.tileSize / 8, m_height / m_compute.tileSize / 8, 1);

    // Add barrier to ensure that compute shader has finished writing to the buffer
    // Without this the (rendering) vertex shader may display incomplete results (partial data from last frame)
    // release
    if (m_index.compute != m_index.graphics)
    {
        std::vector<VkBufferMemoryBarrier> bufferBarriers;
        VkBufferMemoryBarrier bufferBarrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,  0, m_index.compute, m_index.graphics,
            m_compute.buffers.tiles.buffer,          0,       m_compute.buffers.tiles.size};
        bufferBarriers.push_back(bufferBarrier);

        bufferBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                         nullptr,
                         VK_ACCESS_SHADER_READ_BIT,
                         0,
                         m_index.compute,
                         m_index.graphics,
                         PointLightBuffer.buffer,
                         0,
                         PointLightBuffer.size};
        vkCmdPipelineBarrier(m_compute.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr,
                             bufferBarriers.size(), bufferBarriers.data(), 0, nullptr);
    }
    EndDebugLabel(m_compute.commandBuffer);
    vkEndCommandBuffer(m_compute.commandBuffer);
}

void Renderer::SetupSSAOPass()
{
    m_SSAOPass.frameBuffer = new FramebufferManager(m_vulkanDevice);
    m_SSAOPass.frameBuffer->width = m_width;
    m_SSAOPass.frameBuffer->height = m_height;

    AttachmentCreateInfo attachmentCI{};
    attachmentCI.width = m_SSAOPass.frameBuffer->width;
    attachmentCI.height = m_SSAOPass.frameBuffer->height;
    attachmentCI.layerCount = 1;
    attachmentCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    attachmentCI.format = VK_FORMAT_R32G32B32A32_SFLOAT;

    m_SSAOPass.frameBuffer->AddAttachment(attachmentCI);

    Tool::CheckResult(m_SSAOPass.frameBuffer->CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    Tool::CheckResult(m_SSAOPass.frameBuffer->CreateRenderPass());
}
void Renderer::CreateBuffersSSAO()
{
    m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 &m_SSAOPass.buffers.ubo, sizeof(SSAOPass::CBufferDesc));
    Tool::CheckResult(m_SSAOPass.buffers.ubo.Map());
    UpdateCBufferSSAO();
    // GenerateNoiseTextureSSAO();
    GenerateSampleKernel();
}
void Renderer::UpdateCBufferSSAO()
{
    m_SSAOPass.buffers.uboData.sampleNum = Settings.AOSetting.NumSamples;
    m_SSAOPass.buffers.uboData.radius = Settings.AOSetting.Radius;
    m_SSAOPass.buffers.uboData.scale = float(m_height) / float(m_blueNoise.height);
    memcpy(m_SSAOPass.buffers.ubo.mapped, &m_SSAOPass.buffers.uboData, sizeof(SSAOPass::CBufferDesc));
}
void Renderer::AllocateDescriptorSetSSAO()
{
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{
        // Binding 0: fragment uniform buffer(Camera Matrix)
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        // world pos
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
        // normal
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
        // depth
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
        // noise texture
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
        // kernels
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
        // Camera
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 6),
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_SSAOPass.setLayout));

    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_SSAOPass.setLayout, 1);
    // Deferred composition
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_SSAOPass.set));

    VkDescriptorImageInfo texDescriptorPosition = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorNormal = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[1].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorDepth = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[4].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    std::vector<VkWriteDescriptorSet> writeDescriptorSets{
        // Binding 0: fragment uniform buffer(Camera Matrix)
        Init::writeDescriptorSet(m_SSAOPass.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_SSAOPass.buffers.ubo.descriptor),
        Init::writeDescriptorSet(m_SSAOPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorPosition),
        Init::writeDescriptorSet(m_SSAOPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorNormal),
        Init::writeDescriptorSet(m_SSAOPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texDescriptorDepth),
        Init::writeDescriptorSet(m_SSAOPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &m_blueNoise.descriptor),
        Init::writeDescriptorSet(m_SSAOPass.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &m_SSAOPass.buffers.kernels.descriptor),
        Init::writeDescriptorSet(m_SSAOPass.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6, &m_sharedBuffers.ConstBufferCamera.descriptor)};
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
void Renderer::PreparePipelineSSAO()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCI = Init::pipelineLayoutCreateInfo(&m_SSAOPass.setLayout, 1);

    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &m_SSAOPass.pipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendStateCI = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI =
        Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportStateCI = Init::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleStateCI = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCI = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Main/SSAO.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Main/SSAO.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineVertexInputStateCreateInfo emptyInputState = Init::pipelineVertexInputStateCreateInfo();

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_SSAOPass.pipelineLayout, m_SSAOPass.frameBuffer->renderPass, 0);
    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pRasterizationState = &rasterizationStateCI;
    pipelineCI.pColorBlendState = &colorBlendStateCI;
    pipelineCI.pMultisampleState = &multisampleStateCI;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pDepthStencilState = &depthStencilStateCI;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &emptyInputState;

    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_SSAOPass.pipeline));
}
void Renderer::SetupPasses()
{
    SetupCSMPass();
    SetupSSAOPass();
    SetupSdfAOPass();
    SetupHBAOPass();
    SetupGTAOPass();
    SetupCBFPass();
    SetupLightingPass();
    if (m_shadowPointLightCount != 0)
    {
        SetupPassShadowOmni();
    }
}
void Renderer::AllocateDescriptorSets()
{

    AllocateDescriptorSetCSM();
    AllocateDescriptorSetSdfAO();
    AllocateDescriptorSetSSAO();
    AllocateDescriptorSetHBAO();
    AllocateDescriptorSetGTAO();
    AllocateDescriptorSetCBF();
    AllocateDescriptorSetLighting();
    AllocateDescriptorSetSkyBox();
}
void Renderer::UpdateBuffers()
{
    UpdateCBufferSSAO();
    UpdateCameraInfos();
    UpdateUBOCSM();
    // UpdateCBufferLighting();
}
void Renderer::CreateBuffers()
{
    CreateBuffersPointLights();
    CreateBuffersDirLights();
    CreateBuffersSpotLight();
    CreateBufferCameraInfos();

    CreateBuffersLighting();
    // CreateBuffersCSM();
    CreateBuffersSdfAO();
    CreateBuffersSSAO();
    CreateBuffersHBAO();
    CreateBuffersGTAO();
    CreateBuffersCBF();
}
void Renderer::LoadDebugUtilsFunctions()
{
    m_vkCmdBeginDebugUtilsLabelEXT =
        reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(m_device, "vkCmdBeginDebugUtilsLabelEXT"));

    m_vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(m_device, "vkCmdEndDebugUtilsLabelEXT"));
}
void Renderer::BeginDebugLabel(VkCommandBuffer cmd, const char* name, float r, float g, float b, float a)
{
    if (!m_vkCmdBeginDebugUtilsLabelEXT)
        return;

    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0] = r;
    label.color[1] = g;
    label.color[2] = b;
    label.color[3] = a;

    m_vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
}

void Renderer::EndDebugLabel(VkCommandBuffer cmd)
{
    if (m_vkCmdEndDebugUtilsLabelEXT)
    {
        m_vkCmdEndDebugUtilsLabelEXT(cmd);
    }
}
void Renderer::GenerateNoiseTextureSSAO()
{
    const uint32_t noiseTexWidth = 8;
    const uint32_t noiseTexHeight = 8;
    const VkFormat noiseTexFormat = VK_FORMAT_R8G8B8A8_UNORM;

    // 生成随机方向数据
    std::vector<uint32_t> noiseTexData(noiseTexWidth * noiseTexHeight);
    for (uint32_t i = 0; i < noiseTexWidth * noiseTexHeight; ++i)
    {
        // 在 [-1,1] 区间内生成一个二维随机向量（XY 平面方向）
        glm::vec2 dir = glm::normalize(glm::linearRand(glm::vec2(-1.0f), glm::vec2(1.0f)));
        glm::vec4 packedDir = glm::vec4(dir.x * 0.5f + 0.5f, 0.0f, dir.y * 0.5f + 0.5f, 1.0f); // 转为 [0,1] 区间
        noiseTexData[i] = glm::packUnorm4x8(
            packedDir); // 打包为 RGBA8 把 4 个 [0,1] 区间的 float 数字，分别映射成 0~255 的 8位无符号整数（uint8），然后按顺序打包成一个 uint32_t
    }

    m_SSAOPass.noiseTexture.FromBuffer(noiseTexData.data(), noiseTexData.size() * sizeof(uint32_t), noiseTexFormat, noiseTexWidth, noiseTexHeight,
                                       m_vulkanDevice,         // VulkanDevice*
                                       m_queues.graphicsQueue, // 拷贝队列
                                       VK_FILTER_NEAREST,      // 使用 nearest 保持清晰
                                       VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
void Renderer::CreateDefaultTextures()
{
    const uint32_t noiseTexWidth = 1;
    const uint32_t noiseTexHeight = 1;
    const VkFormat noiseTexFormat = VK_FORMAT_R8_UNORM;

    unsigned char* white = new unsigned char;
    unsigned char* black = new unsigned char;
    // unsigned char* white2 = new unsigned char[8];
    memset(white, 1.0f, 1);
    memset(black, 0.0f, 1);
    // memset(white2, 1.0f, 8);

    m_defaultTextures.Black.FromBuffer(white, 1 * sizeof(uint8_t), noiseTexFormat, noiseTexWidth, noiseTexHeight,
                                       m_vulkanDevice,         // VulkanDevice*
                                       m_queues.graphicsQueue, // 拷贝队列
                                       VK_FILTER_NEAREST,      // 使用 nearest 保持清晰
                                       VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_defaultTextures.White.FromBuffer(white, 1 * sizeof(uint8_t), noiseTexFormat, noiseTexWidth, noiseTexHeight,
                                       m_vulkanDevice,         // VulkanDevice*
                                       m_queues.graphicsQueue, // 拷贝队列
                                       VK_FILTER_NEAREST,      // 使用 nearest 保持清晰
                                       VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // m_defaultTextures.WhiteArray.FromBuffer(
    //	white2,
    //	2 * sizeof(uint8_t),
    //	VK_FORMAT_D16_UNORM,
    //	noiseTexWidth,
    //	noiseTexHeight,
    //	2,
    //	m_vulkanDevice,
    //	m_queues.graphicsQueue,
    //	VK_FILTER_NEAREST,
    //	VK_IMAGE_USAGE_SAMPLED_BIT,
    //	VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    //);

    // delete[] white2;
    delete white;
    delete black;
}
void Renderer::GenerateSampleKernel()
{
    std::vector<glm::vec4> kernels;
    uint32_t kernelSize = 32;
    kernels.reserve(kernelSize);
    for (uint32_t i = 0; i < kernelSize; ++i)
    {
        glm::vec3 sample;
        sample = glm::normalize(glm::linearRand(glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f)));

        float scale = (float)i / kernelSize;
        scale = glm::mix(0.2f, 1.0f, scale * scale);

        sample *= scale;

        kernels.emplace_back(glm::vec4(sample, 0.0f));
    }

    // for (auto& sample : kernels)
    VkDeviceSize bufferSize{kernelSize * sizeof(glm::vec4)};

    Buffer stagingBuffer;

    m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 &stagingBuffer, bufferSize, kernels.data());

    m_vulkanDevice->CreateBuffer(
        // The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m_SSAOPass.buffers.kernels,
        bufferSize);

    VkCommandBuffer copyCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, m_SSAOPass.buffers.kernels.buffer, 1, &copyRegion);

    m_vulkanDevice->FlushCommandBuffer(copyCmd, m_queues.graphicsQueue, true);

    stagingBuffer.Destroy();
}
void Renderer::CreateStorageBuffer(VkDeviceSize bufferSize)
{
    ;
}
void Renderer::CreateBufferCameraInfos()
{
    m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 &m_sharedBuffers.ConstBufferCamera, sizeof(CameraInfos));
    Tool::CheckResult(m_sharedBuffers.ConstBufferCamera.Map());

    m_cameraInfosData.zNear = m_camera.znear;
    m_cameraInfosData.zFar = m_camera.zfar;
    m_cameraInfosData.screenSize = glm::vec2(float(m_width), float(m_height));
    m_cameraInfosData.invScreenSize = glm::vec2(1.0f / float(m_width), 1.0f / float(m_height));
    UpdateCameraInfos();
}
void Renderer::UpdateCameraInfos()
{
    m_cameraInfosData.view = m_camera.matrices.view;
    m_cameraInfosData.proj = m_camera.matrices.perspective;
    m_cameraInfosData.projView = m_camera.matrices.perspective * m_camera.matrices.view;
    m_cameraInfosData.invView = glm::inverse(m_camera.matrices.view);
    m_cameraInfosData.invProj = glm::inverse(m_camera.matrices.perspective);
    m_cameraInfosData.invProjView = glm::inverse(m_cameraInfosData.projView);
    m_cameraInfosData.cameraWorldPos = glm::vec4(m_camera.position * glm::vec3(-1.0f, 1.0f, -1.0f), 1.0f);
    memcpy(m_sharedBuffers.ConstBufferCamera.mapped, &m_cameraInfosData, sizeof(CameraInfos));
}

// void Renderer::CreateLights()
//{
//	// directional lights
//	DirectionalLight light;
//	light.castShadow = 1;
//	light.color = glm::vec3(1.0f, 0.95f, 0.85f);
//	light.direction = glm::vec3(-1.0f, -1.0f, -1.0f);
//	light.intensity = 1.0f;
//	light.pos = glm::vec3(20.0f, 20.0f, 20.0f);
//	m_lights.DirLights.push_back(light);
//
// }
void Renderer::UpdateCascades()
{
    int y = 0;
    for (int x = 0; x < m_lights.DirLights.size(); ++x)
    {
        if (m_lights.DirLights[x].castShadow)
        {
            float cascadeSplitLambda = 0.95f; // 混合系数 即0.95 按指数分割

            float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

            float nearClip = m_camera.getNearClip();
            float farClip = m_camera.getFarClip();
            float clipRange = farClip - nearClip;

            float minZ = nearClip;
            float maxZ = nearClip + clipRange;

            float range = maxZ - minZ; // far- near
            float ratio = maxZ / minZ; // far/near

            // Calculate split depths based on view camera frustum
            // 混合 均匀分割 和 指数分割 计算每一层的深度 0-1
            for (auto i = 0; i < SHADOW_MAP_CASCADE_COUNT; ++i)
            {
                float p = static_cast<float>(i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT); //
                float log = minZ * std::pow(ratio, p);
                float uniform = minZ + range * p; // 均匀深度
                float d = cascadeSplitLambda * (log - uniform) + uniform;
                cascadeSplits[i] = (d - nearClip) / clipRange; // 得到当前层的 深度
            }
            glm::mat4& invCam = m_cameraInfosData.invProjView;
            glm::vec3 lightDir = normalize(-m_lights.DirLights[x].pos);
            glm::mat4 vulkanClip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // Y 轴反转
                                             0.0f, 0.0f, 0.5f, 0.0f,                          // Z 从 [-1,1] -> [0,1]
                                             0.0f, 0.0f, 0.5f, 1.0f);
            // Calculate orthographic projection matrix for each cascade
            // 计算每一层的 正交投影矩阵
            float lastSplitDist = 0.0;
            for (auto i = 0; i < SHADOW_MAP_CASCADE_COUNT; ++i)
            {
                float splitDist = cascadeSplits[i];
                // 视锥体 的 八个角（六面体）
                glm::vec3 frustumCorners[8] = {
                    glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(-1.0f, -1.0f, 0.0f),
                    glm::vec3(-1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(1.0f, -1.0f, 1.0f), glm::vec3(-1.0f, -1.0f, 1.0f),
                };

                // Project frustum corners into world space

                for (uint32_t j = 0; j < 8; j++)
                {
                    glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
                    frustumCorners[j] = invCorner / invCorner.w; // 矩阵会引入 w的变化，来自透视投影矩阵（构造时即引入）
                }

                for (uint32_t j = 0; j < 4; j++)
                {
                    glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];     // 大视锥体的两个角点之间的距离
                    frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist); // 当前cascade 的视锥体 远处角点的 位置
                    frustumCorners[j] =
                        frustumCorners[j] + (dist * lastSplitDist); // 当前cascade 的视锥体 近处角点的 位置（即上一个级联远处角点位置）
                }
                // 计算 当前级联视锥体中心
                glm::vec3 frustumCenter = glm::vec3(0.0f);
                for (uint32_t j = 0; j < 8; j++)
                {
                    frustumCenter += frustumCorners[j];
                }
                frustumCenter /= 8.0f;

                // 得到 当前 级联 八个角点 到 级联中心的最远长度 ，从而得到级联包围求球半径
                // 具体作用最好画个图
                // 当时思考过来着
                float radius = 0.0f;
                for (uint32_t j = 0; j < 8; j++)
                {
                    float distance = glm::length(frustumCorners[j] - frustumCenter);
                    radius = glm::max(radius, distance);
                }
                // 对浮点数 radius 进行​​量化处理​​，将其调整为一个特定的精度（这里为 1/16 的倍数）
                // 以避免 shadow map 在相机移动时"抖动"或"游移（swimming）"。
                radius = std::ceil(radius * 16.0f) / 16.0f;

                glm::vec3 maxExtents = glm::vec3(radius);
                glm::vec3 minExtents = -maxExtents;

                // pos, centre ,up
                glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
                glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

                // Store split distance and matrix in cascade
                m_CSMPass.Cascades[y][i].splitDepth = (m_camera.getNearClip() + splitDist * clipRange) * -1.0f;
                m_CSMPass.Cascades[y][i].viewProjMatrix = vulkanClip * lightOrthoMatrix * lightViewMatrix;

                lastSplitDist = cascadeSplits[i];
            }
            y += 1;
        }
    }
}
void Renderer::SetupCSMPass()
{
    VkFormat depthFormat = m_vulkanDevice->GetSupportedDepthFormat(true);

    /*
        Depth map renderpass
    */

    VkAttachmentDescription attachmentDescription{};
    attachmentDescription.format = depthFormat;
    attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 0;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;         // 原阶段是外部pass的 fragment shader（创建阶段？）
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;    // 目标阶段是读写这张深度图 的 early深度测试阶段
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;                    // 原阶段 对其做了 读取操作
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // 目标阶段要对 attachment 进行写入
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassCreateInfo = Init::renderPassCreateInfo();
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attachmentDescription;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassCreateInfo.pDependencies = dependencies.data();

    Tool::CheckResult(vkCreateRenderPass(m_device, &renderPassCreateInfo, nullptr, &m_CSMPass.renderPass));

    /*
        Layered depth image and views
    */
    // 为每一个有阴影的方向光创建image 和views
    int k = 0;
    for (int j = 0; j < m_lights.DirLights.size(); ++j)
    {
        if (m_lights.DirLights[j].castShadow == 1)
        {
            FramebufferAttachment depth;
            VkImageCreateInfo imageInfo = Init::imageCreateInfo();
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = SHADOWMAP_DIM;
            imageInfo.extent.height = SHADOWMAP_DIM;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = SHADOW_MAP_CASCADE_COUNT;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.format = depthFormat;
            imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Tool::CheckResult(vkCreateImage(m_device, &imageInfo, nullptr, &depth.image));
            VkMemoryAllocateInfo memAlloc = Init::memoryAllocateInfo();
            VkMemoryRequirements memReqs;
            vkGetImageMemoryRequirements(m_device, depth.image, &memReqs);
            memAlloc.allocationSize = memReqs.size;
            memAlloc.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Tool::CheckResult(vkAllocateMemory(m_device, &memAlloc, nullptr, &depth.memory));
            Tool::CheckResult(vkBindImageMemory(m_device, depth.image, depth.memory, 0));
            // Full depth map view (all layers)
            VkImageViewCreateInfo viewInfo = Init::imageViewCreateInfo();
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            viewInfo.format = depthFormat;
            viewInfo.subresourceRange = {};
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = SHADOW_MAP_CASCADE_COUNT;
            viewInfo.image = depth.image;
            Tool::CheckResult(vkCreateImageView(m_device, &viewInfo, nullptr, &depth.view));
            // std::array<Cascade, SHADOW_MAP_CASCADE_COUNT> cascades;
            //  One image view and framebuffer per cascade
            for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
            {
                // Image view for this cascade's layer (inside the depth map)
                // This view is used to render to that specific depth image layer
                VkImageViewCreateInfo viewInfo = Init::imageViewCreateInfo();
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                viewInfo.format = depthFormat;
                viewInfo.subresourceRange = {};
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                viewInfo.subresourceRange.baseMipLevel = 0;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.baseArrayLayer = i;
                viewInfo.subresourceRange.layerCount = 1;
                viewInfo.image = depth.image;
                Tool::CheckResult(vkCreateImageView(m_device, &viewInfo, nullptr, &m_CSMPass.Cascades[k][i].view));
                // FramebufferManager
                VkFramebufferCreateInfo framebufferInfo = Init::framebufferCreateInfo();
                framebufferInfo.renderPass = m_CSMPass.renderPass;
                framebufferInfo.attachmentCount = 1;
                framebufferInfo.pAttachments = &m_CSMPass.Cascades[k][i].view;
                framebufferInfo.width = SHADOWMAP_DIM;
                framebufferInfo.height = SHADOWMAP_DIM;
                framebufferInfo.layers = 1;
                Tool::CheckResult(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_CSMPass.Cascades[k][i].frameBuffer));
            }

            VkSamplerCreateInfo sampler = Init::samplerCreateInfo();
            sampler.magFilter = VK_FILTER_LINEAR;
            sampler.minFilter = VK_FILTER_LINEAR;
            sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler.addressModeV = sampler.addressModeU;
            sampler.addressModeW = sampler.addressModeU;
            sampler.mipLodBias = 0.0f;
            sampler.maxAnisotropy = 1.0f;
            sampler.minLod = 0.0f;
            sampler.maxLod = 1.0f;
            sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            Tool::CheckResult(vkCreateSampler(m_device, &sampler, nullptr, &depth.sampler));

            VkCommandBuffer cmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
            VkImageSubresourceRange subresourceRange = {};
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.levelCount = 1;
            subresourceRange.baseArrayLayer = 0;
            subresourceRange.layerCount = 4; // 级联层数

            Tool::SetImageLayout(cmd, depth.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, subresourceRange);
            m_vulkanDevice->FlushCommandBuffer(cmd, m_queues.graphicsQueue);
            m_CSMPass.Depths.push_back(depth);
            k += 1;
        }
    }
}

// void Renderer::CreateBuffersCSM()
//{
//	// Cascade matrices
//	Tool::CheckResult(m_vulkanDevice->CreateBuffer(
//		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//		&m_CSMPass.buffers.cascadeViewProjMatricesBuffer,
//		sizeof(glm::mat4) * SHADOW_MAP_CASCADE_COUNT));
//
//	// Scene uniform buffer blocks
//	// Lighting Pass buffer
//	//Tool::CheckResult(m_vulkanDevice->CreateBuffer(
//	//	VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//	//	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//	//	&uniformBuffers.VS,
//	//	sizeof(uboVS)));
//	//
//	//Tool::CheckResult(m_vulkanDevice->CreateBuffer(
//	//	VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//	//	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//	//	&m_CSMPass.buffers.uboFS,
//	//	sizeof(UBOFS)));
//
//	// Map persistent
//	Tool::CheckResult(m_CSMPass.buffers.cascadeViewProjMatricesBuffer.Map());
//	//Tool::CheckResult(m_CSMPass.buffers.uboFS.Map());
//	//CreateLights();
//	UpdateUBOCSM();
// }
void Renderer::UpdateUBOCSM()
{
    UpdateCascades();
    for (int j = 0; j < m_shadowDirLightCount; ++j)
    {
        for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; ++i)
        {
            m_CSMPass.CascadeData[j].cascadeViewProjMatrices[i] = m_CSMPass.Cascades[j][i].viewProjMatrix;
        }
    }

    memcpy(m_CSMPass.buffers.cascadeViewProjMatricesBuffer.mapped, m_CSMPass.CascadeData.data(),
           m_shadowDirLightCount * sizeof(CSMPass::CascadeDataDesc));

    // for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
    //{
    //	uboFS.cascadeSplits[i] = m_CSMPass.cascades[i].splitDepth;
    // }
    // uboFS.inverseViewMat = glm::inverse(camera.matrices.view);
    // uboFS.lightDir = normalize(-lightPos);
    // uboFS.colorCascades = colorCascades;
    // memcpy(uniformBuffers.FS.mapped, &uboFS, sizeof(uboFS));
}
void Renderer::AllocateDescriptorSetCSM()
{
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{
        // Binding 0: cascade Matrics
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_CSMPass.setLayout));

    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_CSMPass.setLayout, 1);
    // Deferred composition
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_CSMPass.set));

    std::vector<VkWriteDescriptorSet> writeDescriptorSets{
        // Binding 0: fragment uniform buffer(Camera Matrix)
        Init::writeDescriptorSet(m_CSMPass.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_CSMPass.buffers.cascadeViewProjMatricesBuffer.descriptor),
    };
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
void Renderer::PreparePipelineCSM()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&m_CSMPass.setLayout, 1);
    VkPushConstantRange pushConstantRange = Init::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(CSMPass::PushBlock), 0);
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_CSMPass.pipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE, 0);
    VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(0, nullptr);
    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        Init::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 1> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_CSMPass.pipelineLayout, m_CSMPass.renderPass, 0);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position});
    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Main/Cascade.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    // shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Main/Cascade.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    //  No blend attachment states (no color attachments used)

    // Enable depth clamp (if available)
    rasterizationState.depthClampEnable = m_enabledFeatures.depthClamp;

    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_CSMPass.pipeline));
}

void Renderer::SetupHBAOPass()
{
    m_HBAOPass.FrameBuffer = new FramebufferManager(m_vulkanDevice);
    m_HBAOPass.FrameBuffer->width = m_width;
    m_HBAOPass.FrameBuffer->height = m_height;

    AttachmentCreateInfo attachmentCI{};
    attachmentCI.width = m_HBAOPass.FrameBuffer->width;
    attachmentCI.height = m_HBAOPass.FrameBuffer->height;
    attachmentCI.layerCount = 1;
    attachmentCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    attachmentCI.format = VK_FORMAT_R32G32B32A32_SFLOAT;

    m_HBAOPass.FrameBuffer->AddAttachment(attachmentCI);

    Tool::CheckResult(m_HBAOPass.FrameBuffer->CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    Tool::CheckResult(m_HBAOPass.FrameBuffer->CreateRenderPass());
}
void Renderer::CreateBuffersHBAO()
{
    m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 &m_HBAOPass.Buffers.ConstBuffer, sizeof(HBAOPass::ConstBufferDesc));
    Tool::CheckResult(m_HBAOPass.Buffers.ConstBuffer.Map());
    // GenerateNoiseTextureHBAO();

    // Init
    m_HBAOPass.ConstBufferData.Radius = config_->HBAO.Radius;
    m_HBAOPass.ConstBufferData.Radius2 = config_->HBAO.Radius * config_->HBAO.Radius;
    m_HBAOPass.ConstBufferData.NegInvR2 = -1.0f / m_HBAOPass.ConstBufferData.Radius2;
    m_HBAOPass.ConstBufferData.Scale = float(m_height) / float(m_blueNoise.height);
    m_HBAOPass.ConstBufferData.NumDirections = config_->HBAO.DirNum;
    m_HBAOPass.ConstBufferData.NumSteps = config_->HBAO.StepNum;
    m_HBAOPass.ConstBufferData.MaxRadiusPixels = 100.0f;
    m_HBAOPass.ConstBufferData.TanBias = glm::tan(glm::radians(30.0f));
    m_HBAOPass.ConstBufferData.Strength = 1.9f;
    m_HBAOPass.ConstBufferData.UVToViewA = glm::vec2(0);
    m_HBAOPass.ConstBufferData.UVToViewB = glm::vec2(0);
    m_HBAOPass.ConstBufferData.LinMAD = glm::vec2(0);

    memcpy(m_HBAOPass.Buffers.ConstBuffer.mapped, &m_HBAOPass.ConstBufferData, sizeof(HBAOPass::ConstBufferDesc));
}

void Renderer::UpdateCBufferHBAO()
{
    m_HBAOPass.ConstBufferData.NumDirections = Settings.AOSetting.NumDir;
    m_HBAOPass.ConstBufferData.NumSteps = Settings.AOSetting.NumSteps;
    m_HBAOPass.ConstBufferData.Radius = Settings.AOSetting.Radius;
    m_HBAOPass.ConstBufferData.Scale = m_width / m_HBAOPass.NoiseTex.width;
    m_HBAOPass.ConstBufferData.Strength = Settings.AOSetting.Strength;
    m_HBAOPass.ConstBufferData.TanBias = glm::tan(glm::radians(Settings.AOSetting.HBAOTangentBias));
    m_HBAOPass.ConstBufferData.MaxRadiusPixels = Settings.AOSetting.MaxRadiusPixels;
    memcpy(m_HBAOPass.Buffers.ConstBuffer.mapped, &m_HBAOPass.ConstBufferData, sizeof(HBAOPass::ConstBufferDesc));
}

void Renderer::GenerateNoiseTextureHBAO()
{
    const uint32_t NoiseTexWidth = 8;
    const uint32_t NoiseTexHeight = 8;
    const VkFormat NoiseTexFormat = VK_FORMAT_R8G8_UNORM;

    std::vector<uint16_t> noiseTexData(NoiseTexWidth * NoiseTexHeight);
    for (auto& data : noiseTexData)
    {
        float stepBias = glm::linearRand(0.6f, 1.0f);
        float dirBias = glm::linearRand(0.0f, 1.0f);
        glm::vec2 packedNoise = glm::vec2(dirBias, stepBias);
        data = glm::packUnorm2x8(packedNoise);
    }
    m_HBAOPass.NoiseTex.FromBuffer(noiseTexData.data(), noiseTexData.size() * sizeof(uint32_t), NoiseTexFormat, NoiseTexWidth, NoiseTexHeight,
                                   m_vulkanDevice, m_queues.graphicsQueue, VK_FILTER_NEAREST, VK_IMAGE_USAGE_SAMPLED_BIT,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
void Renderer::AllocateDescriptorSetHBAO()
{
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{
        // HBAO Constants
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        // CameraInfos
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
        // normal
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
        // depth
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
        // noise texture
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_HBAOPass.SetLayout));

    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_HBAOPass.SetLayout, 1);
    // Deferred composition
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_HBAOPass.Set));

    VkDescriptorImageInfo texDescriptorNormal = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[1].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorDepth = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[4].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    std::vector<VkWriteDescriptorSet> writeDescriptorSets{
        // Binding 0: fragment uniform buffer(Camera Matrix)
        Init::writeDescriptorSet(m_HBAOPass.Set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_HBAOPass.Buffers.ConstBuffer.descriptor),
        Init::writeDescriptorSet(m_HBAOPass.Set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &m_sharedBuffers.ConstBufferCamera.descriptor),
        Init::writeDescriptorSet(m_HBAOPass.Set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorNormal),
        Init::writeDescriptorSet(m_HBAOPass.Set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texDescriptorDepth),
        Init::writeDescriptorSet(m_HBAOPass.Set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &m_blueNoise.descriptor),
    };
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
void Renderer::PreparePipelineHBAO()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCI = Init::pipelineLayoutCreateInfo(&m_HBAOPass.SetLayout, 1);
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &m_HBAOPass.PipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE); // 因为是全屏pass
    VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendStateCI = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI =
        Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportStateCI = Init::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleStateCI = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCI = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Main/FullScreen.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Main/HBAORefine.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineVertexInputStateCreateInfo emptyInputState = Init::pipelineVertexInputStateCreateInfo();

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_HBAOPass.PipelineLayout, m_HBAOPass.FrameBuffer->renderPass, 0);

    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pRasterizationState = &rasterizationStateCI;
    pipelineCI.pColorBlendState = &colorBlendStateCI;
    pipelineCI.pMultisampleState = &multisampleStateCI;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pDepthStencilState = &depthStencilStateCI;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &emptyInputState;

    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_HBAOPass.Pipeline));
}
void Renderer::SetupGTAOPass()
{
    m_GTAOPass.FrameBuffer = new FramebufferManager(m_vulkanDevice);
    m_GTAOPass.FrameBuffer->width = m_width;
    m_GTAOPass.FrameBuffer->height = m_height;

    AttachmentCreateInfo attachmentCI{};
    attachmentCI.width = m_GTAOPass.FrameBuffer->width;
    attachmentCI.height = m_GTAOPass.FrameBuffer->height;
    attachmentCI.layerCount = 1;
    attachmentCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    attachmentCI.format = VK_FORMAT_R32G32B32A32_SFLOAT;

    m_GTAOPass.FrameBuffer->AddAttachment(attachmentCI);

    Tool::CheckResult(m_GTAOPass.FrameBuffer->CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    Tool::CheckResult(m_GTAOPass.FrameBuffer->CreateRenderPass());
}

void Renderer::UpdateCBufferGTAO()
{
    m_GTAOPass.ConstBufferData.NumDirections = Settings.AOSetting.NumDir;
    m_GTAOPass.ConstBufferData.NumSteps = Settings.AOSetting.NumSteps;
    m_GTAOPass.ConstBufferData.Radius = Settings.AOSetting.Radius;
    m_GTAOPass.ConstBufferData.MaxRadiusPixels = Settings.AOSetting.MaxRadiusPixels;
    m_GTAOPass.ConstBufferData.Scale = float(m_height) / float(m_blueNoise.height);
    m_GTAOPass.ConstBufferData.Strength = Settings.AOSetting.Strength;
    memcpy(m_GTAOPass.Buffers.ConstBuffer.mapped, &m_GTAOPass.ConstBufferData, sizeof(GTAOPass::ConstBufferDesc));
}

void Renderer::GenerateNoiseTextureGTAO()
{
    const uint32_t NoiseTexWidth = 8;
    const uint32_t NoiseTexHeight = 8;
    const VkFormat NoiseTexFormat = VK_FORMAT_R8G8_UNORM;

    std::vector<uint16_t> noiseTexData(NoiseTexWidth * NoiseTexHeight);
    for (auto& data : noiseTexData)
    {
        float stepBias = glm::linearRand(0.6f, 1.0f);
        float dirBias = glm::linearRand(0.0f, 1.0f);
        glm::vec2 packedNoise = glm::vec2(dirBias, stepBias);
        data = glm::packUnorm2x8(packedNoise);
    }
    m_GTAOPass.NoiseTex.FromBuffer(noiseTexData.data(), noiseTexData.size() * sizeof(uint32_t), NoiseTexFormat, NoiseTexWidth, NoiseTexHeight,
                                   m_vulkanDevice, m_queues.graphicsQueue, VK_FILTER_NEAREST, VK_IMAGE_USAGE_SAMPLED_BIT,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
void Renderer::AllocateDescriptorSetGTAO()
{
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{
        // GTAO Constants
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        // CameraInfos
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
        // normal
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
        // depth
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
        // noise texture
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_GTAOPass.SetLayout));

    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_GTAOPass.SetLayout, 1);
    // Deferred composition
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_GTAOPass.Set));

    VkDescriptorImageInfo texDescriptorNormal = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[1].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorDepth = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[4].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    std::vector<VkWriteDescriptorSet> writeDescriptorSets{
        // Binding 0: fragment uniform buffer(Camera Matrix)
        Init::writeDescriptorSet(m_GTAOPass.Set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_GTAOPass.Buffers.ConstBuffer.descriptor),
        Init::writeDescriptorSet(m_GTAOPass.Set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &m_sharedBuffers.ConstBufferCamera.descriptor),
        Init::writeDescriptorSet(m_GTAOPass.Set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorNormal),
        Init::writeDescriptorSet(m_GTAOPass.Set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texDescriptorDepth),
        Init::writeDescriptorSet(m_GTAOPass.Set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &m_blueNoise.descriptor)
        // Init::writeDescriptorSet(m_GTAOPass.Set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &m_GTAOPass.NoiseTex.descriptor),
    };
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
void Renderer::PreparePipelineGTAO()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCI = Init::pipelineLayoutCreateInfo(&m_GTAOPass.SetLayout, 1);
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &m_GTAOPass.PipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE); // 因为是全屏pass
    VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendStateCI = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI =
        Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportStateCI = Init::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleStateCI = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCI = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Main/FullScreen.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Main/GTAORefine.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineVertexInputStateCreateInfo emptyInputState = Init::pipelineVertexInputStateCreateInfo();

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_GTAOPass.PipelineLayout, m_GTAOPass.FrameBuffer->renderPass, 0);

    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pRasterizationState = &rasterizationStateCI;
    pipelineCI.pColorBlendState = &colorBlendStateCI;
    pipelineCI.pMultisampleState = &multisampleStateCI;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pDepthStencilState = &depthStencilStateCI;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &emptyInputState;

    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_GTAOPass.Pipeline));
}
void Renderer::CreateBuffersGTAO()
{
    m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 &m_GTAOPass.Buffers.ConstBuffer, sizeof(GTAOPass::ConstBufferDesc));
    Tool::CheckResult(m_GTAOPass.Buffers.ConstBuffer.Map());
    // GenerateNoiseTextureGTAO();

    // Init
    m_GTAOPass.ConstBufferData.NumDirections = 8;
    m_GTAOPass.ConstBufferData.NumSteps = 6;
    m_GTAOPass.ConstBufferData.Radius = 0.1;
    m_GTAOPass.ConstBufferData.Scale = float(m_height) / float(m_blueNoise.height);
    m_GTAOPass.ConstBufferData.Strength = 1.0f;
    m_GTAOPass.ConstBufferData.MaxRadiusPixels = 100.0f;
    memcpy(m_GTAOPass.Buffers.ConstBuffer.mapped, &m_GTAOPass.ConstBufferData, sizeof(GTAOPass::ConstBufferDesc));
}

void Renderer::SetupCBFPass()
{
    m_CBFPass.FrameBufferX = new FramebufferManager(m_vulkanDevice);
    m_CBFPass.FrameBufferX->width = m_width;
    m_CBFPass.FrameBufferX->height = m_height;

    AttachmentCreateInfo attachmentCI{};
    attachmentCI.width = m_CBFPass.FrameBufferX->width;
    attachmentCI.height = m_CBFPass.FrameBufferX->height;
    attachmentCI.layerCount = 1;
    attachmentCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    attachmentCI.format = VK_FORMAT_R32G32B32A32_SFLOAT;

    m_CBFPass.FrameBufferX->AddAttachment(attachmentCI);

    Tool::CheckResult(m_CBFPass.FrameBufferX->CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    Tool::CheckResult(m_CBFPass.FrameBufferX->CreateRenderPass());

    m_CBFPass.FrameBufferY = new FramebufferManager(m_vulkanDevice);
    m_CBFPass.FrameBufferY->width = m_width;
    m_CBFPass.FrameBufferY->height = m_height;

    m_CBFPass.FrameBufferY->AddAttachment(attachmentCI);

    Tool::CheckResult(m_CBFPass.FrameBufferY->CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    Tool::CheckResult(m_CBFPass.FrameBufferY->CreateRenderPass());
}
void Renderer::CreateBuffersCBF()
{
    m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 &m_CBFPass.Buffers.ConstBuffer, sizeof(CrossBilateralFilterPass::ConstBufferDesc));
    Tool::CheckResult(m_CBFPass.Buffers.ConstBuffer.Map());
    // GenerateNoiseTextureGTAO();

    // Init
    m_CBFPass.ConstBufferData.ZFactor = 30.0f;
    m_CBFPass.ConstBufferData.KernelRadius = 8;
    m_CBFPass.ConstBufferData.Res = glm::vec2(m_width, m_height);
    m_CBFPass.ConstBufferData.InvRes = glm::vec2(1.0f / m_width, 1.0f / m_height);
    memcpy(m_CBFPass.Buffers.ConstBuffer.mapped, &m_CBFPass.ConstBufferData, sizeof(CrossBilateralFilterPass::ConstBufferDesc));
}
void Renderer::UpdateDescriptorSetCBF()
{

    VkDescriptorImageInfo texDescriptorAOZ;
    if (Settings.AOSetting.UseAO != 0 && Settings.AOSetting.UseCBFBlur == true)
    {
        if (Settings.AOSetting.UseAO == 1)
        {
            texDescriptorAOZ = Init::descriptorImageInfo(m_SSAOPass.frameBuffer->sampler, m_SSAOPass.frameBuffer->attachments[0].view,
                                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        else if (Settings.AOSetting.UseAO == 2)
        {
            texDescriptorAOZ = Init::descriptorImageInfo(m_HBAOPass.FrameBuffer->sampler, m_HBAOPass.FrameBuffer->attachments[0].view,
                                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        else if (Settings.AOSetting.UseAO == 3)
        {
            texDescriptorAOZ = Init::descriptorImageInfo(m_GTAOPass.FrameBuffer->sampler, m_GTAOPass.FrameBuffer->attachments[0].view,
                                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        else if (Settings.AOSetting.UseAO == 4)
        {
            texDescriptorAOZ = Init::descriptorImageInfo(sdfAOPass_.frameBuffer->sampler, sdfAOPass_.frameBuffer->attachments[0].view,
                                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        VkWriteDescriptorSet write = Init::writeDescriptorSet(m_CBFPass.SetX, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorAOZ);
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }
}
void Renderer::AllocateDescriptorSetCBF()
{
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        // AO and Z
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)};
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_CBFPass.SetLayout));

    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_CBFPass.SetLayout, 1);
    // Deferred composition
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_CBFPass.SetX));
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &m_CBFPass.SetY));

    VkDescriptorImageInfo texDescriptorAOZ = Init::descriptorImageInfo(m_SSAOPass.frameBuffer->sampler, m_SSAOPass.frameBuffer->attachments[0].view,
                                                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorAOZX = Init::descriptorImageInfo(m_CBFPass.FrameBufferX->sampler, m_CBFPass.FrameBufferX->attachments[0].view,
                                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    std::vector<VkWriteDescriptorSet> writeDescriptorSets{
        // Binding 0: fragment uniform buffer(Camera Matrix)
        Init::writeDescriptorSet(m_CBFPass.SetX, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_CBFPass.Buffers.ConstBuffer.descriptor),
        Init::writeDescriptorSet(m_CBFPass.SetX, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorAOZ),
    };
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    writeDescriptorSets = {
        // Binding 0: fragment uniform buffer(Camera Matrix)
        Init::writeDescriptorSet(m_CBFPass.SetY, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_CBFPass.Buffers.ConstBuffer.descriptor),
        Init::writeDescriptorSet(m_CBFPass.SetY, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorAOZX),
    };
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
void Renderer::PreparePipelineCBF()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCI = Init::pipelineLayoutCreateInfo(&m_CBFPass.SetLayout, 1);
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &m_CBFPass.PipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE); // 因为是全屏pass
    VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendStateCI = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI =
        Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportStateCI = Init::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleStateCI = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCI = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Main/FullScreen.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "AO/BlurX.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineVertexInputStateCreateInfo emptyInputState = Init::pipelineVertexInputStateCreateInfo();

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_CBFPass.PipelineLayout, m_CBFPass.FrameBufferX->renderPass, 0);
    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pRasterizationState = &rasterizationStateCI;
    pipelineCI.pColorBlendState = &colorBlendStateCI;
    pipelineCI.pMultisampleState = &multisampleStateCI;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pDepthStencilState = &depthStencilStateCI;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &emptyInputState;
    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_CBFPass.PipelineX));

    pipelineCI.renderPass = m_CBFPass.FrameBufferY->renderPass;
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "AO/BlurY.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_CBFPass.PipelineY));
}
void Renderer::UpdateCBufferCBF()
{
    m_CBFPass.ConstBufferData.ZFactor = Settings.AOSetting.ZStrength;
    m_CBFPass.ConstBufferData.KernelRadius = Settings.AOSetting.BlurKernelRadius;
    m_CBFPass.ConstBufferData.Res = glm::vec2(m_width, m_height);
    m_CBFPass.ConstBufferData.InvRes = glm::vec2(1.0f / m_width, 1.0f / m_height);
    memcpy(m_CBFPass.Buffers.ConstBuffer.mapped, &m_CBFPass.ConstBufferData, sizeof(CrossBilateralFilterPass::ConstBufferDesc));
}
bool Renderer::ReadFinalVoxelStateTextureToCPU_Staging()
{
    const uint32_t gridSize = config_->Sdf.Resolution;
    const VkDeviceSize imageSize = gridSize * gridSize * gridSize * sizeof(uint8_t);

    voxelData_.resize(imageSize);

    // 1. 创建CPU可见的中转缓冲区
    Buffer stagingBuffer;
    VkResult result = m_vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, imageSize);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create staging buffer.");
    }

    // 2. 创建一次性命令缓冲，录制拷贝指令
    VkCommandBuffer commandBuffer = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, m_commandPool, true);

    // 3. 插入图像内存屏障，将布局从VK_IMAGE_LAYOUT_GENERAL转换为VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    VkImageMemoryBarrier barrierToTransferSrc{};
    barrierToTransferSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToTransferSrc.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrierToTransferSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrierToTransferSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToTransferSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToTransferSrc.image = m_voxelizationPass.finalVoxelStateTexture.image;
    barrierToTransferSrc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierToTransferSrc.subresourceRange.baseMipLevel = 0;
    barrierToTransferSrc.subresourceRange.levelCount = 1;
    barrierToTransferSrc.subresourceRange.baseArrayLayer = 0;
    barrierToTransferSrc.subresourceRange.layerCount = 1;
    barrierToTransferSrc.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrierToTransferSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrierToTransferSrc);

    // 4. 复制图像到缓冲区
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {gridSize, gridSize, gridSize};

    vkCmdCopyImageToBuffer(commandBuffer, m_voxelizationPass.finalVoxelStateTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.buffer,
                           1, &region);

    // 5. 插入图像内存屏障，将布局从VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL恢复为VK_IMAGE_LAYOUT_GENERAL
    VkImageMemoryBarrier barrierToGeneral{};
    barrierToGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrierToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrierToGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToGeneral.image = m_voxelizationPass.finalVoxelStateTexture.image;
    barrierToGeneral.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierToGeneral.subresourceRange.baseMipLevel = 0;
    barrierToGeneral.subresourceRange.levelCount = 1;
    barrierToGeneral.subresourceRange.baseArrayLayer = 0;
    barrierToGeneral.subresourceRange.layerCount = 1;
    barrierToGeneral.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrierToGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrierToGeneral);

    // 6. 提交命令缓冲并等待完成
    m_vulkanDevice->FlushCommandBuffer(commandBuffer, m_queues.graphicsQueue, m_commandPool);

    // 7. 映射缓冲区内存，复制数据到vector
    stagingBuffer.Map();
    memcpy(voxelData_.data(), stagingBuffer.mapped, static_cast<size_t>(imageSize));
    stagingBuffer.Unmap();

    // 8. 清理中转缓冲资源
    stagingBuffer.Destroy();

    return true;
}

/// @brief 初始化统一GPU管线资源 - 只创建一次，不在循环中调用
void Renderer::InitializeUnifiedGPUPipelineResources()
{
    if (m_unifiedGPUPipeline.resourcesInitialized)
        return;
    // 2. 创建专用command pool和command buffer
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_vulkanDevice->queueFamilyIndices.graphics;
    Tool::CheckResult(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_unifiedGPUPipeline.commandPool));

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_unifiedGPUPipeline.commandPool;
    allocInfo.commandBufferCount = 1;
    Tool::CheckResult(vkAllocateCommandBuffers(m_device, &allocInfo, &m_unifiedGPUPipeline.commandBuffer));

    // 3. 创建同步对象
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // 初始状态为已完成
    Tool::CheckResult(vkCreateFence(m_device, &fenceInfo, nullptr, &m_unifiedGPUPipeline.executionFence));

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    Tool::CheckResult(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_unifiedGPUPipeline.completionSemaphore));

    m_unifiedGPUPipeline.resourcesInitialized = true;
    printf("Unified GPU Pipeline resources initialized\n");
    fflush(stdout);

    // 初始化阶段三和阶段四的资源
    printf("About to initialize Solid Node Selection resources...\n");
    fflush(stdout);
    InitializeAnalyticalNodeSelectionResource(); // analytical 的描述符集等

    printf("About to initialize Solid Node Selection B resources...\n");
    fflush(stdout);
    InitializeMultiviewNodeSelectionResource(); // multi-view depth SDF 的描述符集等


    // 初始化阶段四版本C：新的多视角深度SDF方案
    printf("Initializing MultiViewDepthSDF4C resources...\n");
    fflush(stdout);
    InitializeMultiViewDepthSDF4CResources();

    // 现在可以安全地更新SolidNodeSelection的descriptor set，因为：
    // 1. GPUMipmapOctree已经创建，mipmap纹理已经存在
    // 2. SolidNodeSelection的descriptor set已经创建
    printf("Updating AnalyticalSolidNodeSelection descriptor set with mipmap textures...\n");
    fflush(stdout);
    UpdateSolidNodeSelectionDescriptorSet();

    printf("Updating AnalyticalSolidNodeSelection B descriptor set with mipmap textures...\n");
    fflush(stdout);
    UpdateMultiviewNodeSelectionDescriptorSet();

    printf("About to initialize Analytical SDF Generation resources...\n");
    fflush(stdout);
    InitializeAnalyticalSDFGenerationResources();


    printf("All stage 3 and 4 resources initialized successfully\n");
    fflush(stdout);
}

void Renderer::InitializeAnalyticalNodeSelectionResource()
{
    // 创建Solid Node Buffer (存储筛选出的节点) - 需要HOST_VISIBLE以便CPU读取用于调试
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &analyticalNodeSelection_.solidNodeBuffer, sizeof(AnalyticalSolidNodeSelection::SolidNode) * AnalyticalSolidNodeSelection::MAX_SOLID_NODES));

    // 创建Counter Buffer (原子计数器) - 需要HOST_VISIBLE以便CPU清零
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &analyticalNodeSelection_.counterBuffer, sizeof(uint32_t)));

    // 初始化计数器为0
    uint32_t zero = 0;
    analyticalNodeSelection_.counterBuffer.Map();
    memcpy(analyticalNodeSelection_.counterBuffer.mapped, &zero, sizeof(uint32_t));
    analyticalNodeSelection_.counterBuffer.Unmap();

    // 创建描述符集布局
    // 这里需要注意, 128的输入应该只有6层, 256就应该有7层, 目前是硬编码, 后续可以改进
    // 对于analytical, 4x4x4是允许被使用的, 但是multiview 最多应该使用8x8x8, 4x4x4仅作为复杂度计算
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        // Binding 0: Counter buffer (atomic counter)
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        // Binding 1: Solid node buffer (output)
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        // Binding 2-7: Mipmap octree texture array 
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Level 0: basesize 128
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Level 1: 64
        {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Level 2: 32
        {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Level 3: 16
        {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Level 4: 8
        {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}  // Level 4: 4
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &analyticalNodeSelection_.descriptorSetLayout));

    // 创建管线布局 (支持Push Constants)
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(AnalyticalSolidNodeSelection::SolidNodeSelectionPushConstant); // modelCenter + halfSizeWithMargin = 16字节

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &analyticalNodeSelection_.descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &analyticalNodeSelection_.pipelineLayout));

    // 加载Compute Shader
    std::string shaderPath = Tool::GetShadersPath() + "Analytical/AnalyticalNodeSelection.Comp.spv";
    VkShaderModule shaderModule = Tool::LoadShader(shaderPath.c_str(), m_device);

    // 创建Compute Pipeline
    VkComputePipelineCreateInfo computePipelineInfo{};
    computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computePipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineInfo.stage.module = shaderModule;
    computePipelineInfo.stage.pName = "main";
    computePipelineInfo.layout = analyticalNodeSelection_.pipelineLayout;

    Tool::CheckResult(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &analyticalNodeSelection_.pipeline));

    // 清理shader module
    vkDestroyShaderModule(m_device, shaderModule, nullptr);

    // 创建描述符池
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},        // counter + solid node buffers
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6} // 5 mipmap textures (level 0-4)
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    VkDescriptorPool descriptorPool;
    Tool::CheckResult(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &descriptorPool));

    // 分配描述符集
    VkDescriptorSetAllocateInfo descriptorAllocInfo{};
    descriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocInfo.descriptorPool = descriptorPool;
    descriptorAllocInfo.descriptorSetCount = 1;
    descriptorAllocInfo.pSetLayouts = &analyticalNodeSelection_.descriptorSetLayout;

    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &descriptorAllocInfo, &analyticalNodeSelection_.descriptorSet));

    // 绑定资源到描述符集
    std::vector<VkWriteDescriptorSet> descriptorWrites;

    // Binding 0: Counter buffer
    VkDescriptorBufferInfo counterBufferInfo{};
    counterBufferInfo.buffer = analyticalNodeSelection_.counterBuffer.buffer;
    counterBufferInfo.offset = 0;
    counterBufferInfo.range = sizeof(uint32_t);

    VkWriteDescriptorSet counterWrite{};
    counterWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    counterWrite.dstSet = analyticalNodeSelection_.descriptorSet;
    counterWrite.dstBinding = 0;
    counterWrite.dstArrayElement = 0;
    counterWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    counterWrite.descriptorCount = 1;
    counterWrite.pBufferInfo = &counterBufferInfo;
    descriptorWrites.push_back(counterWrite);

    // Binding 1: Solid node buffer
    VkDescriptorBufferInfo solidNodeBufferInfo{};
    solidNodeBufferInfo.buffer = analyticalNodeSelection_.solidNodeBuffer.buffer;
    solidNodeBufferInfo.offset = 0;
    solidNodeBufferInfo.range = sizeof(AnalyticalSolidNodeSelection::SolidNode) * AnalyticalSolidNodeSelection::MAX_SOLID_NODES;

    VkWriteDescriptorSet solidNodeWrite{};
    solidNodeWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    solidNodeWrite.dstSet = analyticalNodeSelection_.descriptorSet;
    solidNodeWrite.dstBinding = 1;
    solidNodeWrite.dstArrayElement = 0;
    solidNodeWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    solidNodeWrite.descriptorCount = 1;
    solidNodeWrite.pBufferInfo = &solidNodeBufferInfo;
    descriptorWrites.push_back(solidNodeWrite);

    // NOTE: Bindings 2-9: Mipmap textures will be bound dynamically in UpdateSolidNodeSelectionDescriptorSet()
    // This is because GPUMipmapOctree textures are created after BuildFromVoxelTexture() is called

    // 更新描述符集
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    printf("Solid Node Selection resources initialized (descriptor set created)\n");
}

/// @brief 初始化阶段三版本B：自适应相机位置筛选资源
void Renderer::InitializeMultiviewNodeSelectionResource()
{
    // === Pass 1-3: 创建候选节点缓冲区 ===

    // 候选节点缓冲区 (最多1000个候选节点)
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &multiViewNodeSelection_.candidateNodesBuffer, sizeof(MultiViewSolidNodeSelection::SolidNode) * MultiViewSolidNodeSelection::MAX_CANDIDATE_NODES));

    // 候选节点计数缓冲区
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &multiViewNodeSelection_.candidateCountBuffer, sizeof(uint32_t)));

    // === Pass 4: 创建最终选择缓冲区 ===

    // 最终选中节点缓冲区 (最多10个)
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &multiViewNodeSelection_.selectedNodesBuffer, sizeof(MultiViewSolidNodeSelection::SolidNode) * config_->Sdf.MultiViewUsedCameraNum));

    // 最终节点计数缓冲区
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &multiViewNodeSelection_.selectedCountBuffer, sizeof(uint32_t)));

    // 初始化所有计数器为0
    uint32_t zero = 0;
    multiViewNodeSelection_.candidateCountBuffer.Map();
    memcpy(multiViewNodeSelection_.candidateCountBuffer.mapped, &zero, sizeof(uint32_t));
    multiViewNodeSelection_.candidateCountBuffer.Unmap();

    multiViewNodeSelection_.selectedCountBuffer.Map();
    memcpy(multiViewNodeSelection_.selectedCountBuffer.mapped, &zero, sizeof(uint32_t));
    multiViewNodeSelection_.selectedCountBuffer.Unmap();

    // === 创建收集管线 (Pass 1-3:) ===

    // 创建收集阶段的描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> collectionBindings = {
        // Binding 0: Candidate count buffer (RW)
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        // Binding 1: Candidate nodes buffer (RW)
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        // Binding 2-5: Mipmap octree textures (input)
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };

 
    VkDescriptorSetLayoutCreateInfo collectionLayoutInfo{};
    collectionLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    collectionLayoutInfo.bindingCount = static_cast<uint32_t>(collectionBindings.size());
    collectionLayoutInfo.pBindings = collectionBindings.data();
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &collectionLayoutInfo, nullptr, &multiViewNodeSelection_.collectionDescriptorSetLayout));

    struct {
        uint32_t BaseSize;
        uint32_t CurrentLevel;
    }pushConst;
    VkPushConstantRange pushConstantRange{Init::pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, sizeof(pushConst), 0)};

    // 创建收集管线布局
    VkPipelineLayoutCreateInfo collectionPipelineLayoutInfo{};
    collectionPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    collectionPipelineLayoutInfo.setLayoutCount = 1;
    collectionPipelineLayoutInfo.pSetLayouts = &multiViewNodeSelection_.collectionDescriptorSetLayout;
    collectionPipelineLayoutInfo.pushConstantRangeCount = 1;
    collectionPipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &collectionPipelineLayoutInfo, nullptr, &multiViewNodeSelection_.collectionPipelineLayout));

    // 创建收集管线
    std::string collectionShaderPath = Tool::GetShadersPath() + "MultiView/MultiViewNodeSelection.Comp.spv";
    VkShaderModule collectionShaderModule = Tool::LoadShader(collectionShaderPath.c_str(), m_device);

    VkComputePipelineCreateInfo collectionComputePipelineInfo{};
    collectionComputePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    collectionComputePipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    collectionComputePipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    collectionComputePipelineInfo.stage.module = collectionShaderModule;
    collectionComputePipelineInfo.stage.pName = "main";
    collectionComputePipelineInfo.layout = multiViewNodeSelection_.collectionPipelineLayout;

    Tool::CheckResult(
        vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &collectionComputePipelineInfo, nullptr, &multiViewNodeSelection_.collectionPipeline));
    vkDestroyShaderModule(m_device, collectionShaderModule, nullptr);


    // 分配收集阶段的描述符集
    VkDescriptorSetAllocateInfo collectionAllocInfo{
        Init::descriptorSetAllocateInfo(m_descriptorPool, &multiViewNodeSelection_.collectionDescriptorSetLayout, 1)};
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &collectionAllocInfo, &multiViewNodeSelection_.collectionDescriptorSet));

    // 绑定收集阶段的缓冲区
    std::vector<VkWriteDescriptorSet> collectionDescriptorWrites;

    // Binding 0: Candidate count buffer

    VkWriteDescriptorSet candidateCountWrite{};
    candidateCountWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    candidateCountWrite.dstSet = multiViewNodeSelection_.collectionDescriptorSet;
    candidateCountWrite.dstBinding = 0;
    candidateCountWrite.dstArrayElement = 0;
    candidateCountWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    candidateCountWrite.descriptorCount = 1;
    candidateCountWrite.pBufferInfo = &multiViewNodeSelection_.candidateCountBuffer.descriptor;
    collectionDescriptorWrites.push_back(candidateCountWrite);

    // Binding 1: Candidate nodes buffer
    VkWriteDescriptorSet candidateNodesWrite{};
    candidateNodesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    candidateNodesWrite.dstSet = multiViewNodeSelection_.collectionDescriptorSet;
    candidateNodesWrite.dstBinding = 1;
    candidateNodesWrite.dstArrayElement = 0;
    candidateNodesWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    candidateNodesWrite.descriptorCount = 1;
    candidateNodesWrite.pBufferInfo = &multiViewNodeSelection_.candidateNodesBuffer.descriptor;
    collectionDescriptorWrites.push_back(candidateNodesWrite);

    // 更新收集阶段描述符集（纹理绑定稍后在UpdateSolidNodeSelectionBDescriptorSet中处理）
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(collectionDescriptorWrites.size()), collectionDescriptorWrites.data(), 0, nullptr);

    // === Pass 4: 创建最终选择管线和描述符集 ===

    // 创建最终选择管线描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> finalSelectionBindings;

    // Binding 0: candidateCountBuffer (read-only)
    VkDescriptorSetLayoutBinding candidateCountFinalBinding{};
    candidateCountFinalBinding.binding = 0;
    candidateCountFinalBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    candidateCountFinalBinding.descriptorCount = 1;
    candidateCountFinalBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    finalSelectionBindings.push_back(candidateCountFinalBinding);

    // Binding 1: candidateNodesBuffer (read-only)
    VkDescriptorSetLayoutBinding candidateNodesFinalBinding{};
    candidateNodesFinalBinding.binding = 1;
    candidateNodesFinalBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    candidateNodesFinalBinding.descriptorCount = 1;
    candidateNodesFinalBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    finalSelectionBindings.push_back(candidateNodesFinalBinding);

    // Binding 2: finalCountBuffer (write)
    VkDescriptorSetLayoutBinding finalCountBinding{};
    finalCountBinding.binding = 2;
    finalCountBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    finalCountBinding.descriptorCount = 1;
    finalCountBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    finalSelectionBindings.push_back(finalCountBinding);

    // Binding 3: finalNodesBuffer (write)
    VkDescriptorSetLayoutBinding finalNodesBinding{};
    finalNodesBinding.binding = 3;
    finalNodesBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    finalNodesBinding.descriptorCount = 1;
    finalNodesBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    finalSelectionBindings.push_back(finalNodesBinding);

    VkDescriptorSetLayoutCreateInfo finalSelectionLayoutInfo{};
    finalSelectionLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    finalSelectionLayoutInfo.bindingCount = static_cast<uint32_t>(finalSelectionBindings.size());
    finalSelectionLayoutInfo.pBindings = finalSelectionBindings.data();
    Tool::CheckResult(
        vkCreateDescriptorSetLayout(m_device, &finalSelectionLayoutInfo, nullptr, &multiViewNodeSelection_.finalSelectionDescriptorSetLayout));

    // 创建最终选择管线布局
    VkPipelineLayoutCreateInfo finalSelectionPipelineLayoutInfo{};
    finalSelectionPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    finalSelectionPipelineLayoutInfo.setLayoutCount = 1;
    finalSelectionPipelineLayoutInfo.pSetLayouts = &multiViewNodeSelection_.finalSelectionDescriptorSetLayout;
    Tool::CheckResult(
        vkCreatePipelineLayout(m_device, &finalSelectionPipelineLayoutInfo, nullptr, &multiViewNodeSelection_.finalSelectionPipelineLayout));

    // 分配最终选择管线描述符集
    VkDescriptorSetAllocateInfo finalSelectionAllocInfo{};
    finalSelectionAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    finalSelectionAllocInfo.descriptorPool = m_descriptorPool;
    finalSelectionAllocInfo.descriptorSetCount = 1;
    finalSelectionAllocInfo.pSetLayouts = &multiViewNodeSelection_.finalSelectionDescriptorSetLayout;
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &finalSelectionAllocInfo, &multiViewNodeSelection_.finalSelectionDescriptorSet));

    // 创建最终选择管线
    std::string finalSelectionShaderPath = Tool::GetShadersPath() + "MultiView/MultiViewNodeSelectionFinal.Comp.spv";
    VkShaderModule finalSelectionShaderModule = Tool::LoadShader(finalSelectionShaderPath.c_str(), m_device);

    VkPipelineShaderStageCreateInfo finalSelectionShaderStageInfo{};
    finalSelectionShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    finalSelectionShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    finalSelectionShaderStageInfo.module = finalSelectionShaderModule;
    finalSelectionShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo finalSelectionComputePipelineInfo{};
    finalSelectionComputePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    finalSelectionComputePipelineInfo.layout = multiViewNodeSelection_.finalSelectionPipelineLayout;
    finalSelectionComputePipelineInfo.stage = finalSelectionShaderStageInfo;

    Tool::CheckResult(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &finalSelectionComputePipelineInfo, nullptr,
                                               &multiViewNodeSelection_.finalSelectionPipeline));
    vkDestroyShaderModule(m_device, finalSelectionShaderModule, nullptr);

    // 绑定最终选择描述符集
    std::vector<VkWriteDescriptorSet> finalSelectionDescriptorWrites;

    // Binding 0: candidateCountBuffer (read-only)

    VkWriteDescriptorSet candidateCountFinalWrite{};
    candidateCountFinalWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    candidateCountFinalWrite.dstSet = multiViewNodeSelection_.finalSelectionDescriptorSet;
    candidateCountFinalWrite.dstBinding = 0;
    candidateCountFinalWrite.dstArrayElement = 0;
    candidateCountFinalWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    candidateCountFinalWrite.descriptorCount = 1;
    candidateCountFinalWrite.pBufferInfo = &multiViewNodeSelection_.candidateCountBuffer.descriptor;
    finalSelectionDescriptorWrites.push_back(candidateCountFinalWrite);

    // Binding 1: candidateNodesBuffer (read-only)
    VkWriteDescriptorSet candidateNodesFinalWrite{};
    candidateNodesFinalWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    candidateNodesFinalWrite.dstSet = multiViewNodeSelection_.finalSelectionDescriptorSet;
    candidateNodesFinalWrite.dstBinding = 1;
    candidateNodesFinalWrite.dstArrayElement = 0;
    candidateNodesFinalWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    candidateNodesFinalWrite.descriptorCount = 1;
    candidateNodesFinalWrite.pBufferInfo = &multiViewNodeSelection_.candidateNodesBuffer.descriptor;
    finalSelectionDescriptorWrites.push_back(candidateNodesFinalWrite);

    // Binding 2: finalCountBuffer (selectedCountBuffer重用)
    VkWriteDescriptorSet finalCountWrite{};
    finalCountWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    finalCountWrite.dstSet = multiViewNodeSelection_.finalSelectionDescriptorSet;
    finalCountWrite.dstBinding = 2;
    finalCountWrite.dstArrayElement = 0;
    finalCountWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    finalCountWrite.descriptorCount = 1;
    finalCountWrite.pBufferInfo = &multiViewNodeSelection_.selectedCountBuffer.descriptor;
    finalSelectionDescriptorWrites.push_back(finalCountWrite);

    // Binding 3: finalNodesBuffer (selectedNodesBuffer重用)
    VkWriteDescriptorSet finalNodesWrite{};
    finalNodesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    finalNodesWrite.dstSet = multiViewNodeSelection_.finalSelectionDescriptorSet;
    finalNodesWrite.dstBinding = 3;
    finalNodesWrite.dstArrayElement = 0;
    finalNodesWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    finalNodesWrite.descriptorCount = 1;
    finalNodesWrite.pBufferInfo = &multiViewNodeSelection_.selectedNodesBuffer.descriptor;
    finalSelectionDescriptorWrites.push_back(finalNodesWrite);

    // 更新最终选择描述符集
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(finalSelectionDescriptorWrites.size()), finalSelectionDescriptorWrites.data(), 0, nullptr);

    printf("Solid Node Selection B resources initialized (multi-pass architecture with final selection)");
}

// Update descriptor set with mipmap textures for Solid Node Selection B
void Renderer::UpdateMultiviewNodeSelectionDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> descriptorWrites;
    std::vector<VkDescriptorImageInfo> imageInfos;

    // 只绑定前4个mipmap级别 (Levels 0-3: 32x32x32 to 4x4x4)
    const uint32_t maxUsedLevel = m_gpuMipmapOctree->GetMaxLevel();
    imageInfos.reserve(maxUsedLevel);
    descriptorWrites.reserve(maxUsedLevel);

    // 收集所有mipmap纹理的image info
    for (uint32_t level = 0; level <= maxUsedLevel; ++level)
    {
        VkImageView mipmapView = m_gpuMipmapOctree->GetMipLevelView(level);
        if (mipmapView == VK_NULL_HANDLE)
        {
            printf("WARNING: Mip level %u view is null\n", level);
            return;
        }
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = mipmapView;
        imageInfo.sampler = m_gpuMipmapOctree->GetSampler(); // Combined image sampler需要sampler
        imageInfos.push_back(imageInfo);
    }

    // === 同时更新收集管线的描述符集纹理绑定 ===
    std::vector<VkWriteDescriptorSet> collectionDescriptorWrites;

    // 创建收集管线的descriptor writes，引用同样的imageInfos
    for (uint32_t level = 0; level <= maxUsedLevel; ++level)
    {
        VkWriteDescriptorSet collectionWrite{};
        collectionWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        collectionWrite.dstSet = multiViewNodeSelection_.collectionDescriptorSet;
        collectionWrite.dstBinding = 2 + level; // Bindings 2-5 (Level 0-3) in collection shader
        collectionWrite.dstArrayElement = 0;
        collectionWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        collectionWrite.descriptorCount = 1;
        collectionWrite.pImageInfo = &imageInfos[level];
        collectionDescriptorWrites.push_back(collectionWrite);
    }

    // 更新收集管线描述符集
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(collectionDescriptorWrites.size()), collectionDescriptorWrites.data(), 0, nullptr);

    printf("Solid Node Selection B descriptor sets updated with mipmap textures (both original and collection pipelines)\n");
}

// Execute Node Selection 
void Renderer::ExecuteMultiViewNodeSelection(VkCommandBuffer commandBuffer)
{
    // === Multi-Pass执行架构 ===
    // Pass 1-3: 收集候选节点（level 3→2→1）
    // Pass 4: 包含检查和最终选择（使用SolidNodeSelectionB_Final.Comp）

    // === Phase 1: 清零所有计数器 ===

    // 清零候选节点计数器
    vkCmdFillBuffer(commandBuffer, multiViewNodeSelection_.candidateCountBuffer.buffer, 0, sizeof(uint32_t), 0);

    // 清零最终节点计数器
    vkCmdFillBuffer(commandBuffer, multiViewNodeSelection_.selectedCountBuffer.buffer, 0, VK_WHOLE_SIZE, 0);

    // === Phase 2: Pass 1-3 候选节点收集 (Level 3→2→1) ===

    // 绑定收集管线
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, multiViewNodeSelection_.collectionPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, multiViewNodeSelection_.collectionPipelineLayout, 0, 1,
                            &multiViewNodeSelection_.collectionDescriptorSet, 0, nullptr);



    multiViewNodeSelection_.CollectionPushConstant.BaseSize = config_->Sdf.Resolution;
    // 执行Level max,... 0
    // 应当从8x8x8开始选择, 直到base, 所以需要动态计算开始level.
    // 理想的base范围为[8,128]
    for (int32_t level = m_gpuMipmapOctree->GetMaxLevel()-1; level >= 0; --level)
    {
        // 设置当前level
        multiViewNodeSelection_.CollectionPushConstant.CurrentLevel = static_cast<uint32_t>(level);
        vkCmdPushConstants(commandBuffer, multiViewNodeSelection_.collectionPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MultiViewSolidNodeSelection::CollectionPushConstantDesc), &multiViewNodeSelection_.CollectionPushConstant);

        // 计算dispatch尺寸（使用GRID_SIZE=32作为基础）
        uint32_t gridSize = config_->Sdf.Resolution >> static_cast<uint32_t>(level); // Level 3: 4, Level 2: 8, Level 1: 16
        uint32_t dispatchX = (gridSize + 3) / 4; // 4x4x4 workgroup
        uint32_t dispatchY = (gridSize + 3) / 4;
        uint32_t dispatchZ = (gridSize + 3) / 4;

        // 执行候选节点收集
        vkCmdDispatch(commandBuffer, dispatchX, dispatchY, dispatchZ);

        // Level间屏障
        if (level > 1)
        {
            // 创建一个包含两个屏障的数组
            VkBufferMemoryBarrier bufferBarriers[2] = {};

            // 屏障 1: 同步候选节点计数器 (candidateCountBuffer)
            // 上一个shader写入了它，下一个shader需要读取并继续写入
            bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            bufferBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT; // 下一个shader会读写它
            bufferBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarriers[0].buffer = multiViewNodeSelection_.candidateCountBuffer.buffer;
            bufferBarriers[0].offset = 0;
            bufferBarriers[0].size = VK_WHOLE_SIZE;

            // 屏障 2: 同步候选节点数据 (candidateNodesBuffer)
            // 上一个shader向其追加了数据，下一个shader也需要向其追加
            // 尽管写入位置不同，但对同一资源的写入操作需要排序
            bufferBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            bufferBarriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // 下一个shader只会写它
            bufferBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            // 注意: 这里需要填入您的 candidateNodesBuffer 的 VkBuffer 句柄
            // 根据您代码的其他部分，它应该在 m_solidNodeSelectionB 结构体中
            bufferBarriers[1].buffer = multiViewNodeSelection_.candidateNodesBuffer.buffer;
            bufferBarriers[1].offset = 0;
            bufferBarriers[1].size = VK_WHOLE_SIZE;

            // 执行包含两个Buffer屏障的Pipeline Barrier
            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // 源阶段
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // 目标阶段
                                 0, 0, nullptr, 2, bufferBarriers,     // 传入2个buffer barrier
                                 0, nullptr);
        }
    }

    // === Phase 3: Pass 4 最终选择（包含检查）===

    // 屏障：确保候选节点收集完成
    VkMemoryBarrier collectionCompleteBarrier{};
    collectionCompleteBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    collectionCompleteBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    collectionCompleteBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &collectionCompleteBarrier,
                         0, nullptr, 0, nullptr);

    // === Phase 3: Pass 4 最终选择（包含检查）===

    // 绑定最终选择管线
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, multiViewNodeSelection_.finalSelectionPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, multiViewNodeSelection_.finalSelectionPipelineLayout, 0, 1,
                            &multiViewNodeSelection_.finalSelectionDescriptorSet, 0, nullptr);

    // === 优化：动态读取候选节点数量来精确计算dispatch尺寸 ===

    // 同步以确保候选节点计数已完成
    VkMemoryBarrier candidateCountBarrier{};
    candidateCountBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    candidateCountBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    candidateCountBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &candidateCountBarrier, 0, nullptr, 0,
                         nullptr);

    // 需要提交命令缓冲区并等待，以便读取候选节点数量
    // 为了优化，我们可以使用间接dispatch或保守估计
    // 这里先使用保守但更合理的估计：基于level数量
    uint32_t estimatedCandidates = multiViewNodeSelection_.MAX_CANDIDATE_NODES;       // Level 3: 4³=64, Level 2: 8³=512太多，保守估计
    uint32_t optimizedDispatch = (estimatedCandidates + 63) / 64; // 约2个workgroup而不是16个

    //printf("Optimized final selection dispatch: %u workgroups (estimated %u candidates)\n", optimizedDispatch, estimatedCandidates);

    vkCmdDispatch(commandBuffer, optimizedDispatch, 1, 1);

    //printf("Solid Node Selection B executed (multi-pass: 3 collection passes + 1 final selection pass)\n");
}
void Renderer::MultiViewSolidNodeSelection::cleanup(VkDevice device)
{


    // 清理收集管线
    if (collectionPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, collectionPipeline, nullptr);
        collectionPipeline = VK_NULL_HANDLE;
    }
    if (collectionPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, collectionPipelineLayout, nullptr);
        collectionPipelineLayout = VK_NULL_HANDLE;
    }
    if (collectionDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, collectionDescriptorSetLayout, nullptr);
        collectionDescriptorSetLayout = VK_NULL_HANDLE;
    }

    // 清理最终选择管线
    if (finalSelectionPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, finalSelectionPipeline, nullptr);
        finalSelectionPipeline = VK_NULL_HANDLE;
    }
    if (finalSelectionPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, finalSelectionPipelineLayout, nullptr);
        finalSelectionPipelineLayout = VK_NULL_HANDLE;
    }
    if (finalSelectionDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, finalSelectionDescriptorSetLayout, nullptr);
        finalSelectionDescriptorSetLayout = VK_NULL_HANDLE;
    }

    // 清理缓冲区
    candidateNodesBuffer.Destroy();
    candidateCountBuffer.Destroy();
    selectedNodesBuffer.Destroy();
    selectedCountBuffer.Destroy();
}

/// @brief 初始化阶段四版本B：多视角深度SDF资源
void Renderer::InitializeMultiViewDepthSDFResources()
{
    printf("Initializing Multi-View Depth SDF Resources...\n");

    // Initialize depth rendering pass resources
    InitializeMultiViewDepthRenderingPass();

    // Initialize SDF fusion pass resources
    InitializeSDFFusionPass();

    printf("Multi-View Depth SDF initialization complete\n");
}

void Renderer::InitializeGPUDataPreparation()
{
    printf("Initializing GPU Data Preparation resources...\n");

    // === 创建GPU-only缓冲区 ===

    // 相机矩阵缓冲区 (最多20个相机的矩阵 + 活跃相机数量)
    struct CameraMatrix
    {
        glm::mat4 viewMatrix;
        glm::vec3 cameraPosition;
        float padding;
    };
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // GPU-only
                                                   &m_gpuDataPreparation.cameraMatricesBuffer_GPU,
                                                   sizeof(CameraMatrix) * config_->Sdf.MultiViewUsedCameraNum));

    // 活跃相机数量缓冲区
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // GPU-only
                                                   &m_gpuDataPreparation.activeCameraCountBuffer_GPU, sizeof(uint32_t)));

    // 间接绘制命令缓冲区
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // GPU-only
                                                   &m_gpuDataPreparation.indirectDrawBuffer_GPU,
                                                   sizeof(VkDrawIndexedIndirectCommand) * config_->Sdf.MultiViewUsedCameraNum));

    // === 创建相机矩阵准备管线 ===
    CreateCameraMatrixPreparationPipeline();

    // === 创建间接命令生成管线 ===
    CreateIndirectCommandGenerationPipeline();

    // === 绑定描述符集 ===
    BindGPUDataPreparationDescriptors();

    printf("GPU Data Preparation resources initialized\n");
}

void Renderer::BindGPUDataPreparationDescriptors()
{
    // === 分配描述符集 ===

    // 分配相机矩阵准备描述符集
    VkDescriptorSetAllocateInfo cameraMatrixAllocInfo{};
    cameraMatrixAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    cameraMatrixAllocInfo.descriptorPool = m_descriptorPool;
    cameraMatrixAllocInfo.descriptorSetCount = 1;
    cameraMatrixAllocInfo.pSetLayouts = &m_gpuDataPreparation.cameraMatrixDescriptorLayout;
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &cameraMatrixAllocInfo, &m_gpuDataPreparation.cameraMatrixDescriptorSet));

    // 分配间接命令生成描述符集
    VkDescriptorSetAllocateInfo indirectCommandAllocInfo{};
    indirectCommandAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    indirectCommandAllocInfo.descriptorPool = m_descriptorPool;
    indirectCommandAllocInfo.descriptorSetCount = 1;
    indirectCommandAllocInfo.pSetLayouts = &m_gpuDataPreparation.indirectCommandDescriptorLayout;
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &indirectCommandAllocInfo, &m_gpuDataPreparation.indirectCommandDescriptorSet));

    // === 绑定相机矩阵准备描述符集 ===
    std::vector<VkWriteDescriptorSet> cameraMatrixDescriptorWrites;

    // Binding 0: selectedNodes (使用现有的选中节点缓冲区)
    VkDescriptorBufferInfo selectedNodesBufferInfo{};
    if (m_useSolidNodeSelectionB)
    {
        selectedNodesBufferInfo.buffer = multiViewNodeSelection_.selectedNodesBuffer.buffer;
        selectedNodesBufferInfo.range = sizeof(MultiViewSolidNodeSelection::SolidNode) * config_->Sdf.MultiViewUsedCameraNum;
    }
    else
    {
        selectedNodesBufferInfo.buffer = analyticalNodeSelection_.solidNodeBuffer.buffer;
        selectedNodesBufferInfo.range = sizeof(AnalyticalSolidNodeSelection::SolidNode) * AnalyticalSolidNodeSelection::MAX_SOLID_NODES;
    }
    selectedNodesBufferInfo.offset = 0;

    VkWriteDescriptorSet selectedNodesWrite{};
    selectedNodesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    selectedNodesWrite.dstSet = m_gpuDataPreparation.cameraMatrixDescriptorSet;
    selectedNodesWrite.dstBinding = 0;
    selectedNodesWrite.dstArrayElement = 0;
    selectedNodesWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    selectedNodesWrite.descriptorCount = 1;
    selectedNodesWrite.pBufferInfo = &selectedNodesBufferInfo;
    cameraMatrixDescriptorWrites.push_back(selectedNodesWrite);

    // Binding 1: cameraMatrices
    VkDescriptorBufferInfo cameraMatricesBufferInfo{};
    cameraMatricesBufferInfo.buffer = m_gpuDataPreparation.cameraMatricesBuffer_GPU.buffer;
    cameraMatricesBufferInfo.offset = 0;
    cameraMatricesBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet cameraMatricesWrite{};
    cameraMatricesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    cameraMatricesWrite.dstSet = m_gpuDataPreparation.cameraMatrixDescriptorSet;
    cameraMatricesWrite.dstBinding = 1;
    cameraMatricesWrite.dstArrayElement = 0;
    cameraMatricesWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    cameraMatricesWrite.descriptorCount = 1;
    cameraMatricesWrite.pBufferInfo = &cameraMatricesBufferInfo;
    cameraMatrixDescriptorWrites.push_back(cameraMatricesWrite);

    // Binding 2: activeCameraCount
    VkDescriptorBufferInfo activeCameraCountBufferInfo{};
    activeCameraCountBufferInfo.buffer = m_gpuDataPreparation.activeCameraCountBuffer_GPU.buffer;
    activeCameraCountBufferInfo.offset = 0;
    activeCameraCountBufferInfo.range = sizeof(uint32_t);

    VkWriteDescriptorSet activeCameraCountWrite{};
    activeCameraCountWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    activeCameraCountWrite.dstSet = m_gpuDataPreparation.cameraMatrixDescriptorSet;
    activeCameraCountWrite.dstBinding = 2;
    activeCameraCountWrite.dstArrayElement = 0;
    activeCameraCountWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    activeCameraCountWrite.descriptorCount = 1;
    activeCameraCountWrite.pBufferInfo = &activeCameraCountBufferInfo;
    cameraMatrixDescriptorWrites.push_back(activeCameraCountWrite);

    // 更新相机矩阵准备描述符集
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(cameraMatrixDescriptorWrites.size()), cameraMatrixDescriptorWrites.data(), 0, nullptr);

    // === 绑定间接命令生成描述符集 ===
    std::vector<VkWriteDescriptorSet> indirectCommandDescriptorWrites;

    // Binding 0: cameraMatrices (reuse the same buffer)
    VkWriteDescriptorSet cameraMatricesReadWrite{};
    cameraMatricesReadWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    cameraMatricesReadWrite.dstSet = m_gpuDataPreparation.indirectCommandDescriptorSet;
    cameraMatricesReadWrite.dstBinding = 0;
    cameraMatricesReadWrite.dstArrayElement = 0;
    cameraMatricesReadWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    cameraMatricesReadWrite.descriptorCount = 1;
    cameraMatricesReadWrite.pBufferInfo = &cameraMatricesBufferInfo; // 重用同一缓冲区信息
    indirectCommandDescriptorWrites.push_back(cameraMatricesReadWrite);

    // Binding 1: activeCameraCount (reuse the same buffer)
    VkWriteDescriptorSet activeCameraCountReadWrite{};
    activeCameraCountReadWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    activeCameraCountReadWrite.dstSet = m_gpuDataPreparation.indirectCommandDescriptorSet;
    activeCameraCountReadWrite.dstBinding = 1;
    activeCameraCountReadWrite.dstArrayElement = 0;
    activeCameraCountReadWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    activeCameraCountReadWrite.descriptorCount = 1;
    activeCameraCountReadWrite.pBufferInfo = &activeCameraCountBufferInfo; // 重用同一缓冲区信息
    indirectCommandDescriptorWrites.push_back(activeCameraCountReadWrite);

    // Binding 2: indirectCommands
    VkDescriptorBufferInfo indirectCommandsBufferInfo{};
    indirectCommandsBufferInfo.buffer = m_gpuDataPreparation.indirectDrawBuffer_GPU.buffer;
    indirectCommandsBufferInfo.offset = 0;
    indirectCommandsBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet indirectCommandsWrite{};
    indirectCommandsWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    indirectCommandsWrite.dstSet = m_gpuDataPreparation.indirectCommandDescriptorSet;
    indirectCommandsWrite.dstBinding = 2;
    indirectCommandsWrite.dstArrayElement = 0;
    indirectCommandsWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indirectCommandsWrite.descriptorCount = 1;
    indirectCommandsWrite.pBufferInfo = &indirectCommandsBufferInfo;
    indirectCommandDescriptorWrites.push_back(indirectCommandsWrite);

    // 更新间接命令生成描述符集
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(indirectCommandDescriptorWrites.size()), indirectCommandDescriptorWrites.data(), 0,
                           nullptr);
}

void Renderer::ExecuteGPUDataPreparation(VkCommandBuffer cmd)
{
    auto& gpuPrep = m_multiViewDepthSDF4C.gpuPreparation;

    // === Phase 1: 相机矩阵准备 ===
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gpuPrep.cameraMatrixPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gpuPrep.cameraMatrixPipelineLayout, 0, 1, &gpuPrep.cameraMatrixDescriptorSet, 0,
                            nullptr);

    // 推送参数：相机数限制 + 坐标变换参数 (与voxelization一致)
    struct CameraMatrixPushConstants
    {
        uint32_t maxCameraCount{}; // 最大相机数限制 (10)
        glm::vec3 modelCenter;                                      // 模型中心，与voxelization一致
        float halfSizeWithMargin;                                   // 包含边距的半尺寸，与voxelization一致
    } cameraPC;
    cameraPC.maxCameraCount = config_->Sdf.MultiViewUsedCameraNum;
    cameraPC.modelCenter = glm::vec3(0.0f);
    cameraPC.halfSizeWithMargin = 1.0f;

    vkCmdPushConstants(cmd, gpuPrep.cameraMatrixPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(cameraPC), &cameraPC);

    vkCmdDispatch(cmd, 1, 1, 1); // 单工作组处理

    // === 屏障：确保相机矩阵写入完成 ===
    VkMemoryBarrier cameraMatrixBarrier{};
    cameraMatrixBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    cameraMatrixBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    cameraMatrixBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &cameraMatrixBarrier, 0, nullptr, 0,
                         nullptr);

    // === Phase 2: 间接命令生成 ===
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gpuPrep.indirectCommandPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gpuPrep.indirectCommandPipelineLayout, 0, 1, &gpuPrep.indirectCommandDescriptorSet,
                            0, nullptr);

    // 推送模型参数
    struct IndirectCommandPushConstants
    {
        uint32_t totalPartCount; // 模型部件数量
    } indirectPC;

    indirectPC.totalPartCount = m_multiViewDepthSDF4C.staticData.totalPartCount;

    vkCmdPushConstants(cmd, gpuPrep.indirectCommandPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(indirectPC), &indirectPC);

    // 计算总的绘制命令数量：partCount * MAX_CAMERAS * 6 faces
    // 使用硬编码最大值，GPU着色器内部会处理实际相机数过滤
    // Now we only generate one command per part (not per camera-face combination)
    uint32_t commandCount = indirectPC.totalPartCount;
    vkCmdDispatch(cmd, (commandCount + 31) / 32, 1, 1); // 32线程per workgroup并行生成命令

    // === 屏障：确保间接命令写入完成 ===
    VkMemoryBarrier indirectCommandBarrier{};
    indirectCommandBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    indirectCommandBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    indirectCommandBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 1, &indirectCommandBarrier, 0, nullptr, 0,
                         nullptr);

    printf("GPU Data Preparation executed: camera matrices + indirect commands\n");
}

void Renderer::InitializeMultiViewDepthRenderingPass()
{
    ;
}

/// @brief 初始化阶段四：解析式SDF生成资源
void Renderer::InitializeAnalyticalSDFGenerationResources()
{
    // 创建64x64x64的SDF纹理 (R16_SFLOAT格式存储带符号距离)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_3D;
    imageInfo.extent.width = config_->Sdf.Resolution;
    imageInfo.extent.height = config_->Sdf.Resolution;
    imageInfo.extent.depth = config_->Sdf.Resolution;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_analyticalSDFGeneration.sdfTexture.image) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SDF texture!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_analyticalSDFGeneration.sdfTexture.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_analyticalSDFGeneration.sdfTexture.deviceMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate SDF texture memory!");
    }

    vkBindImageMemory(m_device, m_analyticalSDFGeneration.sdfTexture.image, m_analyticalSDFGeneration.sdfTexture.deviceMemory, 0);

    // 创建Image View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_analyticalSDFGeneration.sdfTexture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    viewInfo.format = VK_FORMAT_R32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_analyticalSDFGeneration.sdfTexture.view) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SDF texture view!");
    }

    // 设置基本属性
    m_analyticalSDFGeneration.sdfTexture.device = m_vulkanDevice;
    m_analyticalSDFGeneration.sdfTexture.format = VK_FORMAT_R16_SFLOAT;
    m_analyticalSDFGeneration.sdfTexture.width = config_->Sdf.Resolution;
    m_analyticalSDFGeneration.sdfTexture.height = config_->Sdf.Resolution;
    m_analyticalSDFGeneration.sdfTexture.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // 注意：图像布局转换将在执行时处理

    // 创建描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> bindings = {// Binding 0: Counter buffer (input - nodeCount)
                                                          {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                                                          // Binding 1: Solid node buffer (input)
                                                          {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                                                          // Binding 2: SDF texture (output)
                                                          {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_analyticalSDFGeneration.descriptorSetLayout));

    // 创建管线布局
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_analyticalSDFGeneration.descriptorSetLayout;
    VkPushConstantRange pushConstantRange{Init::pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, sizeof(uint32_t), 0)};
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_analyticalSDFGeneration.pipelineLayout));

    // 加载Compute Shader
    std::string shaderPath = Tool::GetShadersPath() + "Analytical/AnalyticalSDF.Comp.spv";
    VkShaderModule shaderModule = Tool::LoadShader(shaderPath.c_str(), m_device);

    // 创建Compute Pipeline
    VkComputePipelineCreateInfo computePipelineInfo{};
    computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computePipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineInfo.stage.module = shaderModule;
    computePipelineInfo.stage.pName = "main";
    computePipelineInfo.layout = m_analyticalSDFGeneration.pipelineLayout;

    Tool::CheckResult(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_analyticalSDFGeneration.pipeline));

    // 清理shader module
    vkDestroyShaderModule(m_device, shaderModule, nullptr);

    // 创建描述符池
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}, // counter + solid node buffers
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}   // SDF texture output
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    VkDescriptorPool descriptorPool;
    Tool::CheckResult(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &descriptorPool));

    // 分配描述符集
    VkDescriptorSetAllocateInfo sdfDescriptorAllocInfo{};
    sdfDescriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    sdfDescriptorAllocInfo.descriptorPool = descriptorPool;
    sdfDescriptorAllocInfo.descriptorSetCount = 1;
    sdfDescriptorAllocInfo.pSetLayouts = &m_analyticalSDFGeneration.descriptorSetLayout;

    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &sdfDescriptorAllocInfo, &m_analyticalSDFGeneration.descriptorSet));

    // 绑定资源到描述符集
    std::vector<VkWriteDescriptorSet> descriptorWrites;

    // Binding 0: Counter buffer (从SolidNodeSelection获取计数)
    VkDescriptorBufferInfo counterBufferInfo{};
    counterBufferInfo.buffer = analyticalNodeSelection_.counterBuffer.buffer; // 共享counter buffer
    counterBufferInfo.offset = 0;
    counterBufferInfo.range = sizeof(uint32_t);

    VkWriteDescriptorSet counterWrite{};
    counterWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    counterWrite.dstSet = m_analyticalSDFGeneration.descriptorSet;
    counterWrite.dstBinding = 0;
    counterWrite.dstArrayElement = 0;
    counterWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    counterWrite.descriptorCount = 1;
    counterWrite.pBufferInfo = &counterBufferInfo;
    descriptorWrites.push_back(counterWrite);

    // Binding 1: Solid node buffer (从SolidNodeSelection获取节点数据)
    VkDescriptorBufferInfo solidNodeBufferInfo{};
    solidNodeBufferInfo.buffer = analyticalNodeSelection_.solidNodeBuffer.buffer; // 共享solid node buffer
    solidNodeBufferInfo.offset = 0;
    solidNodeBufferInfo.range = sizeof(AnalyticalSolidNodeSelection::SolidNode) * AnalyticalSolidNodeSelection::MAX_SOLID_NODES;

    VkWriteDescriptorSet solidNodeWrite{};
    solidNodeWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    solidNodeWrite.dstSet = m_analyticalSDFGeneration.descriptorSet;
    solidNodeWrite.dstBinding = 1;
    solidNodeWrite.dstArrayElement = 0;
    solidNodeWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    solidNodeWrite.descriptorCount = 1;
    solidNodeWrite.pBufferInfo = &solidNodeBufferInfo;
    descriptorWrites.push_back(solidNodeWrite);

    // Binding 2: SDF texture output
    VkDescriptorImageInfo sdfImageInfo{};
    sdfImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    sdfImageInfo.imageView = m_analyticalSDFGeneration.sdfTexture.view;
    sdfImageInfo.sampler = VK_NULL_HANDLE; // Storage image doesn't need sampler

    VkWriteDescriptorSet sdfImageWrite{};
    sdfImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sdfImageWrite.dstSet = m_analyticalSDFGeneration.descriptorSet;
    sdfImageWrite.dstBinding = 2;
    sdfImageWrite.dstArrayElement = 0;
    sdfImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    sdfImageWrite.descriptorCount = 1;
    sdfImageWrite.pImageInfo = &sdfImageInfo;
    descriptorWrites.push_back(sdfImageWrite);

    // 更新描述符集
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    printf("Analytical SDF Generation resources initialized (descriptor set created)\n");
}

// Update AnalyticalSDFGeneration descriptor set to use different node selection version
void Renderer::UpdateAnalyticalSDFGenerationDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> descriptorWrites;

    // Binding 0: Counter buffer (选择对应版本的counter buffer)
    VkDescriptorBufferInfo counterBufferInfo{};
    if (m_useSolidNodeSelectionB)
    {
        counterBufferInfo.buffer = multiViewNodeSelection_.selectedCountBuffer.buffer;
        counterBufferInfo.range = sizeof(uint32_t);
    }
    else
    {
        counterBufferInfo.buffer = analyticalNodeSelection_.counterBuffer.buffer;
        counterBufferInfo.range = sizeof(uint32_t);
    }
    counterBufferInfo.offset = 0;

    VkWriteDescriptorSet counterWrite{};
    counterWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    counterWrite.dstSet = m_analyticalSDFGeneration.descriptorSet;
    counterWrite.dstBinding = 0;
    counterWrite.dstArrayElement = 0;
    counterWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    counterWrite.descriptorCount = 1;
    counterWrite.pBufferInfo = &counterBufferInfo;
    descriptorWrites.push_back(counterWrite);

    // Binding 1: Solid node buffer (选择对应版本的node buffer)
    VkDescriptorBufferInfo solidNodeBufferInfo{};
    if (m_useSolidNodeSelectionB)
    {
        solidNodeBufferInfo.buffer = multiViewNodeSelection_.selectedNodesBuffer.buffer;
        solidNodeBufferInfo.range = sizeof(MultiViewSolidNodeSelection::SolidNode) * config_->Sdf.MultiViewUsedCameraNum;
    }
    else
    {
        solidNodeBufferInfo.buffer = analyticalNodeSelection_.solidNodeBuffer.buffer;
        solidNodeBufferInfo.range = sizeof(AnalyticalSolidNodeSelection::SolidNode) * AnalyticalSolidNodeSelection::MAX_SOLID_NODES;
    }
    solidNodeBufferInfo.offset = 0;

    VkWriteDescriptorSet solidNodeWrite{};
    solidNodeWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    solidNodeWrite.dstSet = m_analyticalSDFGeneration.descriptorSet;
    solidNodeWrite.dstBinding = 1;
    solidNodeWrite.dstArrayElement = 0;
    solidNodeWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    solidNodeWrite.descriptorCount = 1;
    solidNodeWrite.pBufferInfo = &solidNodeBufferInfo;
    descriptorWrites.push_back(solidNodeWrite);

    // Binding 2: SDF texture output (不变)
    VkDescriptorImageInfo sdfImageInfo{};
    sdfImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    sdfImageInfo.imageView = m_analyticalSDFGeneration.sdfTexture.view;
    sdfImageInfo.sampler = VK_NULL_HANDLE;

    VkWriteDescriptorSet sdfImageWrite{};
    sdfImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sdfImageWrite.dstSet = m_analyticalSDFGeneration.descriptorSet;
    sdfImageWrite.dstBinding = 2;
    sdfImageWrite.dstArrayElement = 0;
    sdfImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    sdfImageWrite.descriptorCount = 1;
    sdfImageWrite.pImageInfo = &sdfImageInfo;
    descriptorWrites.push_back(sdfImageWrite);

    // 更新描述符集
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    printf("Analytical SDF Generation descriptor set updated for %s\n", m_useSolidNodeSelectionB ? "Version B" : "Version A");
}

// Set which version of Solid Node Selection to use
void Renderer::SetSolidNodeSelectionVersion(bool useVersionB)
{
    if (m_useSolidNodeSelectionB != useVersionB)
    {
        m_useSolidNodeSelectionB = useVersionB;
        printf("Switched to Solid Node Selection %s\n", useVersionB ? "Version B (Adaptive)" : "Version A (Legacy)");

        // 如果命令已经录制过，需要重新录制
        if (m_unifiedGPUPipeline.commandsRecorded)
        {
            m_unifiedGPUPipeline.commandsRecorded = false;
            printf("Pipeline commands will be re-recorded with new version\n");
        }
    }
}

/// @brief 执行阶段三：实体节点筛选
/// @brief 更新SolidNodeSelection描述符集的mipmap纹理绑定
void Renderer::UpdateSolidNodeSelectionDescriptorSet()
{
    if (!m_gpuMipmapOctree)
    {
        printf("ERROR: GPUMipmapOctree not initialized, cannot bind mipmap textures\n");
        return;
    }

    // 准备mipmap纹理的descriptor信息 
    const uint32_t maxUsedLevel = m_gpuMipmapOctree->GetMaxLevel(); // Use levels 0, 1, 2, 3, 4 (5 levels total)
    std::vector<VkWriteDescriptorSet> descriptorWrites;
    std::vector<VkDescriptorImageInfo> imageInfos;

    // 预分配空间以避免vector重新分配导致的指针失效
    imageInfos.reserve(maxUsedLevel);
    descriptorWrites.reserve(maxUsedLevel);

    // 获取GPUMipmapOctree的sampler
    VkSampler mipmapSampler = m_gpuMipmapOctree->GetSampler();

    // 为每个mipmap层级创建image info - 限制到level 0-4 (64x64x64 到 4x4x4)

    // 先收集所有imageInfo
    for (uint32_t level = 0; level <= maxUsedLevel; ++level)
    {
        VkImageView mipmapView = m_gpuMipmapOctree->GetMipLevelView(level);
        if (mipmapView == VK_NULL_HANDLE)
        {
            return;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = mipmapView;
        imageInfo.sampler = mipmapSampler;

        imageInfos.push_back(imageInfo);
    }

    // 然后创建descriptorWrites，引用稳定的imageInfos
    for (uint32_t level = 0; level <= maxUsedLevel; ++level)
    {
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = analyticalNodeSelection_.descriptorSet;
        descriptorWrite.dstBinding = 2 + level; // Bindings 2-6
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfos[level];
        descriptorWrites.push_back(descriptorWrite);
    }

    // 更新descriptor set
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    printf("AnalyticalSolidNodeSelection: Mipmap textures bound to descriptor set (levels 0-%d)\n", maxUsedLevel);
}

void Renderer::ExecuteAnalyticalNodeSelection(VkCommandBuffer cmd)
{
    // NOTE: UpdateSolidNodeSelectionDescriptorSet is now called during initialization
    // in InitializeUnifiedGPUPipelineResources(), so descriptor set is ready to use

    // 重置计数器
    uint32_t zero = 0;
    vkCmdFillBuffer(cmd, analyticalNodeSelection_.counterBuffer.buffer, 0, sizeof(uint32_t), 0);

    // 内存屏障：确保计数器重置完成
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

    // 绑定管线和描述符集
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, analyticalNodeSelection_.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, analyticalNodeSelection_.pipelineLayout, 0, 1, &analyticalNodeSelection_.descriptorSet, 0,
                            nullptr);

    AnalyticalSolidNodeSelection::SolidNodeSelectionPushConstant pushConstants;
    pushConstants.SampledLevel = config_->Sdf.AnalyticalSampledLevel;
    pushConstants.BaseSize = config_->Sdf.Resolution; // 64
    pushConstants.modelCenter = glm::vec3(0.0f, 0.0f, 0.0f);
    pushConstants.halfSizeWithMargin = 1.0f;

    vkCmdPushConstants(cmd, analyticalNodeSelection_.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);

    uint32_t levelSize = config_->Sdf.Resolution >> config_->Sdf.AnalyticalSampledLevel;
    uint32_t groupSize = 4;
    uint32_t groupCountX = (levelSize + groupSize - 1) / groupSize;
    uint32_t groupCountY = (levelSize + groupSize - 1) / groupSize;
    uint32_t groupCountZ = (levelSize + groupSize - 1) / groupSize;

    vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);

    printf("  Solid Node Selection: Dispatched %dx%dx%d thread groups (%d threads)\n", groupCountX, groupCountY, groupCountZ,
           groupCountX * groupCountY * 16);
}

void Renderer::ExecuteVoxelizationMarkPass(VkCommandBuffer cmd)
{
    // === 开始Dynamic Rendering ===
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, {config_->Sdf.Resolution, config_->Sdf.Resolution}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 0;
    renderingInfo.pColorAttachments = nullptr;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderingInfo);

    // === 设置视口和剪切 ===
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width =static_cast<float>(config_->Sdf.Resolution);
    viewport.height = static_cast<float>(config_->Sdf.Resolution);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = {config_->Sdf.Resolution, config_->Sdf.Resolution};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // === 绑定管线和描述符集 ===
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_voxelizationPass.markPassPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_voxelizationPass.markPassPipelineLayout, 0, 1,
                            &m_voxelizationPass.markPassDescriptorSet, 0, nullptr);

    // === 绘制模型 ===
    // 使用RenderFlags::BindImages来绑定描述符集1，修复验证层错误
    m_glTFModel.Draw(cmd, 0, m_voxelizationPass.markPassPipelineLayout, 1);

    // === 结束Dynamic Rendering ===
    vkCmdEndRendering(cmd);
}

void Renderer::ExecuteVoxelizationFillPass(VkCommandBuffer cmd)
{
    // === 绑定管线和描述符集 ===
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_voxelizationPass.fillPassPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_voxelizationPass.fillPassPipelineLayout, 0, 1,
                            &m_voxelizationPass.fillPassDescriptorSet, 0, nullptr);

    // === 执行Compute Shader ===
    // 方案要求：64x64个线程，每个线程沿Z轴扫描VoxelCounterTexture
    uint32_t groupSize = (config_->Sdf.Resolution + 7) / 8; // 8x8 线程组 (XY平面)
    vkCmdDispatch(cmd, groupSize, groupSize, 1);                // Z轴扫描，因此Z维度为1
}

/// @brief 预录制统一GPU管线所有命令 - 只录制一次，提升性能
void Renderer::RecordUnifiedGPUPipelineCommands()
{
    if (!m_unifiedGPUPipeline.resourcesInitialized)
    {
        printf("ERROR: Resources not initialized before recording commands\n");
        return;
    }

    if (m_unifiedGPUPipeline.commandsRecorded)
    {
        return; // 已经录制过了
    }


    SetSolidNodeSelectionVersion(config_->Sdf.SdfMode == 1?true:false);

    // 在录制命令之前，根据版本选择更新阶段四的descriptor set
    UpdateAnalyticalSDFGenerationDescriptorSet();

    VkCommandBuffer cmd = m_unifiedGPUPipeline.commandBuffer;

    // 开始录制命令
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // 可以多次提交
    Tool::CheckResult(vkBeginCommandBuffer(cmd, &beginInfo));

    // ===== 统一GPU管线录制开始 =====
    BeginDebugLabel(cmd, "Unified GPU Pipeline", 1.0f, 1.0f, 0.0f, 1.0f);

    // 1. 体素化标记阶段 (Mark Pass)
    BeginDebugLabel(cmd, "Voxelization Mark Pass", 1.0f, 0.0f, 0.0f, 1.0f);
    ExecuteVoxelizationMarkPass(cmd);
    EndDebugLabel(cmd);

    // 屏障：确保标记写入对填充可见
    VkMemoryBarrier memBarrier1{};
    memBarrier1.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier1.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier1, 0, nullptr, 0,
                         nullptr);

    // 2. 体素化填充阶段 (Fill Pass)
    BeginDebugLabel(cmd, "Voxelization Fill Pass", 0.0f, 1.0f, 0.0f, 1.0f);
    ExecuteVoxelizationFillPass(cmd);
    EndDebugLabel(cmd);

    // 屏障：确保填充写入对Mipmap可见
    VkMemoryBarrier memBarrier2{};
    memBarrier2.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier2.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier2, 0, nullptr, 0, nullptr);

    // 3. Mipmap隐式八叉树构建 (限制到第4层)
    BeginDebugLabel(cmd, "Mipmap Octree Generation (to level 4)", 0.0f, 0.0f, 1.0f, 1.0f);
    // 注意：这里需要修改GPUMipmapOctree来限制mipmap层数到4层
    m_gpuMipmapOctree->BuildFromVoxelTexture(cmd, &m_voxelizationPass.finalVoxelStateTexture);
    EndDebugLabel(cmd);

    // 屏障：确保八叉树构建完成对节点筛选可见
    VkMemoryBarrier memBarrier3{};
    memBarrier3.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier3.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier3.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier3, 0, nullptr, 0, nullptr);

    // 4. 阶段三：实体节点筛选 (Solid Node Selection)
    if (m_useSolidNodeSelectionB)
    {
        BeginDebugLabel(cmd, "Solid Node Selection B (Adaptive)", 0.0f, 1.0f, 0.5f, 1.0f);
        ExecuteMultiViewNodeSelection(cmd);
        EndDebugLabel(cmd);
    }
    else
    {
        BeginDebugLabel(cmd, "Solid Node Selection A (Legacy)", 0.0f, 1.0f, 1.0f, 1.0f);
        ExecuteAnalyticalNodeSelection(cmd);
        EndDebugLabel(cmd);
    }

    // 屏障：确保节点筛选完成对SDF生成可见
    VkMemoryBarrier memBarrier4{};
    memBarrier4.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier4.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier4.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier4, 0, nullptr, 0, nullptr);
    printf("DEBUG: m_useSolidNodeSelectionB = %s\n", m_useSolidNodeSelectionB ? "true (MultiView)" : "false (Analytical)");

    if (m_useSolidNodeSelectionB)
    {
        // 5A. 阶段四：多视角深度渲染与融合 (Multi-View Depth SDF) - 启用
        // GPU Data Preparation Stage: Execute compute shaders for camera matrices and indirect commands
        BeginDebugLabel(cmd, "GPU Data Preparation Stage", 1.0f, 0.5f, 0.0f, 1.0f);
        ExecuteGPUDataPreparation(cmd);
        EndDebugLabel(cmd);

        // Memory barrier: Ensure GPU data preparation completes before depth rendering
        VkMemoryBarrier gpuDataBarrier{};
        gpuDataBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        gpuDataBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        gpuDataBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0,
                             1, &gpuDataBarrier, 0, nullptr, 0, nullptr);

        BeginDebugLabel(cmd, "Multi-View Depth SDF (256³)", 0.0f, 0.5f, 1.0f, 1.0f);
        ExecuteMultiViewDepthRendering(cmd);
        EndDebugLabel(cmd);
        // Note: Image barrier is handled inside ExecuteMultiViewDepthRendering()
        // No additional barrier needed here - image is already in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        BeginDebugLabel(cmd, "SDFFusion", 0.0f, 0.5f, 1.0f, 1.0f);
        ExecuteSDFFusion(cmd);
        EndDebugLabel(cmd);
    }
    else
    {
        // 5B. 阶段四：解析式SDF生成 (20个节点，64³分辨率)
        BeginDebugLabel(cmd, "Analytical SDF Generation (64 nodes, 64³)", 1.0f, 0.0f, 1.0f, 1.0f);
        ExecuteAnalyticalSDFGeneration(cmd);
        EndDebugLabel(cmd);
    }

    EndDebugLabel(cmd); // 结束统一GPU管线标签

    // 结束命令录制
    Tool::CheckResult(vkEndCommandBuffer(cmd));

    m_unifiedGPUPipeline.commandsRecorded = true;
    //printf("Unified GPU Pipeline commands pre-recorded\n");
}

/// @brief 在主渲染循环中提交统一GPU管线
void Renderer::SubmitUnifiedGPUPipeline()
{
    if (!m_unifiedGPUPipeline.resourcesInitialized || !m_unifiedGPUPipeline.commandsRecorded)
    {
        printf("ERROR: Pipeline not ready for submission\n");
        return;
    }


    // 更新体素化常量（每帧可能变化的数据）
    UpdateVoxelizationConstants();

    // 提交预录制的命令缓冲区
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_unifiedGPUPipeline.commandBuffer;

    // 等待compute完成信号量，发出completion信号量
    // Fence仅用于时间统计，不用于同步
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_compute.semaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_unifiedGPUPipeline.completionSemaphore;
    Tool::CheckResult(vkQueueSubmit(m_queues.graphicsQueue, 1, &submitInfo, nullptr));

    // 移除调试验证代码以减少CPU-GPU同步开销
}

/// @brief 验证SolidNodeSelection的执行结果（调试用）
void Renderer::ValidateSolidNodeSelectionResults()
{
    // 读取counterBuffer中的节点计数
    analyticalNodeSelection_.counterBuffer.Map();
    uint32_t* counterData = static_cast<uint32_t*>(analyticalNodeSelection_.counterBuffer.mapped);
    uint32_t selectedNodeCount = counterData[0];
    analyticalNodeSelection_.counterBuffer.Unmap();

    // 静态计数器，只在第一次验证时输出详细信息
    static int validationCount = 0;
    if (validationCount == 0)
    {
        printf("=== Final Node Selection (Priority L3→L2→L1→L0) ===\n");
        printf("Selected Node Count: %u\n", selectedNodeCount);

        if (selectedNodeCount == 0)
        {
            printf("ERROR: No nodes found!\n");
        }
        else
        {
            // 读取solidNodeBuffer来检查节点分布
            analyticalNodeSelection_.solidNodeBuffer.Map();
            const AnalyticalSolidNodeSelection::SolidNode* nodes =
                static_cast<const AnalyticalSolidNodeSelection::SolidNode*>(analyticalNodeSelection_.solidNodeBuffer.mapped);

            // Show first 20 selected nodes and count by level
            printf("First 64 selected nodes:\n");
            uint32_t nodesToShow = std::min(selectedNodeCount, 512u);
            uint32_t levelCount[4] = {0}; // Only levels 0-3 exist
            for (uint32_t i = 0; i < nodesToShow; ++i)
            {
                printf("  Node %2u: center(%6.3f,%6.3f,%6.3f), size=%.3f, L%u\n", i, nodes[i].center.x, nodes[i].center.y, nodes[i].center.z,
                       nodes[i].size, nodes[i].level);
                if (nodes[i].level < 4)
                    levelCount[nodes[i].level]++;
            }
            printf("Level distribution: L0=%u, L1=%u, L2=%u, L3=%u\n", levelCount[0], levelCount[1], levelCount[2], levelCount[3]);

            // Analyze Z distribution
            uint32_t negativeZ = 0, positiveZ = 0;
            for (uint32_t i = 0; i < nodesToShow; ++i)
            {
                if (nodes[i].center.z < 0.0f)
                    negativeZ++;
                else
                    positiveZ++;
            }
            printf("Z distribution in first 20: Negative=%u, Positive=%u\n", negativeZ, positiveZ);

            analyticalNodeSelection_.solidNodeBuffer.Unmap();
        }
        printf("==============================\n");
    }
    validationCount++;
}

void Renderer::ExecuteAnalyticalSDFGeneration(VkCommandBuffer cmd)
{
    // 转换SDF纹理布局到VK_IMAGE_LAYOUT_GENERAL
    VkImageMemoryBarrier imageBarrier{};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = m_analyticalSDFGeneration.sdfTexture.image;
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.baseMipLevel = 0;
    imageBarrier.subresourceRange.levelCount = 1;
    imageBarrier.subresourceRange.baseArrayLayer = 0;
    imageBarrier.subresourceRange.layerCount = 1;
    imageBarrier.srcAccessMask = 0;
    imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    // 绑定管线和描述符集
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_analyticalSDFGeneration.pipeline);
    vkCmdPushConstants(cmd, m_analyticalSDFGeneration.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                            sizeof(uint32_t), &config_->Sdf.Resolution);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_analyticalSDFGeneration.pipelineLayout, 0, 1,
                            &m_analyticalSDFGeneration.descriptorSet, 0, nullptr);

    // 分发compute工作：64x64x64的线程网格，每个线程计算一个SDF体素
    // Shader使用4x4x4的工作组
    uint32_t groupCountX = (config_->Sdf.Resolution + 3) / 4;                // 16 groups
    uint32_t groupCountY = (config_->Sdf.Resolution + 3) / 4;                // 16 groups
    uint32_t groupCountZ = (config_->Sdf.Resolution + 3) / 4;                // 16 groups
    vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);

    printf("  Analytical SDF Generation: Dispatched %dx%dx%d compute groups (262144 total threads)\n", groupCountX, groupCountY, groupCountZ);
}


/// @brief 获取模型顶点缓冲区
VkBuffer Renderer::GetModelVertexBuffer()
{
    return m_glTFModel.vertices.buffer;
}

/// @brief 获取模型索引缓冲区
VkBuffer Renderer::GetModelIndexBuffer()
{
    return m_glTFModel.indices.buffer;
}

/// @brief 获取模型索引数量
uint32_t Renderer::GetModelIndexCount()
{
    return m_glTFModel.indices.count;
}

/// @brief 获取模型顶点数量
uint32_t Renderer::GetModelVertexCount()
{
    return m_glTFModel.vertices.count;
}

// ==================== MultiViewDepthSDF4C 实现 ====================

/// @brief 从GLTF模型提取并上传静态数据到GPU
void Renderer::LoadModelStaticData4C()
{
    printf("Loading model static data for MultiViewDepthSDF4C...\n");

    auto& staticData = m_multiViewDepthSDF4C.staticData;

    // 1. 直接使用现有的顶点和索引缓冲区
    staticData.modelVertexBuffer.buffer = m_glTFModel.vertices.buffer;
    staticData.modelVertexBuffer.memory = m_glTFModel.vertices.memory;
    staticData.modelVertexBuffer.size = m_glTFModel.vertices.count * sizeof(vkglTF::Vertex);

    staticData.modelIndexBuffer.buffer = m_glTFModel.indices.buffer;
    staticData.modelIndexBuffer.memory = m_glTFModel.indices.memory;
    staticData.modelIndexBuffer.size = m_glTFModel.indices.count * sizeof(uint32_t);

    // 2. 收集所有子部件信息
    staticData.partInfos.clear();
    staticData.totalPartCount = 0;

    // 遍历所有节点和原语来构建子部件信息
    for (auto* node : m_glTFModel.linearNodes)
    {
        if (node->mesh)
        {
            for (auto* primitive : node->mesh->primitives)
            {
                MultiViewDepthSDF4C::ModelPartInfo partInfo;
                partInfo.indexCount = primitive->indexCount;
                partInfo.firstIndex = primitive->firstIndex;
                partInfo.vertexOffset = primitive->firstVertex;
                partInfo.materialIndex = 0; // 暂时设为0，可以根据需要扩展

                staticData.partInfos.push_back(partInfo);
                staticData.totalPartCount++;
            }
        }
    }

    printf("Found %u model parts\n", staticData.totalPartCount);
    printf("Model vertex buffer: %p, size: %zu bytes (%u vertices)\n", staticData.modelVertexBuffer.buffer, staticData.modelVertexBuffer.size,
           GetModelVertexCount());
    printf("Model index buffer: %p, size: %zu bytes (%u indices)\n", staticData.modelIndexBuffer.buffer, staticData.modelIndexBuffer.size,
           GetModelIndexCount());

    // 3. 创建模型子部件信息缓冲区
    CreateModelPartInfos4C();

    // 4. 创建模型矩阵缓冲区
    CreateModelMatrices4C();

    printf("Model static data loaded successfully\n");
}

/// @brief 创建模型子部件信息缓冲区
void Renderer::CreateModelPartInfos4C()
{
    auto& staticData = m_multiViewDepthSDF4C.staticData;

    if (staticData.partInfos.empty())
    {
        printf("Warning: No model parts found\n");
        return;
    }

    // 创建并上传模型子部件信息缓冲区
    VkDeviceSize bufferSize = sizeof(MultiViewDepthSDF4C::ModelPartInfo) * staticData.partInfos.size();

    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &staticData.modelPartsBuffer, bufferSize, staticData.partInfos.data()));

    printf("Created model parts buffer with %zu parts\n", staticData.partInfos.size());
}

/// @brief 创建模型矩阵缓冲区
void Renderer::CreateModelMatrices4C()
{
    auto& staticData = m_multiViewDepthSDF4C.staticData;

    if (staticData.totalPartCount == 0)
    {
        printf("Warning: No model parts found for matrix creation\n");
        return;
    }

    // 为每个子部件创建模型矩阵
    std::vector<glm::mat4> modelMatrices(staticData.totalPartCount);

    size_t partIndex = 0;
    for (auto* node : m_glTFModel.linearNodes)
    {
        if (node->mesh)
        {
            // 获取节点的变换矩阵
            glm::mat4 nodeMatrix = node->getMatrix();

            for (auto* primitive : node->mesh->primitives)
            {
                if (partIndex < modelMatrices.size())
                {
                    // Fix: Use identity matrix since vertices are pre-transformed by glTF loader
                    // The nodeMatrix was already applied during PreTransformVertices stage
                    modelMatrices[partIndex] = glm::mat4(1.0f);
                    partIndex++;
                }
            }
        }
    }

    // 创建并上传模型矩阵缓冲区
    VkDeviceSize bufferSize = sizeof(glm::mat4) * modelMatrices.size();

    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &staticData.modelMatricesBuffer, bufferSize, modelMatrices.data()));

    printf("Created model matrices buffer with %zu matrices\n", modelMatrices.size());
}

/// @brief 初始化MultiViewDepthSDF4C的所有资源
void Renderer::InitializeMultiViewDepthSDF4CResources()
{
    printf("Initializing MultiViewDepthSDF4C resources...\n");

    // 1. 加载模型静态数据
    LoadModelStaticData4C();

    // 2. 初始化GPU数据准备阶段
    InitializeGPUDataPreparation4C();

    // 3. 初始化深度渲染阶段
    InitializeDepthRendering4C();

    // 4. 初始化SDF融合阶段
    InitializeSDFFusion4C();

    printf("MultiViewDepthSDF4C resources initialized successfully\n");
}

/// @brief 初始化GPU数据准备阶段资源
void Renderer::InitializeGPUDataPreparation4C()
{
    printf("Initializing GPU data preparation resources...\n");

    auto& gpuPrep = m_multiViewDepthSDF4C.gpuPreparation;

    // 创建相机矩阵缓冲区 (host-visible for CPU updates)
    VkDeviceSize cameraBufferSize = sizeof(glm::vec4) * config_->Sdf.MultiViewUsedCameraNum; // CameraMatrix{float4 cameraPosition;}
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &gpuPrep.cameraMatricesBuffer, cameraBufferSize));

    // 创建间接命令缓冲区 (for GPU generation of indirect commands)
    uint32_t maxParts = m_multiViewDepthSDF4C.staticData.totalPartCount;
    uint32_t maxCameras = config_->Sdf.MultiViewUsedCameraNum;
    uint32_t maxCommands = maxParts; // Now only one command per part
    VkDeviceSize indirectBufferSize = sizeof(VkDrawIndexedIndirectCommand) * maxCommands;
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &gpuPrep.indirectCommandsBuffer, indirectBufferSize));

    // 创建相机数量缓冲区 (单个uint32_t)
    VkDeviceSize cameraCountBufferSize = sizeof(uint32_t);
    Tool::CheckResult(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                   &gpuPrep.activeCameraCountBuffer, cameraCountBufferSize));

    // 创建CameraMatrixPreparation计算管线
    CreateCameraMatrixPreparationPipeline();

    // 创建IndirectCommandGeneration计算管线
    CreateIndirectCommandGenerationPipeline();

    printf("GPU data preparation resources initialized\n");
}

/// @brief 初始化深度渲染阶段资源
void Renderer::InitializeDepthRendering4C()
{
    printf("Initializing depth rendering resources...\n");

    // 1. 创建深度立方体贴图数组
    CreateDepthCubemapArray();

    // 2. 创建Multiview渲染通道
    CreateMultiViewDepthRenderPass();

    // 3. 创建多视角深度渲染管线
    CreateMultiViewDepthPipeline();

    printf("Depth rendering resources initialized successfully\n");
}

/// @brief 初始化SDF融合阶段资源
/// @brief Initialize SDF Fusion Pass - Stage 4C complete implementation
void Renderer::InitializeSDFFusion4C()
{
    printf("Initializing SDF fusion resources...\n");

    auto& sdfFusionPass = m_multiViewDepthSDF4C.sdfFusionPass;

    // Create all SDF fusion resources and initialize descriptor sets
    InitializeSDFFusionPass();

    printf("SDF fusion resources initialized successfully\n");
}

/// @brief 创建相机矩阵准备计算管线
void Renderer::CreateCameraMatrixPreparationPipeline()
{
    auto& gpuPrep = m_multiViewDepthSDF4C.gpuPreparation;

    // 1. 创建描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    // Binding 0: 输入选中节点缓冲区 (from Stage 3B)
    bindings.push_back(Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0));
    // Binding 1: 输出相机矩阵缓冲区
    bindings.push_back(Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1));
    // Binding 2: 输出活跃相机数量缓冲区
    bindings.push_back(Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2));
    // Binding 3: 输入实际节点数量缓冲区 (from Stage 3B counter buffer)
    bindings.push_back(Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3));

    VkDescriptorSetLayoutCreateInfo layoutInfo = Init::descriptorSetLayoutCreateInfo(bindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &gpuPrep.cameraMatrixDescriptorLayout));

    // 2. 创建管线布局 (支持Push Constants)
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint32_t) + sizeof(glm::vec3) + sizeof(float); // maxCameraCount + modelCenter + halfSizeWithMargin = 20字节

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = Init::pipelineLayoutCreateInfo(&gpuPrep.cameraMatrixDescriptorLayout, 1);
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &gpuPrep.cameraMatrixPipelineLayout));

    // 3. 加载着色器并创建计算管线
    VkPipelineShaderStageCreateInfo shaderStageInfo =
        LoadShader(Tool::GetShadersPath() + "CameraMatrixPreparation.Comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = gpuPrep.cameraMatrixPipelineLayout;

    Tool::CheckResult(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gpuPrep.cameraMatrixPipeline));

    // 清理着色器模块
    vkDestroyShaderModule(m_device, shaderStageInfo.module, nullptr);

    // 4. 分配描述符集
    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &gpuPrep.cameraMatrixDescriptorLayout, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &gpuPrep.cameraMatrixDescriptorSet));

    // 5. 更新描述符集
    std::vector<VkWriteDescriptorSet> writes;

    // 绑定Stage 3B的选中节点缓冲区
    VkDescriptorBufferInfo selectedNodesBufferInfo{};
    selectedNodesBufferInfo.buffer = multiViewNodeSelection_.selectedNodesBuffer.buffer;
    selectedNodesBufferInfo.offset = 0;
    selectedNodesBufferInfo.range = VK_WHOLE_SIZE;

    writes.push_back(Init::writeDescriptorSet(gpuPrep.cameraMatrixDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &selectedNodesBufferInfo));

    // 绑定相机矩阵输出缓冲区
    VkDescriptorBufferInfo cameraMatricesBufferInfo{};
    cameraMatricesBufferInfo.buffer = gpuPrep.cameraMatricesBuffer.buffer;
    cameraMatricesBufferInfo.offset = 0;
    cameraMatricesBufferInfo.range = VK_WHOLE_SIZE;

    writes.push_back(Init::writeDescriptorSet(gpuPrep.cameraMatrixDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &cameraMatricesBufferInfo));

    // 绑定活跃相机数量缓冲区
    VkDescriptorBufferInfo cameraCountBufferInfo{};
    cameraCountBufferInfo.buffer = gpuPrep.activeCameraCountBuffer.buffer;
    cameraCountBufferInfo.offset = 0;
    cameraCountBufferInfo.range = VK_WHOLE_SIZE;

    writes.push_back(Init::writeDescriptorSet(gpuPrep.cameraMatrixDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &cameraCountBufferInfo));

    // 绑定阶段3的实际节点数量缓冲区 (selectedCountBuffer from Stage 3B)
    VkDescriptorBufferInfo actualNodeCountBufferInfo{};
    actualNodeCountBufferInfo.buffer = multiViewNodeSelection_.selectedCountBuffer.buffer;
    actualNodeCountBufferInfo.offset = 0;
    actualNodeCountBufferInfo.range = VK_WHOLE_SIZE;

    writes.push_back(Init::writeDescriptorSet(gpuPrep.cameraMatrixDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &actualNodeCountBufferInfo));

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    printf("Camera matrix preparation pipeline created successfully\n");
}

/// @brief 创建间接命令生成计算管线
void Renderer::CreateIndirectCommandGenerationPipeline()
{
    auto& gpuPrep = m_multiViewDepthSDF4C.gpuPreparation;

    // 1. 创建描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    // Binding 0: 输入模型子部件信息缓冲区
    bindings.push_back(Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0));
    // Binding 1: 输入活跃相机数量缓冲区
    bindings.push_back(Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1));
    // Binding 2: 输出间接绘制命令缓冲区
    bindings.push_back(Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2));

    VkDescriptorSetLayoutCreateInfo layoutInfo = Init::descriptorSetLayoutCreateInfo(bindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &gpuPrep.indirectCommandDescriptorLayout));

    // 2. 创建管线布局 (支持Push Constants)
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint32_t); // totalPartCount

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = Init::pipelineLayoutCreateInfo(&gpuPrep.indirectCommandDescriptorLayout, 1);
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &gpuPrep.indirectCommandPipelineLayout));

    // 3. 加载着色器并创建计算管线
    VkPipelineShaderStageCreateInfo shaderStageInfo =
        LoadShader(Tool::GetShadersPath() + "IndirectCommandGeneration.Comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = gpuPrep.indirectCommandPipelineLayout;

    Tool::CheckResult(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gpuPrep.indirectCommandPipeline));

    // 清理着色器模块
    vkDestroyShaderModule(m_device, shaderStageInfo.module, nullptr);

    // 4. 分配描述符集
    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &gpuPrep.indirectCommandDescriptorLayout, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &gpuPrep.indirectCommandDescriptorSet));

    // 5. 更新描述符集
    std::vector<VkWriteDescriptorSet> writes;

    // 绑定模型子部件信息缓冲区
    VkDescriptorBufferInfo partInfoBufferInfo{};
    partInfoBufferInfo.buffer = m_multiViewDepthSDF4C.staticData.modelPartsBuffer.buffer;
    partInfoBufferInfo.offset = 0;
    partInfoBufferInfo.range = VK_WHOLE_SIZE;

    writes.push_back(Init::writeDescriptorSet(gpuPrep.indirectCommandDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &partInfoBufferInfo));

    // 绑定活跃相机数量缓冲区
    VkDescriptorBufferInfo cameraCountBufferInfo{};
    cameraCountBufferInfo.buffer = gpuPrep.activeCameraCountBuffer.buffer;
    cameraCountBufferInfo.offset = 0;
    cameraCountBufferInfo.range = VK_WHOLE_SIZE;

    writes.push_back(Init::writeDescriptorSet(gpuPrep.indirectCommandDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &cameraCountBufferInfo));

    // 绑定间接绘制命令缓冲区
    VkDescriptorBufferInfo indirectBufferInfo{};
    indirectBufferInfo.buffer = gpuPrep.indirectCommandsBuffer.buffer;
    indirectBufferInfo.offset = 0;
    indirectBufferInfo.range = VK_WHOLE_SIZE;

    writes.push_back(Init::writeDescriptorSet(gpuPrep.indirectCommandDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &indirectBufferInfo));

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    printf("Indirect command generation pipeline created successfully\n");
}

/// @brief 创建深度立方体贴图数组
void Renderer::CreateDepthCubemapArray()
{
    auto& depthPass = m_multiViewDepthSDF4C.depthRendering;
    auto& gpuPrep = m_multiViewDepthSDF4C.gpuPreparation;

    // 获取活跃相机数量，如果为0则使用默认值
    uint32_t cameraCount = gpuPrep.activeCameraCount;
    if (cameraCount == 0)
    {
        cameraCount = config_->Sdf.MultiViewUsedCameraNum; // 使用最大值作为默认
    }
    uint32_t totalLayers = cameraCount * 6; // N个相机 × 6个面

    // 1. 创建图像
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // 启用立方体兼容性，支持CUBE_ARRAY视图
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32_SFLOAT; // DepthRenderingPass4C::DEPTH_FORMAT
    imageInfo.extent.width = 64;             // DepthRenderingPass4C::CUBEMAP_SIZE
    imageInfo.extent.height = 64;            // DepthRenderingPass4C::CUBEMAP_SIZE
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = totalLayers;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    Tool::CheckResult(vkCreateImage(m_device, &imageInfo, nullptr, &depthPass.depthCubemapArray));

    // 2. 分配内存
    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(m_device, depthPass.depthCubemapArray, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_vulkanDevice->GetMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Tool::CheckResult(vkAllocateMemory(m_device, &allocInfo, nullptr, &depthPass.depthCubemapMemory));
    Tool::CheckResult(vkBindImageMemory(m_device, depthPass.depthCubemapArray, depthPass.depthCubemapMemory, 0));

    // 3. 创建图像视图 (用于渲染)
    VkImageViewCreateInfo renderViewInfo{};
    renderViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    renderViewInfo.image = depthPass.depthCubemapArray;
    renderViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    renderViewInfo.format = VK_FORMAT_R32_SFLOAT; // DepthRenderingPass4C::DEPTH_FORMAT
    renderViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    renderViewInfo.subresourceRange.baseMipLevel = 0;
    renderViewInfo.subresourceRange.levelCount = 1;
    renderViewInfo.subresourceRange.baseArrayLayer = 0;
    renderViewInfo.subresourceRange.layerCount = totalLayers;

    Tool::CheckResult(vkCreateImageView(m_device, &renderViewInfo, nullptr, &depthPass.depthCubemapArrayView));

    // 4. 创建采样视图 (用于后续SDF融合阶段)
    VkImageViewCreateInfo samplingViewInfo = renderViewInfo;
    samplingViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY; // 立方体数组视图 (用于SDF融合采样)

    Tool::CheckResult(vkCreateImageView(m_device, &samplingViewInfo, nullptr, &depthPass.depthCubemapSamplingView));

    printf("Depth cubemap array created successfully (%u cameras, %u total layers)\n", cameraCount, totalLayers);
}

/// @brief Create multiview depth render pass
void Renderer::CreateMultiViewDepthRenderPass()
{
    auto& depthPass = m_multiViewDepthSDF4C.depthRendering;

    // 1. Define color attachment (depth values output)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R32_SFLOAT; // DepthRenderingPass4C::DEPTH_FORMAT
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // 2. Define depth attachment (CRITICAL: Missing depth attachment!)
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT; // Standard depth format
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Don't need to store depth
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1; // Second attachment
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 3. Define subpass with both color and depth
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // 4. Create render pass with both color and depth attachments
    VkAttachmentDescription attachments[2] = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2; // Both color and depth
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    Tool::CheckResult(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &depthPass.renderPass));

    // 5. Create depth attachment for depth testing
    VkImageCreateInfo depthImageInfo{};
    depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageInfo.format = VK_FORMAT_D32_SFLOAT;
    depthImageInfo.extent.width = 64;  // DepthRenderingPass4C::CUBEMAP_SIZE
    depthImageInfo.extent.height = 64; // DepthRenderingPass4C::CUBEMAP_SIZE
    depthImageInfo.extent.depth = 1;
    depthImageInfo.mipLevels = 1;
    depthImageInfo.arrayLayers = config_->Sdf.MultiViewUsedCameraNum * 6; // Match color attachment layers (60)
    depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    Tool::CheckResult(vkCreateImage(m_device, &depthImageInfo, nullptr, &depthPass.depthAttachment));

    VkMemoryRequirements depthMemReqs;
    vkGetImageMemoryRequirements(m_device, depthPass.depthAttachment, &depthMemReqs);

    VkMemoryAllocateInfo depthAllocInfo{};
    depthAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    depthAllocInfo.allocationSize = depthMemReqs.size;
    depthAllocInfo.memoryTypeIndex = FindMemoryType(depthMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Tool::CheckResult(vkAllocateMemory(m_device, &depthAllocInfo, nullptr, &depthPass.depthAttachmentMemory));
    vkBindImageMemory(m_device, depthPass.depthAttachment, depthPass.depthAttachmentMemory, 0);

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = depthPass.depthAttachment;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; // Array view for multiple layers
    depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
    depthViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = config_->Sdf.MultiViewUsedCameraNum * 6; // All 60 layers

    Tool::CheckResult(vkCreateImageView(m_device, &depthViewInfo, nullptr, &depthPass.depthAttachmentView));

    // 6. Create single framebuffer that binds both color array and depth attachment
    VkImageView attachmentViews[2] = {depthPass.depthCubemapArrayView, depthPass.depthAttachmentView};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = depthPass.renderPass;
    framebufferInfo.attachmentCount = 2; // Both color array and depth
    framebufferInfo.pAttachments = attachmentViews;
    framebufferInfo.width = 64;                                    // DepthRenderingPass4C::CUBEMAP_SIZE (降低到64)
    framebufferInfo.height = 64;                                   // DepthRenderingPass4C::CUBEMAP_SIZE (降低到64)
    framebufferInfo.layers = config_->Sdf.MultiViewUsedCameraNum * 6; // All layers (10 cameras × 6 faces = 60)

    Tool::CheckResult(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &depthPass.framebuffer));

    printf("Multi-layer depth render pass created successfully (no multiview, %u total layers)\n", config_->Sdf.MultiViewUsedCameraNum * 6);
}

/// @brief 创建多视角深度渲染管线
void Renderer::CreateMultiViewDepthPipeline()
{
    auto& depthPass = m_multiViewDepthSDF4C.depthRendering;

    // 1. 创建描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    // Binding 0: 相机矩阵缓冲区
    bindings.push_back(Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0));
    // Binding 1: 模型矩阵缓冲区
    bindings.push_back(Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1));

    VkDescriptorSetLayoutCreateInfo layoutInfo = Init::descriptorSetLayoutCreateInfo(bindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &depthPass.descriptorSetLayout));

    // 2. 创建管线布局 (包含Push Constants)
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(MultiViewDepthSDF4C::DepthRenderingPass4C::RenderParams); // Actual size
    printf("Push Constants size: %zu bytes\n", sizeof(MultiViewDepthSDF4C::DepthRenderingPass4C::RenderParams));

    // // Debug: Print model dimensions for scale analysis
    // printf("=== MODEL DIMENSIONS DEBUG ===\n");
    // printf("Model min: (%.3f, %.3f, %.3f)\n", m_glTFModel.dimensions.min.x, m_glTFModel.dimensions.min.y, m_glTFModel.dimensions.min.z);
    // printf("Model max: (%.3f, %.3f, %.3f)\n", m_glTFModel.dimensions.max.x, m_glTFModel.dimensions.max.y, m_glTFModel.dimensions.max.z);
    // printf("Model size: (%.3f, %.3f, %.3f)\n", m_glTFModel.dimensions.size.x, m_glTFModel.dimensions.size.y, m_glTFModel.dimensions.size.z);
    // printf("Model center: (%.3f, %.3f, %.3f)\n", m_glTFModel.dimensions.center.x, m_glTFModel.dimensions.center.y,
    // m_glTFModel.dimensions.center.z); printf("Model radius: %.3f\n", m_glTFModel.dimensions.radius); printf("==============================\n");


    VkPipelineLayoutCreateInfo pipelineLayoutInfo = Init::pipelineLayoutCreateInfo(&depthPass.descriptorSetLayout, 1);
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &depthPass.pipelineLayout));

    // 3. 加载着色器
    VkPipelineShaderStageCreateInfo vertShader = LoadShader(Tool::GetShadersPath() + "MultiViewDepth.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    VkPipelineShaderStageCreateInfo fragShader = LoadShader(Tool::GetShadersPath() + "MultiViewDepth.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShader, fragShader};

    // 4. 配置顶点输入状态
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // 使用模型的顶点输入格式
    auto bindingDescription = vkglTF::Vertex::inputBindingDescription(0);
    auto attributeDescriptions = vkglTF::Vertex::inputAttributeDescriptions(0, {vkglTF::VertexComponent::Position}); // 只使用位置，移除未使用的法线

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // 5. 配置输入装配状态
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 6. 配置视口状态
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = 64.0f;  // static_cast<float>(DepthRenderingPass4C::CUBEMAP_SIZE)
    viewport.height = 64.0f; // static_cast<float>(DepthRenderingPass4C::CUBEMAP_SIZE)
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {64, 64}; // {DepthRenderingPass4C::CUBEMAP_SIZE, DepthRenderingPass4C::CUBEMAP_SIZE}

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // 7. 配置光栅化状态
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE; // 启用背面剔除优化
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // 8. 配置深度状态 (CRITICAL: Missing depth state was causing overwrite issues!)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;                    // 启用深度测试
    depthStencil.depthWriteEnable = VK_TRUE;                   // 启用深度写入
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // 深度比较操作
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // 9. 配置多重采样状态
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 10. 配置颜色混合状态
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT; // 只写红色通道(深度值)
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // 11. 创建图形管线
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil; // CRITICAL: Add missing depth state!
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = depthPass.pipelineLayout;
    pipelineInfo.renderPass = depthPass.renderPass;
    pipelineInfo.subpass = 0;

    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &depthPass.pipeline));

    // 清理着色器模块
    vkDestroyShaderModule(m_device, vertShader.module, nullptr);
    vkDestroyShaderModule(m_device, fragShader.module, nullptr);

    // 11. 分配和更新描述符集
    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &depthPass.descriptorSetLayout, 1);
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &depthPass.descriptorSet));

    // 更新描述符集
    std::vector<VkWriteDescriptorSet> writes;

    // 绑定相机矩阵缓冲区
    VkDescriptorBufferInfo cameraBufferInfo{};
    cameraBufferInfo.buffer = m_multiViewDepthSDF4C.gpuPreparation.cameraMatricesBuffer.buffer;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = VK_WHOLE_SIZE;

    writes.push_back(Init::writeDescriptorSet(depthPass.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &cameraBufferInfo));

    // 绑定模型矩阵缓冲区
    VkDescriptorBufferInfo modelBufferInfo{};
    modelBufferInfo.buffer = m_multiViewDepthSDF4C.staticData.modelMatricesBuffer.buffer;
    modelBufferInfo.offset = 0;
    modelBufferInfo.range = VK_WHOLE_SIZE;

    writes.push_back(Init::writeDescriptorSet(depthPass.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &modelBufferInfo));

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    printf("Multi-view depth pipeline created successfully\n");
}

/// @brief 执行多视角深度渲染
/// @brief Execute multiview depth rendering using indirect rendering - true multiview implementation
void Renderer::ExecuteMultiViewDepthRendering(VkCommandBuffer cmd)
{
    auto& depthPass = m_multiViewDepthSDF4C.depthRendering;
    auto& gpuPrep = m_multiViewDepthSDF4C.gpuPreparation;
    auto& staticData = m_multiViewDepthSDF4C.staticData;

    // 0. Transition depth cubemap array to color attachment layout
    VkImageMemoryBarrier preRenderBarrier{};
    preRenderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // After first frame, image will be in SHADER_READ_ONLY_OPTIMAL
    // We'll use a static flag to track the first frame
    preRenderBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    preRenderBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    preRenderBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preRenderBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preRenderBarrier.image = depthPass.depthCubemapArray;
    preRenderBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    preRenderBarrier.subresourceRange.baseMipLevel = 0;
    preRenderBarrier.subresourceRange.levelCount = 1;
    preRenderBarrier.subresourceRange.baseArrayLayer = 0;
    // Use maximum cameras for consistent layer count
    uint32_t layerCount = config_->Sdf.MultiViewUsedCameraNum * 6;
    preRenderBarrier.subresourceRange.layerCount = layerCount;
    preRenderBarrier.srcAccessMask = 0;
    preRenderBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &preRenderBarrier);

    // 1. Begin render pass - includes both color and depth attachments
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{50.0f, 0.0f, 0.0f, 1.0f}}; // Clear color to far plane distance
    clearValues[1].depthStencil = {1.0f, 0};            // Clear depth to far plane (1.0)

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = depthPass.renderPass;   // Render pass with depth attachment
    renderPassInfo.framebuffer = depthPass.framebuffer; // Framebuffer with both color and depth
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {64, 64}; // DepthRenderingPass4C::CUBEMAP_SIZE
    renderPassInfo.clearValueCount = 2;          // Both color and depth
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 2. Bind pipeline and resources - single setup for all rendering
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPass.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPass.pipelineLayout, 0, 1, &depthPass.descriptorSet, 0, nullptr);

    // 3. Bind vertex and index buffers
    VkBuffer vertexBuffers[] = {staticData.modelVertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, staticData.modelIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    // 4. Set up push constants with hardcoded maximum camera count
    // Use MAX_CAMERAS to ensure consistent behavior (no GPU readback)
    uint32_t maxTotalCommands = staticData.totalPartCount * config_->Sdf.MultiViewUsedCameraNum * 6;
    depthPass.renderParams.ModelMatrix = m_glTFModel.GetModelToStandardTransform();
    depthPass.renderParams.projectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.01f, 2.0f); // 降低远平面到50
    depthPass.renderParams.totalPartCount = 1;                                                          // Only use first part
    depthPass.renderParams.activeCameraCount = config_->Sdf.MultiViewUsedCameraNum;                        // Use max cameras (10)
    depthPass.renderParams.totalDrawCommands = maxTotalCommands;
    depthPass.renderParams.baseInstanceID = 0; // 从0开始编码

    vkCmdPushConstants(cmd, depthPass.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(MultiViewDepthSDF4C::DepthRenderingPass4C::RenderParams), &depthPass.renderParams);

    // 5. Execute indirect rendering with maximum camera count
    // Use all generated commands (GPU will handle actual camera filtering)
    vkCmdDrawIndexedIndirect(cmd,
                             gpuPrep.indirectCommandsBuffer.buffer, // Generated in step 2
                             0,                                     // offset
                             1,                                     // draw count (only first part)
                             sizeof(VkDrawIndexedIndirectCommand)); // stride

    // 6. End render pass
    vkCmdEndRenderPass(cmd);

    //printf("Multiview depth rendering executed: %u cameras × 6 faces × %u parts (hardcoded max)\n", config_->Sdf.MultiViewUsedCameraNum,
           //staticData.totalPartCount);
}

// ===== Stage 4C SDF Fusion Implementation =====

void Renderer::CreateFinalSDFTexture()
{
    // Step 1: Create 3D storage texture for final SDF
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_3D;
    imageInfo.format = VK_FORMAT_R32_SFLOAT; // Single-channel 32-bit float for SDF values
    imageInfo.extent = {config_->Sdf.Resolution, config_->Sdf.Resolution,
                        config_->Sdf.Resolution};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // Storage for compute write, sampled for later use, transfer for debug export
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = vkCreateImage(m_device, &imageInfo, nullptr, &m_multiViewDepthSDF4C.sdfFusionPass.finalSDFTexture);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create final SDF texture!");
    }

    // Step 2: Allocate and bind memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_multiViewDepthSDF4C.sdfFusionPass.finalSDFTexture, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_multiViewDepthSDF4C.sdfFusionPass.finalSDFMemory);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate SDF texture memory!");
    }

    vkBindImageMemory(m_device, m_multiViewDepthSDF4C.sdfFusionPass.finalSDFTexture, m_multiViewDepthSDF4C.sdfFusionPass.finalSDFMemory, 0);

    // Step 3: Create image view for storage access
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_multiViewDepthSDF4C.sdfFusionPass.finalSDFTexture;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    viewInfo.format = VK_FORMAT_R32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    result = vkCreateImageView(m_device, &viewInfo, nullptr, &m_multiViewDepthSDF4C.sdfFusionPass.finalSDFView);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SDF texture view!");
    }

    printf("Final SDF texture created: %ux%ux%u R32_SFLOAT 3D storage texture\n", config_->Sdf.Resolution,
           config_->Sdf.Resolution, config_->Sdf.Resolution);
}

void Renderer::CreateDepthCubemapSampler()
{
    // Create sampler for reading depth cubemap array in compute shader
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR; // Smooth interpolation
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // Prevent sampling outside cubemap
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE; // Disable anisotropy for depth maps
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE; // Use [0,1] coordinates
    samplerInfo.compareEnable = VK_FALSE;           // No depth comparison
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    VkResult result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_multiViewDepthSDF4C.sdfFusionPass.depthCubemapSampler);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create depth cubemap sampler!");
    }

    printf("Depth cubemap sampler created for SDF fusion\n");
}

void Renderer::CreateFinalSDFSampler()
{
    // Create sampler for reading final SDF texture in SDF AO Pass
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR; // Smooth interpolation for SDF sampling
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // Clamp to prevent artifacts
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE; // No anisotropy needed for SDF
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE; // Use [0,1] coordinates
    samplerInfo.compareEnable = VK_FALSE;           // No depth comparison
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    VkResult result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_multiViewDepthSDF4C.sdfFusionPass.finalSDFSampler);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create final SDF sampler!");
    }

    // Create descriptor for SDF AO Pass binding
    m_multiViewDepthSDF4C.sdfFusionPass.finalSDFDescriptor.imageView = m_multiViewDepthSDF4C.sdfFusionPass.finalSDFView;
    m_multiViewDepthSDF4C.sdfFusionPass.finalSDFDescriptor.sampler = m_multiViewDepthSDF4C.sdfFusionPass.finalSDFSampler;
    m_multiViewDepthSDF4C.sdfFusionPass.finalSDFDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    printf("Final SDF sampler and descriptor created for SDF AO Pass\n");
}

void Renderer::CreateSDFFusionPipeline()
{
    // Step 1: Create descriptor set layout - matches SDFFusion.Comp.hlsl bindings exactly
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};

    // Binding 0: Sampler
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: Sampled Image (2D Array - depth cubemap array)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Storage Image (3D - final SDF texture)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: Storage Buffer (camera matrices)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkResult result = vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_multiViewDepthSDF4C.sdfFusionPass.descriptorSetLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SDF fusion descriptor set layout!");
    }

    // Step 2: Create push constant range for SDF parameters
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(m_multiViewDepthSDF4C.sdfFusionPass.pushConstants); // 16 bytes: activeCameraCount + maxDistance + padding

    // Step 3: Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_multiViewDepthSDF4C.sdfFusionPass.descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    result = vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_multiViewDepthSDF4C.sdfFusionPass.pipelineLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SDF fusion pipeline layout!");
    }

    // Step 4: Load compute shader
    std::string shaderPath = Tool::GetShadersPath() + "/SDFFusion.Comp.spv";
    VkShaderModule computeShaderModule = Tool::LoadShader(shaderPath.c_str(), m_device);

    // Step 5: Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = computeShaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = m_multiViewDepthSDF4C.sdfFusionPass.pipelineLayout;

    result = vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_multiViewDepthSDF4C.sdfFusionPass.computePipeline);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SDF fusion compute pipeline!");
    }

    // Step 6: Cleanup
    vkDestroyShaderModule(m_device, computeShaderModule, nullptr);

    printf("SDF fusion compute pipeline created with 4 resource bindings\n");
}

void Renderer::InitializeSDFFusionPass()
{
    // Step 1: Create required resources
    // CreateFinalSDFTexture();
    // CreateFinalSDFSampler(); // Create sampler for SDF AO Pass sampling
    CreateDepthCubemapSampler();
    CreateSDFFusionPipeline();

    // Step 2: Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_multiViewDepthSDF4C.sdfFusionPass.descriptorSetLayout;

    VkResult result = vkAllocateDescriptorSets(m_device, &allocInfo, &m_multiViewDepthSDF4C.sdfFusionPass.descriptorSet);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate SDF fusion descriptor set!");
    }

    // Step 3: Reuse existing depth cubemap sampling view (no need to create duplicate)

    // Step 4: Verify image layout for SDF fusion (debug)
    printf("SDF Fusion: Using existing depth cubemap sampling view (image: %p, view: %p)\n",
           (void*)m_multiViewDepthSDF4C.depthRendering.depthCubemapArray, (void*)m_multiViewDepthSDF4C.depthRendering.depthCubemapSamplingView);

    // Step 5: Update descriptor set
    std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

    // Binding 0: Sampler
    VkDescriptorImageInfo samplerInfo{};
    samplerInfo.sampler = m_multiViewDepthSDF4C.sdfFusionPass.depthCubemapSampler;

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = m_multiViewDepthSDF4C.sdfFusionPass.descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &samplerInfo;

    // Binding 1: Sampled Image (depth cubemap array)
    // CRITICAL: Use the same view that depth rendering writes to!
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = m_multiViewDepthSDF4C.depthRendering.depthCubemapSamplingView; // Use sampling view, not array view
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = m_multiViewDepthSDF4C.sdfFusionPass.descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    // Binding 2: Storage Image (final SDF texture)
    VkDescriptorImageInfo storageImageInfo{};
    storageImageInfo.imageView = m_multiViewDepthSDF4C.sdfFusionPass.finalSDFView;
    storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = m_multiViewDepthSDF4C.sdfFusionPass.descriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pImageInfo = &storageImageInfo;

    // Binding 3: Storage Buffer (camera matrices)
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_multiViewDepthSDF4C.gpuPreparation.cameraMatricesBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = m_multiViewDepthSDF4C.sdfFusionPass.descriptorSet;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    // Step 5: Initialize push constants
    // Note: SDF fusion should read actual camera count from GPU buffer, not from push constants
    // This is for compatibility, the actual count will come from cameraMatricesBuffer binding
    m_multiViewDepthSDF4C.sdfFusionPass.pushConstants.activeCameraCount = config_->Sdf.MultiViewUsedCameraNum;
    m_multiViewDepthSDF4C.sdfFusionPass.pushConstants.maxDistance = 10.0f; // 10 units maximum SDF distance

    printf("SDF fusion pass initialized: %u cameras, %.1f max distance\n", m_multiViewDepthSDF4C.sdfFusionPass.pushConstants.activeCameraCount,
           m_multiViewDepthSDF4C.sdfFusionPass.pushConstants.maxDistance);
}

void Renderer::ExecuteSDFFusion(VkCommandBuffer cmd)
{
    // Debug: Print current state for debugging
    printf("SDF Fusion: About to read from depth cubemap array (image: %p)\n", (void*)m_multiViewDepthSDF4C.depthRendering.depthCubemapArray);

    // Step 1: Transition SDF texture to general layout for compute write
    VkImageMemoryBarrier sdfBarrier{};
    sdfBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    sdfBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    sdfBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    sdfBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    sdfBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    sdfBarrier.image = m_multiViewDepthSDF4C.sdfFusionPass.finalSDFTexture;
    sdfBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    sdfBarrier.subresourceRange.baseMipLevel = 0;
    sdfBarrier.subresourceRange.levelCount = 1;
    sdfBarrier.subresourceRange.baseArrayLayer = 0;
    sdfBarrier.subresourceRange.layerCount = 1;
    sdfBarrier.srcAccessMask = 0;
    sdfBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &sdfBarrier);

    // Step 2: Bind compute pipeline and descriptor set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_multiViewDepthSDF4C.sdfFusionPass.computePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_multiViewDepthSDF4C.sdfFusionPass.pipelineLayout, 0, 1,
                            &m_multiViewDepthSDF4C.sdfFusionPass.descriptorSet, 0, nullptr);

    // Step 3: Push constants
    vkCmdPushConstants(cmd, m_multiViewDepthSDF4C.sdfFusionPass.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(m_multiViewDepthSDF4C.sdfFusionPass.pushConstants), &m_multiViewDepthSDF4C.sdfFusionPass.pushConstants);

    // Step 4: Dispatch compute shader
    // SDF grid is 64³, compute shader uses 8×8×8 thread groups
    // Therefore we need (64/8)³ = 8³ work groups
    const uint32_t workGroupsPerDim = config_->Sdf.Resolution / 8; // 64 / 8 = 8
    vkCmdDispatch(cmd, workGroupsPerDim, workGroupsPerDim, workGroupsPerDim);

    // Step 5: Memory barrier for compute write completion
    VkMemoryBarrier memBarrier{};
    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);

    // Step 6: Transition SDF texture from GENERAL to SHADER_READ_ONLY_OPTIMAL for SDF AO Pass
    VkImageMemoryBarrier sdfLayoutTransition{};
    sdfLayoutTransition.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    sdfLayoutTransition.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    sdfLayoutTransition.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    sdfLayoutTransition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    sdfLayoutTransition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    sdfLayoutTransition.image = m_multiViewDepthSDF4C.sdfFusionPass.finalSDFTexture;
    sdfLayoutTransition.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    sdfLayoutTransition.subresourceRange.baseMipLevel = 0;
    sdfLayoutTransition.subresourceRange.levelCount = 1;
    sdfLayoutTransition.subresourceRange.baseArrayLayer = 0;
    sdfLayoutTransition.subresourceRange.layerCount = 1;
    sdfLayoutTransition.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    sdfLayoutTransition.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // For SDF AO Pass
                         0, 0, nullptr, 0, nullptr, 1, &sdfLayoutTransition);

    printf("SDF fusion executed: %ux%ux%u voxels processed (32³ work groups)\n", config_->Sdf.Resolution,
           config_->Sdf.Resolution, config_->Sdf.Resolution);
}

void Renderer::ExportSDFDataForVisualization(Texture* texture, VkImageLayout oldLayout, const std::string fileName)
{
    // 确保所有GPU工作完成
    vkDeviceWaitIdle(m_device);

    printf("GPU工作已完成，开始SDF导出...\n");
    // 自适应检测当前使用的SDF生成版本
    VkImage sdfTextureToExport = texture->image;
    uint32_t sdfResolution = texture->dimZ;

    const size_t totalVoxels = sdfResolution * sdfResolution * sdfResolution;
    const size_t dataSize = totalVoxels * sizeof(float);

    // 1. 创建staging buffer来从GPU读取SDF数据
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = dataSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    Tool::CheckResult(vkCreateBuffer(m_device, &bufferInfo, nullptr, &stagingBuffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        m_vulkanDevice->GetMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    Tool::CheckResult(vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingBufferMemory));
    Tool::CheckResult(vkBindBufferMemory(m_device, stagingBuffer, stagingBufferMemory, 0));

    // 2. 复制SDF纹理到staging buffer
    VkCommandBuffer commandBuffer = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // 转换图像布局为传输源
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout; // SDF纹理当前布局
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = sdfTextureToExport; // 使用自适应选择的SDF纹理
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // 复制图像到buffer
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0; // 紧密打包
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {sdfResolution, sdfResolution, sdfResolution};

    vkCmdCopyImageToBuffer(commandBuffer, sdfTextureToExport, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

    // 恢复原始布局（重要：避免后续渲染出错）
    VkImageMemoryBarrier restoreBarrier{};
    restoreBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    restoreBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    restoreBarrier.newLayout = oldLayout; // 恢复到原始布局
    restoreBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    restoreBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    restoreBarrier.image = sdfTextureToExport;
    restoreBarrier.subresourceRange = barrier.subresourceRange;
    restoreBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    restoreBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &restoreBarrier);

    m_vulkanDevice->FlushCommandBuffer(commandBuffer, m_queues.graphicsQueue, true);

    // 3. 映射内存并保存到文件
    void* data;
    Tool::CheckResult(vkMapMemory(m_device, stagingBufferMemory, 0, dataSize, 0, &data));

    float* sdfData = static_cast<float*>(data);

    // 统计数值分布
    int negCount = 0, posCount = 0, zeroCount = 0;
    float minVal = FLT_MAX, maxVal = -FLT_MAX;
    for (size_t i = 0; i < totalVoxels; i++)
    {
        float val = sdfData[i];
        if (val < -1e-6f)
            negCount++;
        else if (val > 1e-6f)
            posCount++;
        else
            zeroCount++;
        minVal = std::min(minVal, val);
        maxVal = std::max(maxVal, val);
    }
    printf("数值分布: 负值=%d, 正值=%d, 零值=%d, 范围=[%.3f, %.3f]\n", negCount, posCount, zeroCount, minVal, maxVal);

    FILE* file = fopen(fileName.c_str(), "wb");
    if (file)
    {
        fwrite(data, sizeof(float), totalVoxels, file);
        fclose(file);
        printf("SDF data exported to: %s (%u³ = %zu voxels, %.2f MB)\n", fileName.c_str(), sdfResolution, totalVoxels,
               dataSize / (1024.0f * 1024.0f));
    }
    else
    {
        printf("Failed to write SDF data to file: %s\n", fileName.c_str());
    }

    vkUnmapMemory(m_device, stagingBufferMemory);

    // 4. 清理资源
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingBufferMemory, nullptr);

    printf("SDF visualization export completed\n");
}
void Renderer::SetupSdfAOPass()
{
    sdfAOPass_.frameBuffer = new FramebufferManager(m_vulkanDevice);
    sdfAOPass_.frameBuffer->width = m_width;
    sdfAOPass_.frameBuffer->height = m_height;

    AttachmentCreateInfo attachmentCi{};
    attachmentCi.width = sdfAOPass_.frameBuffer->width;
    attachmentCi.height = sdfAOPass_.frameBuffer->height;
    attachmentCi.layerCount = 1;
    attachmentCi.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    attachmentCi.format = VK_FORMAT_R32G32B32A32_SFLOAT;

    sdfAOPass_.frameBuffer->AddAttachment(attachmentCi);

    Tool::CheckResult(sdfAOPass_.frameBuffer->CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    Tool::CheckResult(sdfAOPass_.frameBuffer->CreateRenderPass());
}

void Renderer::CreateBuffersSdfAO()
{
    m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 &sdfAOPass_.buffers.cBuffer, sizeof(SdfAOPass::CBufferDesc));
    Tool::CheckResult(sdfAOPass_.buffers.cBuffer.Map());
    UpdateCBufferSdfAO();
}
void Renderer::UpdateCBufferSdfAO()
{
    sdfAOPass_.buffers.cBufferData.sampleCount = 10;
    sdfAOPass_.buffers.cBufferData.sampleRadius = 1.0f;
    sdfAOPass_.buffers.cBufferData.aoStrength = 2.0f;
    sdfAOPass_.buffers.cBufferData.biasDistance = 0.0f;
    sdfAOPass_.buffers.cBufferData.maxDistance = 0.5f;
    sdfAOPass_.buffers.cBufferData.falloffPower = 1.0f;
    sdfAOPass_.buffers.cBufferData.voxelSize = 1.0f;
    sdfAOPass_.buffers.cBufferData.sdfTextureSize = config_->Sdf.Resolution;
    sdfAOPass_.buffers.cBufferData.minBounds = glm::vec4(-2.5f, 2.5f, 2.5f, 1.0f);
    sdfAOPass_.buffers.cBufferData.maxBounds = glm::vec4(2.5f, -2.5f, -2.5f, 1.0f);
    sdfAOPass_.buffers.cBufferData.noiseScale = glm::vec2(1.0f, 1.0f);
    memcpy(sdfAOPass_.buffers.cBuffer.mapped, &sdfAOPass_.buffers.cBufferData, sizeof(SdfAOPass::CBufferDesc));
}
void Renderer::AllocateDescriptorSetSdfAO()
{
    CreateFinalSDFTexture();
    CreateFinalSDFSampler(); // Create sampler for SDF AO Pass sampling

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{
        // camera
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        // world pos
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
        // normal
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
        // depth
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
        // SDF
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
        // noise texture
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
        // Camera
        Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 6),
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
    Tool::CheckResult(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &sdfAOPass_.setLayout));

    VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &sdfAOPass_.setLayout, 1);
    // Deferred composition
    Tool::CheckResult(vkAllocateDescriptorSets(m_device, &allocInfo, &sdfAOPass_.set));

    VkDescriptorImageInfo texDescriptorPosition = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorNormal = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[1].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo texDescriptorDepth = Init::descriptorImageInfo(
        m_framebuffers.deferred->sampler, m_framebuffers.deferred->attachments[4].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    std::vector<VkWriteDescriptorSet> writeDescriptorSets{
        // Binding 0: fragment uniform buffer(Camera Matrix)
        Init::writeDescriptorSet(sdfAOPass_.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &sdfAOPass_.buffers.cBuffer.descriptor),
        Init::writeDescriptorSet(sdfAOPass_.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorPosition),
        Init::writeDescriptorSet(sdfAOPass_.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorNormal),
        Init::writeDescriptorSet(sdfAOPass_.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texDescriptorDepth),
        Init::writeDescriptorSet(sdfAOPass_.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4,
                                 &m_multiViewDepthSDF4C.sdfFusionPass.finalSDFDescriptor),
        Init::writeDescriptorSet(sdfAOPass_.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, &m_blueNoise.descriptor),
        Init::writeDescriptorSet(sdfAOPass_.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6, &m_sharedBuffers.ConstBufferCamera.descriptor)};
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}

void Renderer::PreparePipelineSdfAO()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCI = Init::pipelineLayoutCreateInfo(&sdfAOPass_.setLayout, 1);

    Tool::CheckResult(vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &sdfAOPass_.pipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI =
        Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI =
        Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendStateCI = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI =
        Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportStateCI = Init::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleStateCI = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCI = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Main/SDFAO.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Main/SDFAO.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineVertexInputStateCreateInfo emptyInputState = Init::pipelineVertexInputStateCreateInfo();

    VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(sdfAOPass_.pipelineLayout, sdfAOPass_.frameBuffer->renderPass, 0);
    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pRasterizationState = &rasterizationStateCI;
    pipelineCI.pColorBlendState = &colorBlendStateCI;
    pipelineCI.pMultisampleState = &multisampleStateCI;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pDepthStencilState = &depthStencilStateCI;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &emptyInputState;

    Tool::CheckResult(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &sdfAOPass_.pipeline));
}
MeshToSdf* Renderer::GetMeshToSdfOperator()
{
    return meshToSdfOperator_;
};

void Renderer::MultiViewDepthSDF4C::cleanup(VkDevice device)
{
    // 清理模型静态数据
    staticData.modelVertexBuffer.Destroy();
    staticData.modelIndexBuffer.Destroy();
    staticData.modelPartsBuffer.Destroy();
    staticData.modelMatricesBuffer.Destroy();

    // 清理GPU数据准备阶段
    if (gpuPreparation.cameraMatrixPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, gpuPreparation.cameraMatrixPipeline, nullptr);
        gpuPreparation.cameraMatrixPipeline = VK_NULL_HANDLE;
    }
    if (gpuPreparation.cameraMatrixPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, gpuPreparation.cameraMatrixPipelineLayout, nullptr);
        gpuPreparation.cameraMatrixPipelineLayout = VK_NULL_HANDLE;
    }
    if (gpuPreparation.cameraMatrixDescriptorLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, gpuPreparation.cameraMatrixDescriptorLayout, nullptr);
        gpuPreparation.cameraMatrixDescriptorLayout = VK_NULL_HANDLE;
    }
    if (gpuPreparation.indirectCommandPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, gpuPreparation.indirectCommandPipeline, nullptr);
        gpuPreparation.indirectCommandPipeline = VK_NULL_HANDLE;
    }
    if (gpuPreparation.indirectCommandPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, gpuPreparation.indirectCommandPipelineLayout, nullptr);
        gpuPreparation.indirectCommandPipelineLayout = VK_NULL_HANDLE;
    }
    if (gpuPreparation.indirectCommandDescriptorLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, gpuPreparation.indirectCommandDescriptorLayout, nullptr);
        gpuPreparation.indirectCommandDescriptorLayout = VK_NULL_HANDLE;
    }
    gpuPreparation.cameraMatricesBuffer.Destroy();
    gpuPreparation.indirectCommandsBuffer.Destroy();
    gpuPreparation.activeCameraCountBuffer.Destroy();

    // 清理深度渲染阶段
    if (depthRendering.pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, depthRendering.pipeline, nullptr);
        depthRendering.pipeline = VK_NULL_HANDLE;
    }
    if (depthRendering.pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, depthRendering.pipelineLayout, nullptr);
        depthRendering.pipelineLayout = VK_NULL_HANDLE;
    }
    if (depthRendering.descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, depthRendering.descriptorSetLayout, nullptr);
        depthRendering.descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (depthRendering.renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device, depthRendering.renderPass, nullptr);
        depthRendering.renderPass = VK_NULL_HANDLE;
    }
    if (depthRendering.framebuffer != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(device, depthRendering.framebuffer, nullptr);
        depthRendering.framebuffer = VK_NULL_HANDLE;
    }
    if (depthRendering.depthCubemapArrayView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, depthRendering.depthCubemapArrayView, nullptr);
        depthRendering.depthCubemapArrayView = VK_NULL_HANDLE;
    }
    if (depthRendering.depthCubemapSamplingView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, depthRendering.depthCubemapSamplingView, nullptr);
        depthRendering.depthCubemapSamplingView = VK_NULL_HANDLE;
    }
    if (depthRendering.depthCubemapArray != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, depthRendering.depthCubemapArray, nullptr);
        depthRendering.depthCubemapArray = VK_NULL_HANDLE;
    }
    if (depthRendering.depthCubemapMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, depthRendering.depthCubemapMemory, nullptr);
        depthRendering.depthCubemapMemory = VK_NULL_HANDLE;
    }
    if (depthRendering.depthAttachmentView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, depthRendering.depthAttachmentView, nullptr);
        depthRendering.depthAttachmentView = VK_NULL_HANDLE;
    }
    if (depthRendering.depthAttachment != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, depthRendering.depthAttachment, nullptr);
        depthRendering.depthAttachment = VK_NULL_HANDLE;
    }
    if (depthRendering.depthAttachmentMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, depthRendering.depthAttachmentMemory, nullptr);
        depthRendering.depthAttachmentMemory = VK_NULL_HANDLE;
    }

    // 清理SDF融合阶段
    if (sdfFusionPass.computePipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, sdfFusionPass.computePipeline, nullptr);
        sdfFusionPass.computePipeline = VK_NULL_HANDLE;
    }
    if (sdfFusionPass.pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, sdfFusionPass.pipelineLayout, nullptr);
        sdfFusionPass.pipelineLayout = VK_NULL_HANDLE;
    }
    if (sdfFusionPass.descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, sdfFusionPass.descriptorSetLayout, nullptr);
        sdfFusionPass.descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (sdfFusionPass.depthCubemapSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device, sdfFusionPass.depthCubemapSampler, nullptr);
        sdfFusionPass.depthCubemapSampler = VK_NULL_HANDLE;
    }
    if (sdfFusionPass.depthCubemapArrayView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, sdfFusionPass.depthCubemapArrayView, nullptr);
        sdfFusionPass.depthCubemapArrayView = VK_NULL_HANDLE;
    }
    if (sdfFusionPass.finalSDFView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, sdfFusionPass.finalSDFView, nullptr);
        sdfFusionPass.finalSDFView = VK_NULL_HANDLE;
    }
    if (sdfFusionPass.finalSDFTexture != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, sdfFusionPass.finalSDFTexture, nullptr);
        sdfFusionPass.finalSDFTexture = VK_NULL_HANDLE;
    }
    if (sdfFusionPass.finalSDFMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, sdfFusionPass.finalSDFMemory, nullptr);
        sdfFusionPass.finalSDFMemory = VK_NULL_HANDLE;
    }
}

void Renderer::GPUDataPreparation::cleanup(VkDevice device)
{
    // 清理相机矩阵准备管线
    if (cameraMatrixPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, cameraMatrixPipeline, nullptr);
        cameraMatrixPipeline = VK_NULL_HANDLE;
    }
    if (cameraMatrixPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, cameraMatrixPipelineLayout, nullptr);
        cameraMatrixPipelineLayout = VK_NULL_HANDLE;
    }
    if (cameraMatrixDescriptorLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, cameraMatrixDescriptorLayout, nullptr);
        cameraMatrixDescriptorLayout = VK_NULL_HANDLE;
    }

    // 清理间接命令生成管线
    if (indirectCommandPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, indirectCommandPipeline, nullptr);
        indirectCommandPipeline = VK_NULL_HANDLE;
    }
    if (indirectCommandPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, indirectCommandPipelineLayout, nullptr);
        indirectCommandPipelineLayout = VK_NULL_HANDLE;
    }
    if (indirectCommandDescriptorLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, indirectCommandDescriptorLayout, nullptr);
        indirectCommandDescriptorLayout = VK_NULL_HANDLE;
    }

    // 清理缓冲区
    cameraMatricesBuffer_GPU.Destroy();
    activeCameraCountBuffer_GPU.Destroy();
    indirectDrawBuffer_GPU.Destroy();
}