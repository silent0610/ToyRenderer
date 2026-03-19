module;
#include "vulkan/vulkan.h"
export module Engine.Rhi.Vulkan.Device;
import std;
import Engine.Rhi.Device;
import Engine.Rhi.Shader;
import Engine.Rhi.Pipeline;
import Engine.Rhi.CommandList;
import Engine.Rhi.Swapchain;
import Engine.Rhi.Texture;
import Engine.Rhi.Sync;
import Engine.Rhi.Buffer;
import Engine.Rhi.Descriptor;
import Engine.Rhi.Sampler;
export namespace Engine::Rhi
{
    class VulkanDevice : public RhiDevice
    {
    public:
        VulkanDevice() = default;
        virtual ~VulkanDevice();
        void Execute(const QueueSubmitInfo& info) override;
        bool Init(const DeviceDesc& desc) override;

        std::unique_ptr<RhiSwapchain> CreateSwapchain(void* windowHandle, uint32_t width, uint32_t height, PixelFormat format) override;

        std::shared_ptr<CommandList> CreateCommandList() override;

        std::unique_ptr<Pipeline> CreatePipeline(const PipelineStateDesc& desc) override;

        std::unique_ptr<RhiShader> CreateShader(const std::string_view filePath, ShaderStage stage) override;

        std::unique_ptr<RhiTexture> CreateTexture(const TextureDesc& desc);
        std::unique_ptr<RhiSemaphore> CreateSyncSemaphore() override;
        std::unique_ptr<RhiFence> CreateSyncFence(bool signaled) override;

        std::unique_ptr<RhiBuffer> CreateBuffer(const BufferDesc& desc);

        std::unique_ptr<Sampler> CreateSampler(const SamplerDesc& desc) override;

        std::unique_ptr<DescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) override;

        std::unique_ptr<RhiDescriptorSet> CreateDescriptorSet(const DescriptorSetLayout* layout, bool isTransit = false) override;

        void UploadTextureData(const RhiTexture* texture, const void* data, uint64_t size) override;
        void UploadBufferData(RhiBuffer* buffer, const void* data, uint64_t size) override;

        void CopyBufferImmediate(RhiBuffer* src, RhiBuffer* dst, uint64_t size, uint64_t srcOffset = 0, uint64_t dstOffset = 0);

        void WaitIdle() override;

        VkInstance GetInstance() const;
        VkPhysicalDevice GetPhysicalDevice() const;
        VkDevice GetDevice() const;

        VkQueue GetGraphicsQueue() const;
        uint32_t GetGraphicsQueueFamilyIndex() const;

        uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    protected:
        void CreateTransientPools() override;
        void BeginFrame(uint32_t frameIndex) override;

    private:
        bool CreateInstance(bool enableValidation);
        bool SetupDebugMessenger();
        bool PickPhysicalDevice();
        bool CreateLogicalDevice();
        std::optional<uint32_t> FindQueueFamilies(VkPhysicalDevice device);
        bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
        void CreateDescriptorPool();

    private:
        VkInstance instance_{};
        VkDebugUtilsMessengerEXT debugMessenger_{};
        VkPhysicalDevice physicalDevice_{};
        VkDevice device_{};
        bool enableValidationLayer_{};
        VkQueue graphicsQueue_{};
        uint32_t graphicsQueueFamilyIndex_;
        VkDescriptorPool globalDescriptorPool_{};
        bool enableValidation_;
        std::vector<const char*> validationLayers_{"VK_LAYER_KHRONOS_validation"};
        const std::vector<const char*> kDeviceExtensions_{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDescriptorPool transientPools_[RhiDevice::GetMaxFramesInFlight()]{};
    };
} // namespace Engine::Rhi