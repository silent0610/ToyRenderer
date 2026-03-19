module;
#include <cstdint>
export module Engine.RenderGraph;

import std;

import Engine.Rhi.Device;
import Engine.Rhi.CommandList;
import Engine.Rhi.Texture;
export namespace Engine
{
    enum class RGResourceType
    {
        Texture,
        Buffer
    };
    class RGResourceHandle
    {
    public:
        uint32_t Index = 0; // 指向textureNodes_中具体资源
        uint32_t Version = 0; // 用于处理同一资源的读写版本控制
        RGResourceType Type = RGResourceType::Texture;

        bool IsValid() const { return Index != 0; }
        bool operator==(const RGResourceHandle &other) const = default;
    };

    class RGTextureNode
    {
    public:
        std::string Name;
        Rhi::TextureDesc Desc;
        Rhi::RhiTexture *ImportedTexture = nullptr;
        Rhi::RhiTexture *InternalTexture = nullptr;
        Rhi::RhiTexture *GetRhiTexture()
        {
            return ImportedTexture ? ImportedTexture : InternalTexture;
        }
        Rhi::ResourceState CurrentState = Rhi::ResourceState::Undefined;
    };

    class RGBufferNode
    {
    public:
        std::string Name;
        Rhi::BufferDesc Desc;
        Rhi::RhiBuffer *ImportedBuffer = nullptr;
        Rhi::RhiBuffer *InternalBuffer = nullptr;
        Rhi::RhiBuffer *GetRhiBuffer()
        {
            return ImportedBuffer ? ImportedBuffer : InternalBuffer;
        }
        Rhi::ResourceState CurrentState = Rhi::ResourceState::Undefined;
    };

    class RenderGraph;
    
    class RGContext
    {
    public:
        RGContext(RenderGraph *graph, const struct RGPassNode &pass, Rhi::CommandList &cmd);

        // 获取真实的 RHI 资源
        Rhi::RhiTexture *GetTexture(RGResourceHandle handle);
        Rhi::RhiBuffer *GetBuffer(RGResourceHandle handle);
        Rhi::CommandList *Cmd;

    private:
        RenderGraph *graph_;
    };

    struct RGPassNode
    {
        std::string Name;

        // 依赖关系 (用于自动屏障)
        std::vector<RGResourceHandle> Reads;
        std::vector<RGResourceHandle> Writes;

        std::vector<RGResourceHandle> TextureList;      // SRV 读取 (Sampled Texture)
        std::vector<RGResourceHandle> BufferList;       // Buffer 读或写 (Uniform/Storage/Vertex...)


        // 自动生成的 RenderPassInfo (用于 BeginRendering)
        std::vector<std::pair<uint32_t, RGResourceHandle>> ColorBindings;
        std::pair<int32_t, RGResourceHandle> DepthBinding = {-1, {}}; // -1 表示无深度
        Rhi::ResourceState ExpectedDepthState{Rhi::ResourceState::DepthStencilWrite};

        std::vector<Rhi::TextureBarrier> PreTextureBarriers{}; // Rename for clarity
        std::vector<Rhi::BufferBarrier> PreBufferBarriers{};   // Added Buffer Barriers


        Rhi::RenderPassInfo PassInfo{};

        // 执行逻辑 (Lambda)
        std::function<void(RGContext &)> ExecuteFunc;
    };
    class RGBuilder
    {
    public:
        RGBuilder(RenderGraph *graph, RGPassNode *pass);

        RGResourceHandle ReadTex(RGResourceHandle handle);
        RGResourceHandle ReadBuffer(RGResourceHandle handle);

        RGResourceHandle WriteTex(RGResourceHandle handle);
        RGResourceHandle WriteBuffer(RGResourceHandle handle);
        RGResourceHandle SetColorRT(RGResourceHandle handle, uint32_t index = 0, Rhi::LoadOp load = Rhi::LoadOp::Clear);
        RGResourceHandle SetDepthRT(RGResourceHandle handle, Rhi::LoadOp load = Rhi::LoadOp::Clear, Rhi::StoreOp store = Rhi::StoreOp::Store,
                                    Rhi::ResourceState state= Rhi::ResourceState::DepthStencilWrite);

