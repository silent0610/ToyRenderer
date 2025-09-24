module;
#include <cstdint>

export module RhiCommandBuffer;
import RhiTypes;
import RhiBuffer;
import RhiRenderPass;
import RhiDescriptor;
import RhiBarrier;
import RhiTexture;

// Forward declarations
export class RhiPipeline;
export class RhiDescriptorSet;

// Command buffer for recording rendering commands
export class RhiCommandBuffer
{
public:
    virtual ~RhiCommandBuffer() = default;

    // Command recording
    virtual RhiResult Begin() = 0;
    virtual RhiResult End() = 0;
    virtual RhiResult Reset() = 0;

    // Render pass commands
    virtual void BeginRenderPass(RhiRenderPass* renderPass) = 0;
    virtual void EndRenderPass() = 0;

    // Pipeline and resource binding
    virtual void BindPipeline(RhiPipeline* pipeline) = 0;
    virtual void BindVertexBuffer(RhiBuffer* buffer, uint32_t binding = 0) = 0;
    virtual void BindIndexBuffer(RhiBuffer* buffer) = 0;
    virtual void BindDescriptorSet(RhiDescriptorSet* descriptorSet, uint32_t setIndex = 0) = 0;

    // Drawing commands
    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, 
                     uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                           uint32_t firstIndex = 0, int32_t vertexOffset = 0, 
                           uint32_t firstInstance = 0) = 0;
    
    // Compute commands
    virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) = 0;
    
    // Resource copy commands
    virtual void CopyBuffer(RhiBuffer* srcBuffer, RhiBuffer* dstBuffer, 
                           uint64_t srcOffset = 0, uint64_t dstOffset = 0, uint64_t size = 0) = 0;
    virtual void CopyImage(RhiTexture* srcTexture, RhiTexture* dstTexture,
                          uint32_t srcMipLevel = 0, uint32_t dstMipLevel = 0,
                          uint32_t srcArrayLayer = 0, uint32_t dstArrayLayer = 0) = 0;
    virtual void CopyBufferToImage(RhiBuffer* srcBuffer, RhiTexture* dstTexture,
                                  uint32_t bufferOffset = 0, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) = 0;
    
    // Synchronization commands
    virtual void PipelineBarrier(const RhiBarrierDesc& barrierDesc) = 0;

    // State commands
    virtual void SetViewport(const RhiViewport& viewport) = 0;
    virtual void SetScissor(const RhiRect2D& scissor) = 0;
};