module;
#include "vulkan/vulkan.h"
#include "GLFW/glfw3.h"

module VulkanDevice;
import VulkanUtils;
import VulkanBuffer;
import VulkanCommandBuffer;
import VulkanRenderPass;
import VulkanPipeline;
import VulkanDescriptor;
import Logger;
import ToolMod;
import VulkanTexture;
import VulkanDepthBuffer;
import std;

VulkanDevice::VulkanDevice(const RhiDeviceDesc &desc)
    : window_(static_cast<GLFWwindow *>(desc.windowHandle)),
      currentFrame_(0), currentImageIndex_(0)
{
    // Initialize Vulkan step by step
    Log::Info("[VulkanDevice] Creating Vulkan instance...");
    instance_ = VulkanUtils::CreateInstance(desc.applicationName, desc.enableValidation);
    if (instance_ == VK_NULL_HANDLE)
    {
        Log::Error("[VulkanDevice] Failed to create Vulkan instance");
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    Log::Info("[VulkanDevice] Creating surface...");
    surface_ = VulkanUtils::CreateSurface(instance_, desc.windowHandle);
    if (surface_ == VK_NULL_HANDLE)
    {
        Log::Error("[VulkanDevice] Failed to create window surface");
        throw std::runtime_error("Failed to create window surface");
    }

    Log::Info("[VulkanDevice] Picking physical device...");
    physicalDevice_ = VulkanUtils::PickPhysicalDevice(instance_, surface_);
    if (physicalDevice_ == VK_NULL_HANDLE)
    {
        Log::Error("[VulkanDevice] Failed to find a suitable GPU");
        throw std::runtime_error("Failed to find a suitable GPU");
    }

    Log::Info("[VulkanDevice] Finding queue families...");
    queueIndices_ = VulkanUtils::FindQueueFamilies(physicalDevice_, surface_);
    if (!queueIndices_.IsComplete())
    {
        Log::Error("[VulkanDevice] Failed to find suitable queue families");
        throw std::runtime_error("Failed to find suitable queue families");
    }

    Log::Info("[VulkanDevice] Creating logical device...");
    device_ = VulkanUtils::CreateLogicalDevice(physicalDevice_, queueIndices_, desc.enableValidation);
    if (device_ == VK_NULL_HANDLE)
    {
        Log::Error("[VulkanDevice] Failed to create logical device");
        throw std::runtime_error("Failed to create logical device");
    }

    // Get queue handles
    vkGetDeviceQueue(device_, queueIndices_.graphicsFamily, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, queueIndices_.presentFamily, 0, &presentQueue_);

    // Create swapchain and other resources
    if (!CreateSwapchain() || !CreateImageViews() || !CreateCommandPool() || !CreateSyncObjects() || !CreateDefaultRenderPass() || !CreateStagingBuffer())
    {
        Log::Error("[VulkanDevice] Failed to create Vulkan resources");
        throw std::runtime_error("Failed to create Vulkan resources");
    }

    Log::Info("[VulkanDevice] Vulkan device created successfully!");
}

VulkanDevice::~VulkanDevice()
{
    if (device_ != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device_);

        // Cleanup sync objects
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
            vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
            vkDestroyFence(device_, inFlightFences_[i], nullptr);
        }

        CleanupSwapchain();

        if (commandPool_ != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device_, commandPool_, nullptr);
        }

        if (defaultRenderPass_ != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device_, defaultRenderPass_, nullptr);
        }

        // Cleanup staging buffer
        DestroyStagingBuffer();

        vkDestroyDevice(device_, nullptr);
    }

    if (surface_ != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }

    if (instance_ != VK_NULL_HANDLE)
    {
        vkDestroyInstance(instance_, nullptr);
    }
}

bool VulkanDevice::CreateSwapchain()
{
    VulkanUtils::SwapchainSupportDetails swapchainSupport =
        VulkanUtils::QuerySwapchainSupport(physicalDevice_, surface_);

    VkSurfaceFormatKHR surfaceFormat = VulkanUtils::ChooseSwapSurfaceFormat(swapchainSupport.formats);
    VkPresentModeKHR presentMode = VulkanUtils::ChooseSwapPresentMode(swapchainSupport.presentModes);

    int width, height;
    glfwGetFramebufferSize(window_, &width, &height);
    VkExtent2D extent = VulkanUtils::ChooseSwapExtent(swapchainSupport.capabilities, width, height);

    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapchainSupport.capabilities.maxImageCount)
    {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {queueIndices_.graphicsFamily, queueIndices_.presentFamily};

    if (queueIndices_.graphicsFamily != queueIndices_.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) != VK_SUCCESS)
    {
        return false;
    }

    // Store swapchain details
    swapchainFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;

    // Get swapchain images
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());

    return true;
}

bool VulkanDevice::CreateImageViews()
{
    swapchainImageViews_.resize(swapchainImages_.size());

    for (size_t i = 0; i < swapchainImages_.size(); i++)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainFormat_;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device_, &createInfo, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS)
        {
            return false;
        }
    }

    return true;
}

bool VulkanDevice::CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueIndices_.graphicsFamily;

    return vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) == VK_SUCCESS;
}

bool VulkanDevice::CreateSyncObjects()
{
    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS)
        {
            return false;
        }
    }

    return true;
}

bool VulkanDevice::CreateDefaultRenderPass()
{
    // Create a simple default render pass compatible with our swapchain
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &defaultRenderPass_) != VK_SUCCESS)
    {
        Log::Error("Failed to create default render pass");
        return false;
    }

    Log::Debug("Default render pass created");
    return true;
}

