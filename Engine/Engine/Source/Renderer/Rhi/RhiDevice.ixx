module;
#include <cstdint>
export module Engine.Rhi.Device;
import Engine.Rhi.Definition;
import Engine.Rhi.Shader;      // 含 ShaderStage, RhiShader
import Engine.Rhi.Pipeline;    // 含 PipelineStateDesc, RhiPipeline
import Engine.Rhi.CommandList; // 含 RhiCommandList
import Engine.Rhi.Swapchain;   // 含 RhiSwapchain
import Engine.Rhi.Texture;
import Engine.Rhi.Sync;
import Engine.Rhi.Buffer;
import Engine.Rhi.Descriptor;
import Engine.Rhi.Sampler;
import std;
export namespace Engine::Rhi
{
    struct DeviceDesc
    {
        bool DebugLayer = true;  // 是否开启图形 API 的调试验证层
        bool RayTracing = false; // 是否开启光追扩展
    };

    struct QueueSubmitInfo
    {
        CommandList* CmdList;
        RhiSemaphore* WaitSemaphore = nullptr;
        WaitStage WaitStageMask{WaitStage::AllCommands};
        RhiSemaphore* SignalSemaphore = nullptr;
        RhiFence* SignalFence = nullptr;
    };

    class RhiDevice
    {
    public:
        virtual bool Init(const DeviceDesc& desc) = 0;

        virtual std::unique_ptr<RhiSwapchain> CreateSwapchain(void* windowHandle, uint32_t width, uint32_t height,
                                                              Engine::Rhi::PixelFormat format) = 0;

        virtual std::shared_ptr<CommandList> CreateCommandList() = 0;

        virtual std::unique_ptr<Pipeline> CreatePipeline(const PipelineStateDesc& desc) = 0;

        virtual std::unique_ptr<RhiShader> CreateShader(const std::string_view filePath, ShaderStage stage) = 0;

        virtual std::unique_ptr<RhiTexture> CreateTexture(const TextureDesc& desc) = 0;

        virtual std::unique_ptr<RhiSemaphore> CreateSyncSemaphore() = 0;

        virtual std::unique_ptr<RhiFence> CreateSyncFence(bool signaled = false) = 0;

        virtual std::unique_ptr<RhiBuffer> CreateBuffer(const BufferDesc& desc) = 0;

        virtual std::unique_ptr<Sampler> CreateSampler(const SamplerDesc& desc) = 0;

        virtual std::unique_ptr<DescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) = 0;

        virtual std::unique_ptr<RhiDescriptorSet> CreateDescriptorSet(const DescriptorSetLayout* layout, bool isTransit = false) = 0;

        virtual void CopyBufferImmediate(RhiBuffer* src, RhiBuffer* dst, uint64_t size, uint64_t srcOffset = 0, uint64_t dstOffset = 0) = 0;

        /// @brief 创建临时 CmdList -> 录制 -> 提交 -> 等待完成。 立即上传数据
        /// @param texture
        /// @param data
        /// @param size
        virtual void UploadTextureData(const RhiTexture* texture, const void* data, uint64_t size) = 0;

        /// @brief 创建临时 CmdList -> 录制 -> 提交 -> 等待完成。 立即上传数据到 Buffer
        virtual void UploadBufferData(RhiBuffer* buffer, const void* data, uint64_t size) = 0;

        virtual void Execute(const QueueSubmitInfo& info) = 0;
        virtual void WaitIdle() = 0;
        virtual ~RhiDevice() = default;

        static constexpr uint32_t GetMaxFramesInFlight()
        {
            return kMaxFramesInFlight_;
        }
        virtual void BeginFrame(uint32_t frameIndex) = 0;

    protected:
        uint32_t currentFrameIndex_{0};
        virtual void CreateTransientPools() = 0;
        static const int kMaxFramesInFlight_ = 3;
    };

    std::unique_ptr<RhiDevice> CreateDevice(GraphicsBackend backend);

} // namespace Engine::Rhi