        RGResourceHandle CreateTexture(const std::string& name, const Rhi::TextureDesc& desc);
        RGResourceHandle CreateBuffer(const std::string& name, const Rhi::BufferDesc& desc);
    private:
        RenderGraph* graph_{};
        RGPassNode* pass_{};
    };
    class RenderGraph
    {
    public:
        RenderGraph(Rhi::RhiDevice* device);
        ~RenderGraph();

        void Reset();

        // 导入外部资源
        RGResourceHandle ImportTexture(const std::string& name, Rhi::RhiTexture* texture,
                                       Rhi::ResourceState initialState = Rhi::ResourceState::Undefined);
        RGResourceHandle AddTextureNode(const std::string& name, const Rhi::TextureDesc& desc);

        RGResourceHandle ImportBuffer(const std::string& name, Rhi::RhiBuffer* buffer,
                                      Rhi::ResourceState initialState = Rhi::ResourceState::Undefined);
        RGResourceHandle AddBufferNode(const std::string& name, const Rhi::BufferDesc& desc);


        // 添加 Pass (模板函数必须放在头文件/接口里)
        template <typename SetupFn, typename ExecFn> void AddPass(std::string name, SetupFn setup, ExecFn execute)
        {
            auto& pass = passes_.emplace_back();
            pass.Name = std::move(name);

            RGBuilder builder(this, &pass);
            setup(builder); // 用户定义依赖

            // 存储执行 Lambda
            pass.ExecuteFunc = [execute](RGContext& ctx) { execute(ctx); };
        }

        void Compile();

        void Execute(Rhi::CommandList* cmd);

        // 内部接口
        Rhi::RhiTexture* GetRhiTexture(RGResourceHandle handle);
        Rhi::RhiBuffer* GetRhiBuffer(RGResourceHandle handle);

    private:
        void ResetCompileState();
        void BuildAdjacencyGraph();
        void PerformTopologicalSort();
        void AllocateResources();
        void GenerateBarriers();

        // 这些纹理在 Frame 之间是持久存在的，但在 Frame 内会被反复借给不同的 Pass
        struct PooledTexture
        {
            std::unique_ptr<Rhi::RhiTexture> Texture;
            bool IsUsed = false;        // 标记当前帧是否被分配了
            uint64_t LastUsedFrame = 0; // 用于未来做垃圾回收(GC)
        };
        std::vector<PooledTexture> texturePool_;

        struct PooledBuffer
        {
            std::unique_ptr<Rhi::RhiBuffer> Buffer;
            bool IsUsed = false;
            uint64_t LastUsedFrame = 0;
        };
        std::vector<PooledBuffer> bufferPool_;


        // 辅助函数：从池子里找一个匹配的，或者创建新的
        Rhi::RhiTexture* AllocateTexture(const Rhi::TextureDesc& desc);
        Rhi::RhiBuffer* AllocateBuffer(const Rhi::BufferDesc& desc);

        Rhi::RhiDevice* device_{};
        std::vector<RGTextureNode> textureNodes_{};
        std::vector<RGBufferNode> bufferNodes_{};
        std::vector<RGPassNode> passes_{}; // 这里需要 RGPassNode 的完整定义，上面已经有了

        std::vector<std::vector<size_t>> currentTextureReaders_;
        std::vector<std::vector<size_t>> currentBufferReaders_;
        std::vector<size_t> latestTextureWriters_;
        std::vector<size_t> latestBufferWriters_;
        
        std::vector<RGPassNode> sortedPasses_;

        std::vector<size_t> sortedIndices_;
        std::vector<size_t> inDegree_;
        std::vector<std::vector<size_t>> adjacencyList_;
        std::queue<size_t> zeroInDegreeQueue_;


    };

}