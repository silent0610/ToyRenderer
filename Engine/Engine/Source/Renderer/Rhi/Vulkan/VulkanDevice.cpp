module;
#include "vulkan/vulkan.h"
#include <GLFW/glfw3.h>
#include "spdlog/spdlog.h"
module Engine.Rhi.Vulkan.Device;
import Engine.Rhi.Swapchain;
import Engine.Rhi.CommandList;
import Engine.Rhi.Vulkan.Tool;
import Engine.Rhi.Vulkan.Swapchain;
import Engine.Rhi.Vulkan.CommandList;
import Engine.Rhi.Vulkan.Pipeline;
import Engine.Rhi.Vulkan.Shader;
import Engine.Rhi.Vulkan.Texture;
import Engine.Rhi.Vulkan.Sync;
import Engine.Rhi.Vulkan.Buffer;
import Engine.Rhi.Vulkan.Sampler;
import Engine.Rhi.Vulkan.Descriptor;

import Engine.Rhi.Buffer;
namespace Engine::Rhi
{
    std::unique_ptr<RhiDevice> CreateDevice(GraphicsBackend backend)
    {
        switch (backend)
        {
        case GraphicsBackend::Vulkan:
            spdlog::info("Chose Vulkan Backend");
            return std::make_unique<VulkanDevice>();
        case GraphicsBackend::DX12:
            spdlog::error("DX12 not supported");
            return nullptr;
        case GraphicsBackend::DX11:
            spdlog::error("DX11 not supported");
            return nullptr;
        case GraphicsBackend::Metal:
            spdlog::error("Metal not supported");
            return nullptr;
        }

        return nullptr;
    }
    std::unique_ptr<Pipeline> VulkanDevice::CreatePipeline(const PipelineStateDesc &desc)
    {
        // 在这里，我们确切地知道要创建 VulkanPipeline
        // *this 会自动匹配构造函数需要的 VulkanDevice&
        return std::make_unique<VulkanPipeline>(this, desc);
    }
    std::unique_ptr<RhiShader> VulkanDevice::CreateShader(const std::string_view filePath, ShaderStage stage)
    {

        return std::make_unique<VulkanShader>(this, filePath, stage);
    }
    VulkanDevice::~VulkanDevice()
    {
        if (globalDescriptorPool_)
        {
            vkDestroyDescriptorPool(device_, globalDescriptorPool_, nullptr);
        }
        if (transientPools_[0])
        {
            for (int i = 0; i < kMaxFramesInFlight_; ++i)
            {
                vkDestroyDescriptorPool(device_, transientPools_[i], nullptr);
            }
            
        }
        if (device_)
        {
            vkDeviceWaitIdle(device_);
            vkDestroyDevice(device_, nullptr);
        }

        if (enableValidationLayer_ && debugMessenger_)
        {
            // 扩展函数需要手动加载指针
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
            if (func)
                func(instance_, debugMessenger_, nullptr);
        }

        if (instance_)
        {
            vkDestroyInstance(instance_, nullptr);
        }
    }
    void VulkanDevice::Execute(const QueueSubmitInfo &info)
    {

        auto vkCmdList = static_cast<VulkanCommandList *>(info.CmdList);
        VkCommandBuffer commandBuffer = vkCmdList->GetCommandBuffer();
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        VkSemaphore waitSema = VK_NULL_HANDLE;

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        if (info.WaitSemaphore)
        {
            waitSema = static_cast<VulkanSemaphore *>(info.WaitSemaphore)->GetHandle();
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &waitSema;

            waitStage = Tool::ConvertWaitStage(info.WaitStageMask);
            submitInfo.pWaitDstStageMask = &waitStage;
        }
        VkSemaphore signalSema = VK_NULL_HANDLE;
        if (info.SignalSemaphore)
        {
            signalSema = static_cast<VulkanSemaphore *>(info.SignalSemaphore)->GetHandle();
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &signalSema;
        }
        VkFence fence = VK_NULL_HANDLE;
        if (info.SignalFence)
        {
            fence = static_cast<VulkanFence *>(info.SignalFence)->GetHandle();
        }
        Tool::CheckResult(vkQueueSubmit(graphicsQueue_, 1, &submitInfo, fence));
    }
    bool VulkanDevice::Init(const DeviceDesc &desc)
    {
        spdlog::info("Init Device");
        enableValidationLayer_ = desc.DebugLayer;

        if (!CreateInstance(enableValidationLayer_))
        {
            return false;
        }

        if (enableValidationLayer_ && !SetupDebugMessenger())
        {
            spdlog::info("enable validation layer");
        }
        if (!PickPhysicalDevice())
        {
            return false;
        }
        if (!CreateLogicalDevice())
        {
            return false;
        }
        CreateDescriptorPool();
        CreateTransientPools();
        return true;
    }
    void VulkanDevice::CreateDescriptorPool()
    {
        // 这是一个简单的“大”池子，能容纳一定数量的常见资源
        std::vector<VkDescriptorPoolSize> poolSizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                                       {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                                       {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
                                                       {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100}};

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 1000; // 最多能分配 1000 个 Set
        // 允许单独释放 Set (虽然 MVP 暂时没用到)
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        Tool::CheckResult(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &globalDescriptorPool_));
    }
    void VulkanDevice::WaitIdle()
    {
        if (device_)
        {
            vkDeviceWaitIdle(device_);
        }
    }

