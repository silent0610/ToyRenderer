module;
#include <cstdint>
#include "vulkan/vulkan.h"
export module Engine.Rhi.Vulkan.CommandList;

import Engine.Rhi.CommandList;   // 基类接口
import Engine.Rhi.Vulkan.Device; // 需要访问 VkDevice
import Engine.Rhi.Swapchain;     // BeginRendering 需要用到 Swapchain
import Engine.Rhi.Definition;    // 枚举
import Engine.Rhi.Vulkan.Tool;
import Engine.Rhi.Buffer;
import Engine.Rhi.Descriptor;
import std;

export namespace Engine::Rhi
{

    class VulkanCommandList final : public CommandList
    {
    public:
        VulkanCommandList(VulkanDevice *device);
        ~VulkanCommandList() override;

        void Begin() override;
        void DebugBegin() override;
        void End() override;
        void DebugEnd() override;
        void BeginRendering(const RenderPassInfo &passInfo) override;
        void EndRendering() override;

        void SetViewport(float x, float y, float width, float height) override;
        void SetScissor(int x, int y, uint32_t width, uint32_t height) override;
        void Draw(uint32_t vertexCount, uint32_t firstVertex) override;
        void SetPipelineState(const Pipeline *pipeline) override;
        void BlitTexture(RhiTexture *src, RhiTexture *dst, FilterMode filter = FilterMode::Linear) override;
        void SetVertexBuffer(const RhiBuffer *buffer) override;
        void SetDescriptorSet(const Pipeline* pipeline, uint32_t setIndex, const RhiDescriptorSet* set)override;
        void SetResourceBarrier(const TextureBarrier &barrier) override;
        void SetResourceBarrier(const BufferBarrier& barrier) override;
        void SetPushConstants(Pipeline *pipeline, ShaderStage stage, const void *data, uint32_t size) override;
        VkCommandBuffer GetCommandBuffer() const { return commandBuffer_; }
        void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
        void CopyBuffer(RhiBuffer *src, RhiBuffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset);

        void SetIndexBuffer(RhiBuffer* buffer, uint64_t offset = 0) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                         uint32_t firstInstance = 0) override;

        void CopyBufferToTexture(const RhiBuffer* source, const RhiTexture* dest, const BufferTextureCopyRegion& region)override;
    private:
        VulkanDevice *device_;

        VkCommandPool commandPool_{};
        VkCommandBuffer commandBuffer_{};

        // 记录当前是否处于 RenderPass 中
        bool isRecording_{false};
        VkImage activeImage_{};
        std::vector<VkImage> activeColorImages_;
        // Resource Barrier
    };
}