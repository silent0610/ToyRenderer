module;
#include "vulkan/vulkan.h"
#include "GLFW/glfw3.h"

export module VulkanDevice;
import Core;
import RhiDevice;
import RhiTypes;
import RhiDescriptor;
import RhiRenderPassDesc;
import RhiPipelineDesc;
import RhiTexture;
import VulkanUtils;
import VulkanDepthBuffer;

// Clean VulkanDevice implementation - no dependencies on old code
export class VulkanDevice : public RhiDevice
{
private:
    // Core Vulkan objects
    VkInstance instance_;
    VkPhysicalDevice physicalDevice_;
    VkDevice device_;
    VkSurfaceKHR surface_;

    // Queues
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;
    VulkanUtils::QueueFamilyIndices queueIndices_;

    // Command pool
    VkCommandPool commandPool_;

    // Swapchain
    VkSwapchainKHR swapchain_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkFormat swapchainFormat_;
    VkExtent2D swapchainExtent_;

    // Synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    uint32_t currentFrame_;
    uint32_t currentImageIndex_; // Current swapchain image index
    static const int MAX_FRAMES_IN_FLIGHT = 2;

    // Default render pass for pipeline creation
    VkRenderPass defaultRenderPass_;

    // Window reference
    GLFWwindow *window_;

    // Staging buffer management
    struct StagingBuffer
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void *mappedData = nullptr;
        uint64_t size = 0;
        uint64_t offset = 0;
    };
    StagingBuffer stagingBuffer_;
    static const uint64_t STAGING_BUFFER_SIZE = 64 * 1024 * 1024; // 64MB

    // Setup methods
    bool CreateSwapchain();
    bool CreateImageViews();
    bool CreateCommandPool();
    bool CreateSyncObjects();
    bool CreateDefaultRenderPass();
    VkRenderPass CreateCompatibleRenderPass(const RhiGraphicsPipelineDesc& desc);
    void CleanupSwapchain();

    // Staging buffer methods
    bool CreateStagingBuffer();
    void DestroyStagingBuffer();
    RhiResult AllocateFromStagingBuffer(uint64_t size, void **mappedData, uint64_t *offset);

    // Depth buffer creation using RAII
    Core::UniquePtr<VulkanDepthBuffer> CreateDepthBuffer(uint32_t width, uint32_t height, VkFormat format);

public:
    VulkanDevice(const RhiDeviceDesc &desc);
    ~VulkanDevice() override;

    // RHI Device interface implementation
    Core::UniquePtr<RhiBuffer> CreateBuffer(const RhiBufferDesc &desc) override;
    Core::UniquePtr<RhiCommandBuffer> CreateCommandBuffer() override;
    Core::UniquePtr<RhiPipeline> CreateGraphicsPipeline() override;                                    // Legacy
    Core::UniquePtr<RhiPipeline> CreateGraphicsPipeline(const RhiGraphicsPipelineDesc &desc) override; // Data-driven
    Core::UniquePtr<RhiPipeline> CreateComputePipeline(const RhiComputePipelineDesc &desc) override;
    Core::UniquePtr<RhiTexture> CreateTexture(const RhiTextureDesc &desc) override;
    Core::UniquePtr<RhiTexture> CreateTextureFromFile(const std::string &filePath, const RhiSamplerDesc &samplerDesc = {}) override;
    Core::UniquePtr<RhiSampler> CreateSampler(const RhiSamplerDesc &desc) override;
    RhiResult UploadTextureData(RhiTexture *texture, const RhiTextureUploadDesc &uploadDesc) override;
    RhiResult UploadBufferData(RhiBuffer *buffer, const void *data, uint64_t size, uint64_t offset = 0) override;
    Core::UniquePtr<RhiRenderPass> CreateRenderPass(uint32_t width, uint32_t height, RhiFormat colorFormat) override; // Legacy
    Core::UniquePtr<RhiRenderPass> CreateRenderPass(const RhiRenderPassDesc &desc) override;                          // New data-driven version

    // Descriptor resource creation
    Core::UniquePtr<RhiDescriptorSetLayout> CreateDescriptorSetLayout(const RhiDescriptorSetLayoutDesc &desc) override;
    Core::UniquePtr<RhiDescriptorPool> CreateDescriptorPool(const RhiDescriptorPoolDesc &desc) override;
    Core::UniquePtr<RhiDescriptorSet> AllocateDescriptorSet(RhiDescriptorPool *pool, RhiDescriptorSetLayout *layout) override;
    void UpdateDescriptorSet(RhiDescriptorSet *descriptorSet, uint32_t binding, RhiBuffer *buffer) override;

    RhiResult Submit(RhiCommandBuffer *commandBuffer) override;
    RhiResult AcquireNextImage() override;
    RhiResult Present() override;
    RhiResult WaitIdle() override;

    RhiFormat GetSwapchainFormat() const override;
    void GetSwapchainExtent(uint32_t &width, uint32_t &height) const override;
    
    // Vulkan-specific methods
    uint32_t GetCurrentSwapchainImageIndex() const { return currentImageIndex_; }

    // Vulkan-specific getters for implementation
    VkDevice GetVkDevice() const { return device_; }
    void GetSwapchainImageViews(std::vector<VkImageView>& imageViews) const { imageViews = swapchainImageViews_; }
    VkPhysicalDevice GetVkPhysicalDevice() const { return physicalDevice_; }
    VkCommandPool GetCommandPool() const { return commandPool_; }
    VkQueue GetGraphicsQueue() const { return graphicsQueue_; }
    // Helper function to convert RHI format to Vulkan format
};