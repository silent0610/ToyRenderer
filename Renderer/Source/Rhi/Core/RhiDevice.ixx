module;
#include <cstdint>

export module RhiDevice;
import Core;
import RhiTypes;
import RhiBuffer;
import RhiCommandBuffer;
import RhiPipeline;
import RhiRenderPass;
import RhiDescriptor;
import RhiRenderPassDesc;
import RhiPipelineDesc;
import RhiTexture;

// Forward declarations for descriptor types
export class RhiDescriptorSetLayout;
export class RhiDescriptorPool;
export class RhiDescriptorSet;
export struct RhiDescriptorSetLayoutDesc;
export struct RhiDescriptorPoolDesc;

// Forward declarations for pipeline types
export struct RhiGraphicsPipelineDesc;
export struct RhiComputePipelineDesc;

// Core device interface - focused on essential rendering functionality
export class RhiDevice
{
public:
    virtual ~RhiDevice() = default;

    // Resource creation
    virtual Core::UniquePtr<RhiBuffer> CreateBuffer(const RhiBufferDesc& desc) = 0;
    virtual Core::UniquePtr<RhiCommandBuffer> CreateCommandBuffer() = 0;
    virtual Core::UniquePtr<RhiPipeline> CreateGraphicsPipeline() = 0; // Legacy hardcoded version
    virtual Core::UniquePtr<RhiPipeline> CreateGraphicsPipeline(const RhiGraphicsPipelineDesc& desc) = 0; // Data-driven version
    virtual Core::UniquePtr<RhiPipeline> CreateComputePipeline(const RhiComputePipelineDesc& desc) = 0;
    virtual Core::UniquePtr<RhiTexture> CreateTexture(const RhiTextureDesc& desc) = 0;
    virtual Core::UniquePtr<RhiTexture> CreateTextureFromFile(const std::string& filePath, const RhiSamplerDesc& samplerDesc = {}) = 0;
    virtual Core::UniquePtr<RhiSampler> CreateSampler(const RhiSamplerDesc& desc) = 0;
    virtual RhiResult UploadTextureData(RhiTexture* texture, const RhiTextureUploadDesc& uploadDesc) = 0;
    virtual RhiResult UploadBufferData(RhiBuffer* buffer, const void* data, uint64_t size, uint64_t offset = 0) = 0;
    virtual Core::UniquePtr<RhiRenderPass> CreateRenderPass(uint32_t width, uint32_t height, RhiFormat colorFormat) = 0; // Legacy
    virtual Core::UniquePtr<RhiRenderPass> CreateRenderPass(const RhiRenderPassDesc& desc) = 0; // New data-driven version
    
    // Descriptor resource creation
    virtual Core::UniquePtr<RhiDescriptorSetLayout> CreateDescriptorSetLayout(const RhiDescriptorSetLayoutDesc& desc) = 0;
    virtual Core::UniquePtr<RhiDescriptorPool> CreateDescriptorPool(const RhiDescriptorPoolDesc& desc) = 0;
    virtual Core::UniquePtr<RhiDescriptorSet> AllocateDescriptorSet(RhiDescriptorPool* pool, RhiDescriptorSetLayout* layout) = 0;
    virtual void UpdateDescriptorSet(RhiDescriptorSet* descriptorSet, uint32_t binding, RhiBuffer* buffer) = 0;

    // Command submission and synchronization
    virtual RhiResult Submit(RhiCommandBuffer* commandBuffer) = 0;
    virtual RhiResult AcquireNextImage() = 0;
    virtual RhiResult Present() = 0;
    virtual RhiResult WaitIdle() = 0;

    // Basic queries
    virtual RhiFormat GetSwapchainFormat() const = 0;
    virtual void GetSwapchainExtent(uint32_t& width, uint32_t& height) const = 0;
};