    std::unique_ptr<RhiSwapchain> VulkanDevice::CreateSwapchain(void *windowHandle, uint32_t width, uint32_t height, PixelFormat format)
    {
        Engine::Rhi::VulkanSwapchain *swapchain = new Engine::Rhi::VulkanSwapchain{this, windowHandle, width, height};

        // 2. 显式移动并转换为基类指针 (Move语义)
        return std::unique_ptr<RhiSwapchain>(std::move(swapchain));
    }

    std::shared_ptr<CommandList> VulkanDevice::CreateCommandList()
    {
        auto cmdList = std::make_shared<VulkanCommandList>(this);
        return cmdList;
    }

    bool VulkanDevice::CreateInstance(bool enableValidation)
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_4;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // 让 GLFW 告诉我们需要什么扩展
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions{nullptr};
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidation)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        createInfo.enabledExtensionCount =
            static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (enableValidation)
        {
            createInfo.enabledLayerCount =
                static_cast<uint32_t>(validationLayers_.size());
            createInfo.ppEnabledLayerNames = validationLayers_.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }
        Engine::Rhi::Tool::CheckResult(vkCreateInstance(&createInfo, nullptr, &instance_));
        return true;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
    {
        // 1. 过滤：只处理警告(Warning)和错误(Error)
        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {

            spdlog::error("[Validation Layer]: {}", pCallbackData->pMessage);
        }

        // 3. 中断：只有当严重程度是“错误(Error)”时，才暂停程序
        // 这样不会因为普通的性能警告而频繁打断你
        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) // 只在 Error 时中断
        {
            __debugbreak();
        }

        return VK_FALSE;
    }
    bool VulkanDevice::SetupDebugMessenger()
    {
        if (!enableValidation_)
        {
            return true;
        }

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback;

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            return func(instance_, &createInfo, nullptr, &debugMessenger_) == VK_SUCCESS;
        }
        return false;
    }

    std::optional<uint32_t> VulkanDevice::FindQueueFamilies(VkPhysicalDevice device)
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        // 寻找支持 Graphics 的队列
        for (int i = 0; i < queueFamilies.size(); i++)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                return i;
            }
        }
        return std::nullopt;
    }

    bool VulkanDevice::PickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
        if (deviceCount == 0)
            return false;

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

        // 简单选择策略：找第一个支持 Graphics 队列的设备
        // (实际引擎中会根据显存大小和是否是独显来打分)
        for (const auto &device : devices)
        {
            if (FindQueueFamilies(device).has_value())
            {
                physicalDevice_ = device;
                break;
            }
        }

        if (physicalDevice_ == VK_NULL_HANDLE)
        {
            std::cerr << "Failed to find a suitable GPU!" << std::endl;
            return false;
        }
        return true;
    }

    bool VulkanDevice::CreateLogicalDevice()
    {
        auto indices = FindQueueFamilies(physicalDevice_);

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.value();
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        // 物理设备特性 (目前不需要特殊特性)
        VkPhysicalDeviceFeatures deviceFeatures{};
        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE; // 🔥 必须开启
        features13.synchronization2 = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.pNext = &features13;

        // 开启 Swapchain 扩展
        createInfo.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions_.size());
        createInfo.ppEnabledExtensionNames = kDeviceExtensions_.data();

        Engine::Rhi::Tool::CheckResult(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_));

        // 获取队列句柄
        vkGetDeviceQueue(device_, indices.value(), 0, &graphicsQueue_);
        graphicsQueueFamilyIndex_ = indices.value();

        return true;
    }
    VkInstance VulkanDevice::GetInstance() const
    {
        return instance_;
    }

    VkPhysicalDevice VulkanDevice::GetPhysicalDevice() const
    {
        return physicalDevice_;
    }

    VkDevice VulkanDevice::GetDevice() const
    {
        return device_;
    }

    VkQueue VulkanDevice::GetGraphicsQueue() const
    {
        return graphicsQueue_;
    }

    uint32_t VulkanDevice::GetGraphicsQueueFamilyIndex() const
    {
        return graphicsQueueFamilyIndex_;
    }
    uint32_t VulkanDevice::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            // 1. 检查 typeFilter 的第 i 位是否为 1 (是否是 Image 支持的类型)
            // 2. 检查该类型的属性是否满足 properties (比如必须是 DeviceLocal)
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }

    std::unique_ptr<RhiTexture> VulkanDevice::CreateTexture(const TextureDesc &desc)
    {
        return std::make_unique<VulkanTexture>(this, desc);
    }
    std::unique_ptr<RhiSemaphore> VulkanDevice::CreateSyncSemaphore()
    {
        return std::make_unique<VulkanSemaphore>(device_);
    }
    std::unique_ptr<RhiFence> VulkanDevice::CreateSyncFence(bool signaled)
    {
        return std::make_unique<VulkanFence>(device_, signaled);
    }
    std::unique_ptr<RhiBuffer> VulkanDevice::CreateBuffer(const BufferDesc &desc)
    {
        return std::make_unique<VulkanBuffer>(this, desc);
    }
    void VulkanDevice::CopyBufferImmediate(RhiBuffer *src, RhiBuffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
    {
        auto cmd = CreateCommandList();

        // 2. 录制
        cmd->Begin();
        cmd->CopyBuffer(src, dst, size, srcOffset, dstOffset);
        cmd->End();

        // 3. 提交
        // Immediate 模式不需要信号量同步，因为它马上就要死等了
        QueueSubmitInfo submitInfo{};
        submitInfo.CmdList = cmd.get();

        Execute(submitInfo);

        // 4. 等 GPU 完成
        // 必须等待，否则函数返回后，cmd 被析构，src 内存（如果是 Staging）被释放，GPU 还没拷完就崩了
        vkQueueWaitIdle(graphicsQueue_);
    }

    std::unique_ptr<Sampler> VulkanDevice::CreateSampler(const SamplerDesc &desc) { return std::make_unique<VulkanSampler>(this, desc); }

    std::unique_ptr<DescriptorSetLayout> VulkanDevice::CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc)
    {
        return std::make_unique<VulkanDescriptorSetLayout>(this, desc);
    }

    std::unique_ptr<RhiDescriptorSet> VulkanDevice::CreateDescriptorSet(const DescriptorSetLayout* layout, bool isTransit)
    {

        auto vkLayout = static_cast<const VulkanDescriptorSetLayout*>(layout);
        VkDescriptorSetLayout layoutHandle = vkLayout->GetHandle();

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

        allocInfo.descriptorPool = isTransit ? transientPools_[currentFrameIndex_]:globalDescriptorPool_; // 从全局池分配
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layoutHandle;

        VkDescriptorSet setHandle;
        Tool::CheckResult(vkAllocateDescriptorSets(device_, &allocInfo, &setHandle));

        return std::make_unique<VulkanDescriptorSet>(this, setHandle, globalDescriptorPool_, isTransit);
    }
    void VulkanDevice::UploadTextureData(const RhiTexture* texture, const void* data, uint64_t size)
    {
        // 1. 创建 Staging Buffer (临时)
        BufferDesc stageDesc;
        stageDesc.Size = size;
        stageDesc.Usage = BufferUsage::TransferSrc;
        stageDesc.MemoryUsage = BufferMemoryUsage::CpuToGpu;
        auto stagingBuffer = CreateBuffer(stageDesc);
        stagingBuffer->WriteData(data, size);

        auto cmdList = CreateCommandList();
        cmdList->Begin();
        

        TextureBarrier barrier1{const_cast<RhiTexture*>(texture), ResourceState::Undefined,ResourceState::TransferDst};
        cmdList->SetResourceBarrier(barrier1);

        cmdList->CopyBufferToTexture(stagingBuffer.get(), texture, {0,texture->GetWidth(), texture->GetHeight(), 1});

        TextureBarrier barrier2{const_cast<RhiTexture*>(texture), ResourceState::TransferDst, ResourceState::ShaderResource};
        cmdList->SetResourceBarrier(barrier2);
        cmdList->End();
        QueueSubmitInfo submit{};
        submit.CmdList = cmdList.get();
        submit.SignalFence = nullptr;
        submit.WaitSemaphore = nullptr;
        submit.SignalSemaphore = nullptr;
        submit.WaitStageMask = WaitStage::AllCommands;
        Execute(submit);
        WaitIdle();
    }

    void VulkanDevice::UploadBufferData(RhiBuffer* buffer, const void* data, uint64_t size)
    {
        // 1. 创建 Staging Buffer (临时)
        BufferDesc stageDesc;
        stageDesc.Size = size;
        stageDesc.Usage = BufferUsage::TransferSrc;
        stageDesc.MemoryUsage = BufferMemoryUsage::CpuToGpu;
        auto stagingBuffer = CreateBuffer(stageDesc);
        stagingBuffer->WriteData(data, size);

        // 2. 拷贝 Staging -> Dest
        CopyBufferImmediate(stagingBuffer.get(), buffer, size, 0, 0);
    }
    void VulkanDevice::CreateTransientPools()
    {
        for (uint32_t i = 0; i < kMaxFramesInFlight_; i++)
        {
            std::vector<VkDescriptorPoolSize> poolSizes = {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 50},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 50}};
            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            poolInfo.maxSets = 100; // 最多能分配 100 个 Set
            // 允许单独释放 Set
            // poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            Tool::CheckResult(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &transientPools_[i]));
        }
    }
    void VulkanDevice::BeginFrame(uint32_t frameIndex)
    {
        currentFrameIndex_ = frameIndex;
        vkResetDescriptorPool(device_, transientPools_[currentFrameIndex_], 0);
    }
}