VkRenderPass VulkanDevice::CreateCompatibleRenderPass(const RhiGraphicsPipelineDesc& desc)
{
    // Create a render pass that exactly matches RenderTargetManager's format:
    // - Color attachment with ShaderReadOnlyOptimal final layout
    // - Depth attachment (always present to match RenderTargetManager)
    // The depth testing enable/disable is a pipeline state, not render pass compatibility issue
    
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colorAttachmentRefs;
    
    // Color attachment (always present)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_; // Use swapchain format for compatibility
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Use appropriate final layout based on usage
    // For swapchain rendering, we need VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    // For off-screen rendering, we need VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Match swapchain VulkanRenderPass
    attachments.push_back(colorAttachment);
    
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentRefs.push_back(colorAttachmentRef);
    
    // Depth attachment (always present to match RenderTargetManager)
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT; // Match RenderTargetManager
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments.push_back(depthAttachment);
    
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
    subpass.pColorAttachments = colorAttachmentRefs.data();
    subpass.pDepthStencilAttachment = &depthAttachmentRef; // Always present
    
    // Subpass dependencies (exactly match VulkanRenderPass)
    std::vector<VkSubpassDependency> dependencies;
    
    // Color attachment dependency
    VkSubpassDependency colorDependency{};
    colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    colorDependency.dstSubpass = 0;
    colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.srcAccessMask = 0;
    colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies.push_back(colorDependency);
    
    // Depth attachment dependency (always present to match RenderTargetManager)
    VkSubpassDependency depthDependency{};
    depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depthDependency.dstSubpass = 0;
    depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.srcAccessMask = 0;
    depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies.push_back(depthDependency);
    
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();
    
    VkRenderPass renderPass;
    if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        Log::Error("Failed to create compatible render pass for pipeline");
        return VK_NULL_HANDLE;
    }
    
    Log::Debug("Created compatible render pass with color + depth attachments (matches RenderTargetManager)");
    return renderPass;
}

void VulkanDevice::CleanupSwapchain()
{
    for (auto imageView : swapchainImageViews_)
    {
        vkDestroyImageView(device_, imageView, nullptr);
    }

    if (swapchain_ != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    }
}

// RHI Interface implementations
Core::UniquePtr<RhiBuffer> VulkanDevice::CreateBuffer(const RhiBufferDesc &desc)
{
    VkBuffer buffer;
    VkDeviceMemory memory;

    // Create buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc.size;

    // Convert RHI buffer usage to Vulkan usage flags
    VkBufferUsageFlags vkUsage = 0;
    if (static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(RhiBufferUsage::Vertex))
    {
        vkUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(RhiBufferUsage::Index))
    {
        vkUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(RhiBufferUsage::Uniform))
    {
        vkUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if (static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(RhiBufferUsage::Storage))
    {
        vkUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if (static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(RhiBufferUsage::TransferSrc))
    {
        vkUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }
    if (static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(RhiBufferUsage::TransferDst))
    {
        vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    // If no usage specified, default to vertex+index+uniform for compatibility
    if (vkUsage == 0)
    {
        vkUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        Log::Warn("No buffer usage specified, using default: Vertex|Index|Uniform");
    }
    
    // For GPU_Only and CPU_TO_GPU buffers, automatically add transfer destination usage for uploading data
    if (desc.memoryUsage == RhiMemoryUsage::GPU_Only || desc.memoryUsage == RhiMemoryUsage::CPU_TO_GPU) {
        vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    bufferInfo.usage = vkUsage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        Log::Error("Failed to create VkBuffer");
        return nullptr;
    }

    // Get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

    // Find suitable memory type (host visible and coherent)
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);

    uint32_t memoryType = UINT32_MAX;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            memoryType = i;
            break;
        }
    }

    if (memoryType == UINT32_MAX)
    {
        Log::Error("Failed to find suitable memory type for buffer");
        vkDestroyBuffer(device_, buffer, nullptr);
        return nullptr;
    }

    // Allocate memory
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryType;

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS)
    {
        Log::Error("Failed to allocate buffer memory");
        vkDestroyBuffer(device_, buffer, nullptr);
        return nullptr;
    }

    // Bind buffer to memory
    vkBindBufferMemory(device_, buffer, memory, 0);

    Log::Debug(std::format("VulkanBuffer created: size={} bytes", desc.size));
    return Core::MakeUnique<VulkanBuffer>(device_, buffer, memory, desc.size, desc.usage);
}

Core::UniquePtr<RhiCommandBuffer> VulkanDevice::CreateCommandBuffer()
{
    VkCommandBuffer commandBuffer;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer) != VK_SUCCESS)
    {
        Log::Error("Failed to allocate command buffer");
        return nullptr;
    }

    Log::Debug("VulkanCommandBuffer created");
    return Core::MakeUnique<VulkanCommandBuffer>(device_, commandBuffer);
}

