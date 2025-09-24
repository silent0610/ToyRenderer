module;
#include "vulkan/vulkan.h"
#include <cstdint>

export module VulkanCommandBuffer;
import RhiCommandBuffer;
import RhiTypes;
import Logger;

// Vulkan implementation of RhiCommandBuffer
export class VulkanCommandBuffer : public RhiCommandBuffer {
private:
    VkCommandBuffer commandBuffer_;
    VkDevice device_;
    bool isRecording_;
    
    // Track bound pipeline layout for descriptor set binding
    VkPipelineLayout currentPipelineLayout_;
    
public:
    VulkanCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer);
    ~VulkanCommandBuffer() override = default;
    
    // Command recording
    RhiResult Begin() override;
    RhiResult End() override;
    RhiResult Reset() override;
    
    // Render pass commands
    void BeginRenderPass(RhiRenderPass* renderPass) override;
    void EndRenderPass() override;
    
    // Pipeline and resource binding
    void BindPipeline(RhiPipeline* pipeline) override;
    void BindVertexBuffer(RhiBuffer* buffer, uint32_t binding = 0) override;
    void BindIndexBuffer(RhiBuffer* buffer) override;
    void BindDescriptorSet(RhiDescriptorSet* descriptorSet, uint32_t setIndex = 0) override;
    
    // Drawing commands
    void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, 
              uint32_t firstVertex = 0, uint32_t firstInstance = 0) override;
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                     uint32_t firstIndex = 0, int32_t vertexOffset = 0, 
                     uint32_t firstInstance = 0) override;
    
    // Compute commands
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) override;
    
    // Resource copy commands
    void CopyBuffer(RhiBuffer* srcBuffer, RhiBuffer* dstBuffer, 
                   uint64_t srcOffset = 0, uint64_t dstOffset = 0, uint64_t size = 0) override;
    void CopyImage(RhiTexture* srcTexture, RhiTexture* dstTexture,
                  uint32_t srcMipLevel = 0, uint32_t dstMipLevel = 0,
                  uint32_t srcArrayLayer = 0, uint32_t dstArrayLayer = 0) override;
    void CopyBufferToImage(RhiBuffer* srcBuffer, RhiTexture* dstTexture,
                          uint32_t bufferOffset = 0, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) override;
    
    // Synchronization commands
    void PipelineBarrier(const RhiBarrierDesc& barrierDesc) override;
    
    // State commands
    void SetViewport(const RhiViewport& viewport) override;
    void SetScissor(const RhiRect2D& scissor) override;
    
    // Vulkan-specific getters
    VkCommandBuffer GetVkCommandBuffer() const { return commandBuffer_; }
};