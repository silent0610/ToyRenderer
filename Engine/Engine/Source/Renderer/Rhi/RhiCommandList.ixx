module;
#include <cstdint>
export module Engine.Rhi.CommandList;

export import Engine.Rhi.Definition;
export import Engine.Rhi.Shader;    // 含 ShaderStage, RhiShader
export import Engine.Rhi.Pipeline;  // 含 PipelineStateDesc, RhiPipeline
export import Engine.Rhi.Texture;
export import Engine.Rhi.Buffer;
import Engine.Rhi.Descriptor;
import std;
export namespace Engine::Rhi
{

    struct RenderAttachment
    {
        RhiTexture* Texture{};
        LoadOp Load{LoadOp::Clear};
        StoreOp Store { StoreOp::Store};
        ClearColor ClearValue{};
    };
    struct RenderPassInfo
    {
        std::vector<RenderAttachment> ColorTargets;
        RenderAttachment DepthStencilTarget;

        bool EnableDepth = false;
        bool EnableStencil = false;
        float ClearDepth = 1.0f;   // 默认最远深度
        uint32_t ClearStencil = 0; // 默认模板值
    };

    struct TextureBarrier
    {
        RhiTexture *Texture;
        Engine::Rhi::ResourceState Before;
        Engine::Rhi::ResourceState After;
    };
    struct BufferBarrier
    {
        RhiBuffer* Buffer;
        Engine::Rhi::ResourceState Before;
        Engine::Rhi::ResourceState After;
    };
    class CommandList
    {
    public:
        virtual void Begin() = 0;
        virtual void End() = 0;
        virtual void DebugBegin() = 0;
        virtual void DebugEnd() = 0;

        virtual void BeginRendering(const RenderPassInfo &passInfo) = 0;
        virtual void EndRendering() = 0;

        virtual void SetViewport(float x, float y, float width, float height) = 0;
        virtual void SetScissor(int x, int y, uint32_t width, uint32_t height) = 0;
        virtual void SetPipelineState(const Pipeline *pipeline) = 0;

        virtual void BlitTexture(RhiTexture *src, RhiTexture *dst, FilterMode filter = FilterMode::Linear) = 0;

        virtual void SetVertexBuffer(const RhiBuffer *buffer) = 0;

        virtual void Draw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;
        virtual void SetResourceBarrier(const TextureBarrier &barrier) = 0;
        virtual void SetResourceBarrier(const BufferBarrier& barrier) = 0;
        virtual void SetDescriptorSet(const Pipeline* pipeline, uint32_t setIndex, const RhiDescriptorSet* set) = 0;
        virtual void SetPushConstants(Pipeline *pipeline, ShaderStage stage, const void *data, uint32_t size) = 0;

        virtual void CopyBuffer(RhiBuffer *src, RhiBuffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset) = 0;
        virtual void SetIndexBuffer(RhiBuffer* buffer, uint64_t offset = 0) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                                 uint32_t firstInstance = 0) = 0;
        
        /// @brief 从 Buffer 拷贝到 Texture
        /// @param source 
        /// @param dest 
        /// @param region 
        virtual void CopyBufferToTexture(const RhiBuffer* source, const RhiTexture* dest, const BufferTextureCopyRegion& region) = 0;
        virtual ~CommandList() = default;

        
    };
}