Core::UniquePtr<RhiPipeline> VulkanDevice::CreateGraphicsPipeline()
{
    // Simple hard-coded triangle pipeline

    // Vertex shader source (GLSL)
    const char *vertexShaderSrc = R"glsl(
    #version 450
    
    vec2 positions[3] = vec2[](
        vec2(0.0, -0.5),
        vec2(0.5, 0.5),
        vec2(-0.5, 0.5)
    );
    
    vec3 colors[3] = vec3[](
        vec3(1.0, 0.0, 0.0),  // red
        vec3(0.0, 1.0, 0.0),  // green  
        vec3(0.0, 0.0, 1.0)   // blue
    );
    
    layout(location = 0) out vec3 fragColor;
    
    void main() {
        gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
        fragColor = colors[gl_VertexIndex];
    }
    )glsl";

    // Fragment shader source (GLSL)
    const char *fragmentShaderSrc = R"glsl(
    #version 450
    
    layout(location = 0) in vec3 fragColor;
    layout(location = 0) out vec4 outColor;
    
    void main() {
        outColor = vec4(fragColor, 1.0);
    }
    )glsl";

    // Load compiled SPIR-V shaders
    auto loadShader = [&](const std::string &filename) -> std::vector<uint32_t>
    {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            Log::Error(std::format("Failed to open shader file: {}", filename));
            return {};
        }

        size_t fileSize = file.tellg();
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

        file.seekg(0);
        file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
        file.close();

        Log::Debug(std::format("Loaded shader: {} ({} bytes)", filename, fileSize));
        return buffer;
    };

    // Load vertex and fragment shaders
    auto vertexShaderCode = loadShader("E:/All/Projects/MyToyRenderer/Renderer/Shader/Triangle.Vert.spv");
    auto fragmentShaderCode = loadShader("E:/All/Projects/MyToyRenderer/Renderer/Shader/Triangle.Frag.spv");

    if (vertexShaderCode.empty() || fragmentShaderCode.empty())
    {
        Log::Error("Failed to load triangle shaders");
        return nullptr;
    }

    // Create pipeline layout (empty for simple triangle)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        Log::Error("Failed to create pipeline layout");
        return nullptr;
    }

    // Create shader modules
    auto createShaderModule = [&](const std::vector<uint32_t> &code) -> VkShaderModule
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size() * sizeof(uint32_t);
        createInfo.pCode = code.data();

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            Log::Error("Failed to create shader module");
            return VK_NULL_HANDLE;
        }
        return shaderModule;
    };

    VkShaderModule vertShaderModule = createShaderModule(vertexShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragmentShaderCode);

    if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE)
    {
        Log::Error("Failed to create shader modules");
        if (vertShaderModule != VK_NULL_HANDLE)
            vkDestroyShaderModule(device_, vertShaderModule, nullptr);
        if (fragShaderModule != VK_NULL_HANDLE)
            vkDestroyShaderModule(device_, fragShaderModule, nullptr);
        vkDestroyPipelineLayout(device_, pipelineLayout, nullptr);
        return nullptr;
    }

    // Shader stage creation
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input (no vertex buffers for hardcoded triangle)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (will be dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = defaultRenderPass_;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
    {
        Log::Error("Failed to create graphics pipeline");
        vkDestroyShaderModule(device_, vertShaderModule, nullptr);
        vkDestroyShaderModule(device_, fragShaderModule, nullptr);
        vkDestroyPipelineLayout(device_, pipelineLayout, nullptr);
        return nullptr;
    }

    // Cleanup shader modules (they are no longer needed after pipeline creation)
    vkDestroyShaderModule(device_, vertShaderModule, nullptr);
    vkDestroyShaderModule(device_, fragShaderModule, nullptr);

    Log::Info("Graphics pipeline created successfully");
    return Core::MakeUnique<VulkanPipeline>(device_, pipeline, pipelineLayout);
}

Core::UniquePtr<RhiRenderPass> VulkanDevice::CreateRenderPass(uint32_t width, uint32_t height, RhiFormat colorFormat)
{
    auto renderPass = Core::MakeUnique<VulkanRenderPass>(device_, width, height, colorFormat);

    if (!renderPass->Initialize(swapchainImageViews_))
    {
        Log::Error("Failed to initialize VulkanRenderPass");
        return nullptr;
    }

    Log::Debug(std::format("VulkanRenderPass created: {}x{}", width, height));
    return renderPass;
}

RhiResult VulkanDevice::Submit(RhiCommandBuffer *commandBuffer)
{
    if (!commandBuffer)
    {
        Log::Error("Attempted to submit null command buffer");
        return RhiResult::ErrorInvalidParameter;
    }

    // Cast to VulkanCommandBuffer to get native handle
    auto *vulkanCmd = static_cast<VulkanCommandBuffer *>(commandBuffer);
    VkCommandBuffer vkCmd = vulkanCmd->GetVkCommandBuffer();

    // Submit the command buffer (fence waiting is now handled in AcquireNextImage)
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vkCmd;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[currentFrame_]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]) != VK_SUCCESS)
    {
        Log::Error("Failed to submit command buffer to graphics queue");
        return RhiResult::ErrorDeviceLost;
    }

    Log::Debug(std::format("Command buffer submitted for frame {}", currentFrame_));
    return RhiResult::Success;
}

RhiResult VulkanDevice::AcquireNextImage()
{
    // Wait for the current frame's fence to ensure previous use of this frame's resources is complete
    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

    // Acquire next swapchain image
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                            imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        Log::Warn("Swapchain out of date, need recreation");
        return RhiResult::ErrorDeviceLost;
    }
    else if (result != VK_SUCCESS)
    {
        Log::Error("Failed to acquire swapchain image");
        return RhiResult::ErrorDeviceLost;
    }

    // Store the acquired image index for present
    currentImageIndex_ = imageIndex;
    Log::Debug(std::format("Acquired swapchain image index: {} for frame {}", imageIndex, currentFrame_));
    return RhiResult::Success;
}

RhiResult VulkanDevice::Present()
{
    // Present the image
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_];

    VkSwapchainKHR swapchains[] = {swapchain_};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &currentImageIndex_;

    VkResult result = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        Log::Warn("Swapchain out of date at present, need recreation");
        return RhiResult::ErrorDeviceLost;
    }
    else if (result != VK_SUCCESS)
    {
        Log::Error("Failed to present swapchain image");
        return RhiResult::ErrorDeviceLost;
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    Log::Debug(std::format("Frame presented, advancing to frame {}", currentFrame_));
    return RhiResult::Success;
}

RhiResult VulkanDevice::WaitIdle()
{
    vkDeviceWaitIdle(device_);
    return RhiResult::Success;
}

RhiFormat VulkanDevice::GetSwapchainFormat() const
{
    // Convert Vulkan format to RHI format - preserve SRGB vs UNORM distinction
    switch (swapchainFormat_)
    {
    case VK_FORMAT_B8G8R8A8_SRGB:
        return RhiFormat::B8G8R8A8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:
        return RhiFormat::B8G8R8A8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB:
        return RhiFormat::R8G8B8A8_SRGB;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return RhiFormat::R8G8B8A8_UNORM;
    default:
        return RhiFormat::Undefined;
    }
}

void VulkanDevice::GetSwapchainExtent(uint32_t &width, uint32_t &height) const
{
    width = swapchainExtent_.width;
    height = swapchainExtent_.height;
}

// Descriptor Set system implementation
Core::UniquePtr<RhiDescriptorSetLayout> VulkanDevice::CreateDescriptorSetLayout(const RhiDescriptorSetLayoutDesc &desc)
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (const auto &binding : desc.bindings)
    {
        VkDescriptorSetLayoutBinding vkBinding{};
        vkBinding.binding = binding.binding;
        vkBinding.descriptorCount = binding.descriptorCount;

        // Convert RHI descriptor type to Vulkan
        switch (binding.descriptorType)
        {
        case RhiDescriptorType::UniformBuffer:
            vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
        case RhiDescriptorType::StorageBuffer:
            vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        case RhiDescriptorType::CombinedImageSampler:
            vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            break;
        case RhiDescriptorType::StorageImage:
            vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            break;
        }

        // Convert RHI shader stage flags to Vulkan
        vkBinding.stageFlags = 0;
        if (static_cast<uint32_t>(binding.stageFlags) & static_cast<uint32_t>(RhiShaderStageFlags::Vertex))
        {
            vkBinding.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
        }
        if (static_cast<uint32_t>(binding.stageFlags) & static_cast<uint32_t>(RhiShaderStageFlags::Fragment))
        {
            vkBinding.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        if (static_cast<uint32_t>(binding.stageFlags) & static_cast<uint32_t>(RhiShaderStageFlags::Compute))
        {
            vkBinding.stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
        }

        bindings.push_back(vkBinding);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &layout) != VK_SUCCESS)
    {
        Log::Error("Failed to create descriptor set layout");
        return nullptr;
    }

    Log::Debug(std::format("Created descriptor set layout with {} bindings", bindings.size()));
    return Core::MakeUnique<VulkanDescriptorSetLayout>(layout, device_);
}

Core::UniquePtr<RhiDescriptorPool> VulkanDevice::CreateDescriptorPool(const RhiDescriptorPoolDesc &desc)
{
    std::vector<VkDescriptorPoolSize> poolSizes;

    for (const auto &size : desc.poolSizes)
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.descriptorCount = size.descriptorCount;

        // Convert RHI descriptor type to Vulkan
        switch (size.type)
        {
        case RhiDescriptorType::UniformBuffer:
            poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
        case RhiDescriptorType::StorageBuffer:
            poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        case RhiDescriptorType::CombinedImageSampler:
            poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            break;
        case RhiDescriptorType::StorageImage:
            poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            break;
        }

        poolSizes.push_back(poolSize);
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = desc.maxSets;

    VkDescriptorPool pool;
    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool) != VK_SUCCESS)
    {
        Log::Error("Failed to create descriptor pool");
        return nullptr;
    }

    Log::Debug(std::format("Created descriptor pool (max sets: {})", desc.maxSets));
    return Core::MakeUnique<VulkanDescriptorPool>(pool, device_);
}

Core::UniquePtr<RhiDescriptorSet> VulkanDevice::AllocateDescriptorSet(RhiDescriptorPool *pool, RhiDescriptorSetLayout *layout)
{
    auto *vulkanPool = static_cast<VulkanDescriptorPool *>(pool);
    auto *vulkanLayout = static_cast<VulkanDescriptorSetLayout *>(layout);

    VkDescriptorSetLayout vkLayout = vulkanLayout->GetVkDescriptorSetLayout();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = vulkanPool->GetVkDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &vkLayout;

    VkDescriptorSet set;
    if (vkAllocateDescriptorSets(device_, &allocInfo, &set) != VK_SUCCESS)
    {
        Log::Error("Failed to allocate descriptor set");
        return nullptr;
    }

    Log::Debug("Allocated descriptor set");
    return Core::MakeUnique<VulkanDescriptorSet>(set);
}

void VulkanDevice::UpdateDescriptorSet(RhiDescriptorSet *descriptorSet, uint32_t binding, RhiBuffer *buffer)
{
    auto *vulkanSet = static_cast<VulkanDescriptorSet *>(descriptorSet);
    auto *vulkanBuffer = static_cast<VulkanBuffer *>(buffer);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = vulkanBuffer->GetVkBuffer();
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = vulkanSet->GetVkDescriptorSet();
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // Assuming uniform buffer for now
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
    Log::Debug(std::format("Updated descriptor set binding {}", binding));
}

Core::UniquePtr<RhiRenderPass> VulkanDevice::CreateRenderPass(const RhiRenderPassDesc &desc)
{
    // Determine depth format if depth attachment is requested
    RhiFormat depthFormat = RhiFormat::Undefined;
    if (desc.hasDepthAttachment)
    {
        depthFormat = desc.depthAttachment.format != RhiFormat::Undefined
                          ? desc.depthAttachment.format
                          : RhiFormat::D24_UNORM_S8_UINT; // Default depth format
    }

    // For now, use the first color attachment's format if available
    RhiFormat colorFormat = RhiFormat::B8G8R8A8_UNORM; // Default
    if (!desc.colorAttachments.empty())
    {
        colorFormat = desc.colorAttachments[0].format;
    }

    // Create VulkanRenderPass with depth support
    auto renderPass = Core::MakeUnique<VulkanRenderPass>(
        device_, desc.width, desc.height, colorFormat, depthFormat);

    // Create depth resources if needed
    VkImageView depthImageView = VK_NULL_HANDLE;
    if (desc.hasDepthAttachment)
    {
        // Convert RhiFormat to VkFormat
        VkFormat vkDepthFormat = VK_FORMAT_D32_SFLOAT; // Default
        switch (depthFormat)
        {
        case RhiFormat::D24_UNORM_S8_UINT:
            vkDepthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
            break;
        case RhiFormat::D32_SFLOAT:
            vkDepthFormat = VK_FORMAT_D32_SFLOAT;
            break;
        case RhiFormat::D32_SFLOAT_S8_UINT:
            vkDepthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
            break;
        default:
            Log::Warn(std::format("Unsupported depth format, using default D24_UNORM_S8_UINT"));
            break;
        }

        // Create depth buffer using RAII
        auto depthBuffer = CreateDepthBuffer(desc.width, desc.height, vkDepthFormat);
        if (depthBuffer && depthBuffer->IsValid())
        {
            depthImageView = depthBuffer->GetVkImageView();
            Log::Debug("Depth buffer created successfully");
        }
        else
        {
            Log::Error("Failed to create depth buffer, proceeding without depth");
        }
    }

    // Initialize with swapchain images (for screen rendering) or custom images (for offscreen)
    if (!renderPass->Initialize(swapchainImageViews_, depthImageView))
    {
        Log::Error("Failed to initialize VulkanRenderPass with depth support");
        return nullptr;
    }

    Log::Debug(std::format("VulkanRenderPass created: {}x{} (depth: {})",
                           desc.width, desc.height, desc.hasDepthAttachment ? "enabled" : "disabled"));
    return renderPass;
}

Core::UniquePtr<VulkanDepthBuffer> VulkanDevice::CreateDepthBuffer(uint32_t width, uint32_t height, VkFormat format)
{
    auto depthBuffer = Core::MakeUnique<VulkanDepthBuffer>(device_, physicalDevice_, width, height, format);

    if (!depthBuffer->IsValid())
    {
        Log::Error(std::format("Failed to create depth buffer {}x{} with format {}",
                               width, height, static_cast<int>(format)));
        return nullptr;
    }

    return depthBuffer;
}

Core::UniquePtr<RhiPipeline> VulkanDevice::CreateGraphicsPipeline(const RhiGraphicsPipelineDesc &desc)
{
    // Load shaders from description
    std::vector<VkShaderModule> shaderModules;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    for (const auto &shaderDesc : desc.shaders)
    {
        // Load shader SPIR-V from file
        std::ifstream file(shaderDesc.filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            Log::Error(std::format("Failed to open shader file: {}", shaderDesc.filePath));
            // Clean up previously created modules
            for (auto module : shaderModules)
            {
                vkDestroyShaderModule(device_, module, nullptr);
            }
            return nullptr;
        }

        size_t fileSize = file.tellg();
        std::vector<uint32_t> shaderCode(fileSize / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char *>(shaderCode.data()), fileSize);
        file.close();

        // Create shader module
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = fileSize;
        createInfo.pCode = shaderCode.data();

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            Log::Error(std::format("Failed to create shader module for: {}", shaderDesc.filePath));
            for (auto module : shaderModules)
            {
                vkDestroyShaderModule(device_, module, nullptr);
            }
            return nullptr;
        }

        shaderModules.push_back(shaderModule);

        // Create shader stage info
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.module = shaderModule;
        stageInfo.pName = shaderDesc.entryPoint.c_str();

        // Convert RHI shader stage to Vulkan
        switch (shaderDesc.stage)
        {
        case RhiShaderStage::Vertex:
            stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case RhiShaderStage::Fragment:
            stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case RhiShaderStage::Compute:
            stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            break;
        case RhiShaderStage::Geometry:
            stageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            break;
        default:
            Log::Warn("Unsupported shader stage, using vertex as default");
            stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        }

        shaderStages.push_back(stageInfo);
    }

    // Create vertex input state
    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

    for (const auto &binding : desc.vertexInput.bindings)
    {
        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding = binding.binding;
        bindingDesc.stride = binding.stride;
        bindingDesc.inputRate = (binding.inputRate == RhiVertexInputRate::Instance)
                                    ? VK_VERTEX_INPUT_RATE_INSTANCE
                                    : VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescriptions.push_back(bindingDesc);
    }

    for (const auto &attribute : desc.vertexInput.attributes)
    {
        VkVertexInputAttributeDescription attributeDesc{};
        attributeDesc.binding = attribute.binding;
        attributeDesc.location = attribute.location;
        attributeDesc.offset = attribute.offset;

        // Convert RHI format to Vulkan format
        switch (attribute.format)
        {
        case RhiFormat::R32G32_SFLOAT:
            attributeDesc.format = VK_FORMAT_R32G32_SFLOAT;
            break;
        case RhiFormat::R32G32B32_SFLOAT:
            attributeDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
            break;
        case RhiFormat::R32G32B32A32_SFLOAT:
            attributeDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            break;
        default:
            Log::Warn("Unsupported vertex attribute format");
            attributeDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
            break;
        }

        attributeDescriptions.push_back(attributeDesc);
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Create input assembly state
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    switch (desc.inputAssembly.topology)
    {
    case RhiPrimitiveTopology::TriangleList:
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
    case RhiPrimitiveTopology::LineList:
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        break;
    default:
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
    }
    inputAssembly.primitiveRestartEnable = desc.inputAssembly.primitiveRestartEnable ? VK_TRUE : VK_FALSE;

    // Create pipeline layout with descriptor set layouts
    std::vector<VkDescriptorSetLayout> vkSetLayouts;
    for (void *layout : desc.descriptorSetLayouts)
    {
        if (layout)
        {
            auto *rhiLayout = static_cast<RhiDescriptorSetLayout *>(layout);
            VkDescriptorSetLayout vkLayout = static_cast<VkDescriptorSetLayout>(rhiLayout->GetNativeHandle());
            vkSetLayouts.push_back(vkLayout);
        }
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(vkSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = vkSetLayouts.data();

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        Log::Error("Failed to create pipeline layout");
        for (auto module : shaderModules)
        {
            vkDestroyShaderModule(device_, module, nullptr);
        }
        return nullptr;
    }

    // Viewport state (will be dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = desc.rasterization.depthClampEnable ? VK_TRUE : VK_FALSE;
    rasterizer.rasterizerDiscardEnable = desc.rasterization.rasterizerDiscardEnable ? VK_TRUE : VK_FALSE;
    rasterizer.lineWidth = desc.rasterization.lineWidth;

    // Convert polygon mode
    switch (desc.rasterization.polygonMode)
    {
    case RhiPolygonMode::Fill:
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        break;
    case RhiPolygonMode::Line:
        rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
        break;
    case RhiPolygonMode::Point:
        rasterizer.polygonMode = VK_POLYGON_MODE_POINT;
        break;
    }

    // Convert cull mode
    switch (desc.rasterization.cullMode)
    {
    case RhiCullMode::None:
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        break;
    case RhiCullMode::Front:
        rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        break;
    case RhiCullMode::Back:
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        break;
    case RhiCullMode::FrontAndBack:
        rasterizer.cullMode = VK_CULL_MODE_FRONT_AND_BACK;
        break;
    }

    rasterizer.frontFace = (desc.rasterization.frontFace == RhiFrontFace::CounterClockwise)
                               ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                               : VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = desc.rasterization.depthBiasEnable ? VK_TRUE : VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil state
    VkPipelineDepthStencilStateCreateInfo depthStencilState{};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = desc.depthStencil.depthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencilState.depthWriteEnable = desc.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencilState.stencilTestEnable = desc.depthStencil.stencilTestEnable ? VK_TRUE : VK_FALSE;
    
    // Convert depth compare op
    switch (desc.depthStencil.depthCompareOp) {
        case RhiCompareOp::Never: depthStencilState.depthCompareOp = VK_COMPARE_OP_NEVER; break;
        case RhiCompareOp::Less: depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS; break;
        case RhiCompareOp::Equal: depthStencilState.depthCompareOp = VK_COMPARE_OP_EQUAL; break;
        case RhiCompareOp::LessOrEqual: depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; break;
        case RhiCompareOp::Greater: depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER; break;
        case RhiCompareOp::NotEqual: depthStencilState.depthCompareOp = VK_COMPARE_OP_NOT_EQUAL; break;
        case RhiCompareOp::GreaterOrEqual: depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; break;
        case RhiCompareOp::Always: depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS; break;
        default: depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS; break;
    }
    
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;

    // Color blending
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    for (size_t i = 0; i < desc.colorBlend.attachments.size(); ++i)
    {
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = desc.colorBlend.attachments[i].blendEnable ? VK_TRUE : VK_FALSE;
        colorBlendAttachments.push_back(colorBlendAttachment);
    }

    // If no color attachments specified, use a default one
    if (colorBlendAttachments.empty())
    {
        VkPipelineColorBlendAttachmentState defaultAttachment{};
        defaultAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        defaultAttachment.blendEnable = VK_FALSE;
        colorBlendAttachments.push_back(defaultAttachment);
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = desc.colorBlend.logicOpEnable ? VK_TRUE : VK_FALSE;
    colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
    colorBlending.pAttachments = colorBlendAttachments.data();

    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    // Create a compatible render pass based on pipeline descriptor
    VkRenderPass compatibleRenderPass = CreateCompatibleRenderPass(desc);
    pipelineInfo.renderPass = compatibleRenderPass;
    pipelineInfo.subpass = 0;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
    {
        Log::Error("Failed to create graphics pipeline from description");
        vkDestroyPipelineLayout(device_, pipelineLayout, nullptr);
        for (auto module : shaderModules)
        {
            vkDestroyShaderModule(device_, module, nullptr);
        }
        return nullptr;
    }

    // Cleanup shader modules (no longer needed after pipeline creation)
    for (auto module : shaderModules)
    {
        vkDestroyShaderModule(device_, module, nullptr);
    }

    Log::Debug(std::format("Data-driven graphics pipeline created with {} shaders", desc.shaders.size()));
    return Core::MakeUnique<VulkanPipeline>(device_, pipeline, pipelineLayout, compatibleRenderPass);
}

Core::UniquePtr<RhiPipeline> VulkanDevice::CreateComputePipeline(const RhiComputePipelineDesc &desc)
{
    // Load compute shader
    VkShaderModule computeModule = Tool::LoadShader(desc.computeShader.filePath.c_str(), device_);
    if (computeModule == VK_NULL_HANDLE)
    {
        Log::Error(std::format("Failed to load compute shader: {}", desc.computeShader.filePath));
        return nullptr;
    }

    VkPipelineShaderStageCreateInfo computeStage{};
    computeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeStage.module = computeModule;
    computeStage.pName = desc.computeShader.entryPoint.c_str();

    // Create descriptor set layouts
    std::vector<VkDescriptorSetLayout> vkLayouts;
    vkLayouts.reserve(desc.descriptorSetLayouts.size());
    for (void *layoutPtr : desc.descriptorSetLayouts)
    {
        auto *rhiLayout = static_cast<RhiDescriptorSetLayout *>(layoutPtr);
        auto *vulkanLayout = static_cast<VulkanDescriptorSetLayout *>(rhiLayout);
        vkLayouts.push_back(vulkanLayout->GetVkDescriptorSetLayout());
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(vkLayouts.size());
    layoutInfo.pSetLayouts = vkLayouts.data();
    layoutInfo.pushConstantRangeCount = 0; // TODO: Add push constants support
    layoutInfo.pPushConstantRanges = nullptr;

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        Log::Error("Failed to create compute pipeline layout");
        vkDestroyShaderModule(device_, computeModule, nullptr);
        return nullptr;
    }

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = computeStage;
    pipelineInfo.layout = pipelineLayout;

    VkPipeline pipeline;
    if (vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
    {
        Log::Error("Failed to create compute pipeline");
        vkDestroyPipelineLayout(device_, pipelineLayout, nullptr);
        vkDestroyShaderModule(device_, computeModule, nullptr);
        return nullptr;
    }

    // Cleanup shader module
    vkDestroyShaderModule(device_, computeModule, nullptr);

    Log::Info(std::format("Compute pipeline created: {}", desc.computeShader.filePath));
    return Core::MakeUnique<VulkanPipeline>(device_, pipeline, pipelineLayout);
}

// Helper function to convert RHI texture usage to Vulkan usage
VkImageUsageFlags RhiTextureUsageToVkUsage(RhiTextureUsage usage)
{
    VkImageUsageFlags vkUsage = 0;

    if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(RhiTextureUsage::Sampled))
    {
        vkUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(RhiTextureUsage::Storage))
    {
        vkUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(RhiTextureUsage::ColorAttachment))
    {
        vkUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(RhiTextureUsage::DepthStencilAttachment))
    {
        vkUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(RhiTextureUsage::TransferSrc))
    {
        vkUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(RhiTextureUsage::TransferDst))
    {
        vkUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    return vkUsage;
}

// Helper function to find memory type
uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    return UINT32_MAX;
}

Core::UniquePtr<RhiTexture> VulkanDevice::CreateTexture(const RhiTextureDesc &desc)
{
    Log::Info(std::format("VulkanDevice: Creating texture: {}x{}x{}, format={}, usage={}", 
                          desc.width, desc.height, desc.depth, static_cast<int>(desc.format), static_cast<int>(desc.usage)));
    
    VkFormat vkFormat = RhiFormatToVkFormat(desc.format);
    if (vkFormat == VK_FORMAT_UNDEFINED)
    {
        Log::Error("Unsupported texture format");
        return nullptr;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    switch (desc.type)
    {
    case RhiTextureType::Texture2D:
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        break;
    case RhiTextureType::Texture3D:
        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        break;
    case RhiTextureType::TextureCube:
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        break;
    default:
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        break;
    }

    imageInfo.extent.width = desc.width;
    imageInfo.extent.height = desc.height;
    imageInfo.extent.depth = desc.depth;
    imageInfo.mipLevels = desc.mipLevels;
    imageInfo.arrayLayers = desc.arrayLayers;
    imageInfo.format = vkFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = RhiTextureUsageToVkUsage(desc.usage);
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image;
    if (vkCreateImage(device_, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        Log::Error("Failed to create Vulkan image");
        return nullptr;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice_, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (allocInfo.memoryTypeIndex == UINT32_MAX)
    {
        Log::Error("Failed to find suitable memory type for image");
        vkDestroyImage(device_, image, nullptr);
        return nullptr;
    }

    VkDeviceMemory imageMemory;
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        Log::Error("Failed to allocate image memory");
        vkDestroyImage(device_, image, nullptr);
        return nullptr;
    }

    vkBindImageMemory(device_, image, imageMemory, 0);

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;

    switch (desc.type)
    {
    case RhiTextureType::Texture2D:
    case RhiTextureType::Texture2DArray:
        viewInfo.viewType = desc.arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        break;
    case RhiTextureType::Texture3D:
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        break;
    case RhiTextureType::TextureCube:
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        break;
    }

    viewInfo.format = vkFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = desc.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = desc.arrayLayers;

    // Handle depth formats
    if (desc.format == RhiFormat::D24_UNORM_S8_UINT || desc.format == RhiFormat::D32_SFLOAT)
    {
        Log::Info(std::format("VulkanDevice: Setting depth aspect mask for format {}", static_cast<int>(desc.format)));
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageView imageView;
    if (vkCreateImageView(device_, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        Log::Error("Failed to create image view");
        vkDestroyImage(device_, image, nullptr);
        vkFreeMemory(device_, imageMemory, nullptr);
        return nullptr;
    }

    Log::Info(std::format("Texture created: {}x{}x{}, mips={}, layers={}",
                          desc.width, desc.height, desc.depth, desc.mipLevels, desc.arrayLayers));

    return Core::MakeUnique<VulkanTexture>(device_, image, imageMemory, imageView, desc);
}

Core::UniquePtr<RhiTexture> VulkanDevice::CreateTextureFromFile(const std::string &filePath, const RhiSamplerDesc &samplerDesc)
{
    // TODO: Implement file loading using existing texture loading code
    Log::Warn("CreateTextureFromFile not yet implemented - requires integration with existing texture loading");
    return nullptr;
}

Core::UniquePtr<RhiSampler> VulkanDevice::CreateSampler(const RhiSamplerDesc &desc)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = (desc.magFilter == RhiFilter::Linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.minFilter = (desc.minFilter == RhiFilter::Linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

    auto convertAddressMode = [](RhiSamplerAddressMode mode) -> VkSamplerAddressMode
    {
        switch (mode)
        {
        case RhiSamplerAddressMode::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case RhiSamplerAddressMode::MirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case RhiSamplerAddressMode::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case RhiSamplerAddressMode::ClampToBorder:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    };

    samplerInfo.addressModeU = convertAddressMode(desc.addressModeU);
    samplerInfo.addressModeV = convertAddressMode(desc.addressModeV);
    samplerInfo.addressModeW = convertAddressMode(desc.addressModeW);
    samplerInfo.anisotropyEnable = desc.enableAnisotropy ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = desc.maxAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSampler sampler;
    if (vkCreateSampler(device_, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
    {
        Log::Error("Failed to create sampler");
        return nullptr;
    }

    Log::Debug("Sampler created");
    return Core::MakeUnique<VulkanSampler>(device_, sampler);
}

bool VulkanDevice::CreateStagingBuffer()
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = STAGING_BUFFER_SIZE;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &stagingBuffer_.buffer) != VK_SUCCESS)
    {
        Log::Error("Failed to create staging buffer");
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, stagingBuffer_.buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice_, memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (allocInfo.memoryTypeIndex == UINT32_MAX)
    {
        Log::Error("Failed to find suitable memory type for staging buffer");
        vkDestroyBuffer(device_, stagingBuffer_.buffer, nullptr);
        stagingBuffer_.buffer = VK_NULL_HANDLE;
        return false;
    }

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &stagingBuffer_.memory) != VK_SUCCESS)
    {
        Log::Error("Failed to allocate staging buffer memory");
        vkDestroyBuffer(device_, stagingBuffer_.buffer, nullptr);
        stagingBuffer_.buffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(device_, stagingBuffer_.buffer, stagingBuffer_.memory, 0);

    // Map the staging buffer memory
    if (vkMapMemory(device_, stagingBuffer_.memory, 0, STAGING_BUFFER_SIZE, 0, &stagingBuffer_.mappedData) != VK_SUCCESS)
    {
        Log::Error("Failed to map staging buffer memory");
        vkFreeMemory(device_, stagingBuffer_.memory, nullptr);
        vkDestroyBuffer(device_, stagingBuffer_.buffer, nullptr);
        stagingBuffer_.buffer = VK_NULL_HANDLE;
        stagingBuffer_.memory = VK_NULL_HANDLE;
        return false;
    }

    stagingBuffer_.size = STAGING_BUFFER_SIZE;
    stagingBuffer_.offset = 0;

    Log::Info(std::format("Staging buffer created: {} MB", STAGING_BUFFER_SIZE / (1024 * 1024)));
    return true;
}

void VulkanDevice::DestroyStagingBuffer()
{
    if (stagingBuffer_.mappedData)
    {
        vkUnmapMemory(device_, stagingBuffer_.memory);
        stagingBuffer_.mappedData = nullptr;
    }

    if (stagingBuffer_.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device_, stagingBuffer_.memory, nullptr);
        stagingBuffer_.memory = VK_NULL_HANDLE;
    }

    if (stagingBuffer_.buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device_, stagingBuffer_.buffer, nullptr);
        stagingBuffer_.buffer = VK_NULL_HANDLE;
    }

    stagingBuffer_.size = 0;
    stagingBuffer_.offset = 0;

    Log::Debug("Staging buffer destroyed");
}

RhiResult VulkanDevice::AllocateFromStagingBuffer(uint64_t size, void **mappedData, uint64_t *offset)
{
    if (!stagingBuffer_.mappedData)
    {
        Log::Error("Staging buffer not initialized");
        return RhiResult::ErrorInvalidParameter;
    }

    // Align to 4 bytes
    size = (size + 3) & ~3;

    if (stagingBuffer_.offset + size > stagingBuffer_.size)
    {
        // Reset staging buffer (simple linear allocator)
        stagingBuffer_.offset = 0;
        Log::Warn("Staging buffer reset - consider using larger buffer for better performance");
    }

    if (stagingBuffer_.offset + size > stagingBuffer_.size)
    {
        Log::Error(std::format("Requested size {} too large for staging buffer", size));
        return RhiResult::ErrorInvalidParameter;
    }

    *mappedData = static_cast<uint8_t *>(stagingBuffer_.mappedData) + stagingBuffer_.offset;
    *offset = stagingBuffer_.offset;
    stagingBuffer_.offset += size;

    return RhiResult::Success;
}

RhiResult VulkanDevice::UploadBufferData(RhiBuffer *buffer, const void *data, uint64_t size, uint64_t offset)
{
    if (!buffer || !data || size == 0)
    {
        Log::Warn("Invalid parameters for buffer upload");
        return RhiResult::ErrorInvalidParameter;
    }

    void *stagingData;
    uint64_t stagingOffset;

    if (AllocateFromStagingBuffer(size, &stagingData, &stagingOffset) != RhiResult::Success)
    {
        Log::Error("Failed to allocate staging buffer space");
        return RhiResult::ErrorInvalidParameter;
    }

    // Copy data to staging buffer
    std::memcpy(stagingData, data, size);

    // Create temporary command buffer for copy
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = stagingOffset;
    copyRegion.dstOffset = offset;
    copyRegion.size = size;

    auto *vulkanBuffer = static_cast<VulkanBuffer *>(buffer);
    vkCmdCopyBuffer(commandBuffer, stagingBuffer_.buffer, vulkanBuffer->GetVkBuffer(), 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue_);

    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);

    Log::Debug(std::format("Buffer data uploaded: {} bytes at offset {}", size, offset));
    return RhiResult::Success;
}

RhiResult VulkanDevice::UploadTextureData(RhiTexture *texture, const RhiTextureUploadDesc &uploadDesc)
{
    if (!texture || !uploadDesc.data || uploadDesc.dataSize == 0)
    {
        Log::Warn("Invalid parameters for texture upload");
        return RhiResult::ErrorInvalidParameter;
    }

    void *stagingData;
    uint64_t stagingOffset;

    if (AllocateFromStagingBuffer(uploadDesc.dataSize, &stagingData, &stagingOffset) != RhiResult::Success)
    {
        Log::Error("Failed to allocate staging buffer space for texture upload");
        return RhiResult::ErrorInvalidParameter;
    }

    // Copy data to staging buffer
    std::memcpy(stagingData, uploadDesc.data, uploadDesc.dataSize);

    // Create temporary command buffer for copy
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    auto *vulkanTexture = static_cast<VulkanTexture *>(texture);

    // Transition image to transfer destination layout
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vulkanTexture->GetVkImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = uploadDesc.mipLevel;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = uploadDesc.arrayLayer;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = stagingOffset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = uploadDesc.mipLevel;
    region.imageSubresource.baseArrayLayer = uploadDesc.arrayLayer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};

    uint32_t mipWidth = std::max(1u, texture->GetWidth() >> uploadDesc.mipLevel);
    uint32_t mipHeight = std::max(1u, texture->GetHeight() >> uploadDesc.mipLevel);
    uint32_t mipDepth = std::max(1u, texture->GetDepth() >> uploadDesc.mipLevel);

    region.imageExtent = {mipWidth, mipHeight, mipDepth};

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer_.buffer, vulkanTexture->GetVkImage(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to shader read layout
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue_);

    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);

    Log::Debug(std::format("Texture data uploaded: {} bytes to mip {} layer {}",
                           uploadDesc.dataSize, uploadDesc.mipLevel, uploadDesc.arrayLayer));
    return RhiResult::Success;
}
