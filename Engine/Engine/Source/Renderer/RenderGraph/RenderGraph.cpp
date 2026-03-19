module;
#include <cstdint>
#include "spdlog/spdlog.h"
module Engine.RenderGraph;

namespace Engine
{

	RGContext::RGContext(RenderGraph* graph, const struct RGPassNode& pass, Rhi::CommandList& cmd) : graph_(graph), Cmd(&cmd)
    {
    }

    Rhi::RhiTexture* RGContext::GetTexture(RGResourceHandle handle)
    {
        if (graph_ && handle.Type == RGResourceType::Texture)
        {
            return graph_->GetRhiTexture(handle);
        }
        return nullptr;
    }
    Rhi::RhiBuffer* RGContext::GetBuffer(RGResourceHandle handle)
    {
        if (graph_ && handle.Type == RGResourceType::Buffer)
        {
            return graph_->GetRhiBuffer(handle);
        }
        return nullptr;
    }

    RGBuilder::RGBuilder(RenderGraph* graph, RGPassNode* pass) : graph_(graph), pass_(pass)
    {
    }

    RGResourceHandle RGBuilder::ReadTex(RGResourceHandle handle)
    {
        if (pass_)
        {
            pass_->Reads.push_back(handle);
            pass_->TextureList.push_back(handle);
        }
        return handle;
    }
    RGResourceHandle RGBuilder::SetColorRT(RGResourceHandle handle, uint32_t index, Rhi::LoadOp load)
    {
        if (!pass_ || !graph_)
        {
            return handle;
        }

        // 1. 记录写入依赖
        pass_->Writes.push_back(handle);
        pass_->ColorBindings.push_back({index, handle});


        // 对于内部瞬态资源，需要在 Compile 阶段分配后回填

        Rhi::RenderAttachment attach;
        attach.Texture = nullptr;
        attach.Load = load;
        attach.Store = Rhi::StoreOp::Store;
        attach.ClearValue = {0.0f,0.1f,0.0f,0.0f};
        // 确保数组够大
        if (pass_->PassInfo.ColorTargets.size() <= index)
        {
            pass_->PassInfo.ColorTargets.resize(index + 1);
        }

        pass_->PassInfo.ColorTargets[index] = attach;

        return handle;
    }
    RGResourceHandle RGBuilder::CreateTexture(const std::string& name, const Rhi::TextureDesc& desc)
    {
        RGResourceHandle handle = graph_->AddTextureNode(name, desc);
        // 创建出来的纹理，通常默认就要被当前 Pass 写入 (或者作为 Attachment)
        // 但为了灵活性，我们只返回句柄，让用户自己调 WriteColor
        return handle;
    }
    RGResourceHandle RGBuilder::CreateBuffer(const std::string& name, const Rhi::BufferDesc& desc)
    {
        RGResourceHandle handle = graph_->AddBufferNode(name, desc);
        return handle;
    }

    RGResourceHandle RGBuilder::ReadBuffer(RGResourceHandle handle)
    {
        if (pass_)
        {
            pass_->Reads.push_back(handle);
            pass_->BufferList.push_back(handle);
        }
        return handle;
    }

    RGResourceHandle RGBuilder::WriteBuffer(RGResourceHandle handle)
    {
        if (pass_)
        {
            pass_->Writes.push_back(handle);
            pass_->BufferList.push_back(handle);
        }
        return handle;
    }

    RGResourceHandle RGBuilder::SetDepthRT(RGResourceHandle handle, Rhi::LoadOp load, Rhi::StoreOp store, Rhi::ResourceState state)
    {
        if (!pass_ || !graph_)
        {
            return handle;
        }

        pass_->Writes.push_back(handle);
        pass_->DepthBinding = {0, handle};

        pass_->PassInfo.EnableDepth = true;
        // 如果需要，这里也可以根据 desc.Format 判断是否启用 Stencil
        // pass_->PassInfo.EnableStencil = ...; 
        
        pass_->PassInfo.DepthStencilTarget.Load = load;
        pass_->PassInfo.DepthStencilTarget.Store = store;
        pass_->PassInfo.DepthStencilTarget.Texture = nullptr; // Fill in Compile

        pass_->ExpectedDepthState = state;

        // 默认 Clear Values
        pass_->PassInfo.ClearDepth = 1.0f;
        pass_->PassInfo.ClearStencil = 0;

        return handle;
    }

    RGResourceHandle RGBuilder::WriteTex(RGResourceHandle handle)
    {
        if (!pass_ || !graph_)
        {
            return handle;
        }

        // 1. 记录写入依赖 (用于依赖图构建 & Barrier 推导)
        pass_->Writes.push_back(handle);

        // 2. 添加到纹理列表 (用于绑定到 Shader)
        // 注意：ColorTarget 是用于 OutputMerger 的，而 WriteTex 是用于 Shader (UAV) 的
        pass_->TextureList.push_back(handle);

        return handle;
    }
    RGResourceHandle RenderGraph::AddTextureNode(const std::string& name, const Rhi::TextureDesc& desc)
    {
        RGTextureNode node;
        node.Name = name;
        node.Desc = desc;
        node.ImportedTexture = nullptr; // 这是内部资源，没有外部指针
        node.InternalTexture = nullptr; // 还没编译，没有物理显存
        node.CurrentState = Rhi::ResourceState::Undefined;
        textureNodes_.push_back(node);
        return RGResourceHandle{static_cast<uint32_t>(textureNodes_.size() - 1), 0, RGResourceType::Texture};
    }
    RGResourceHandle RenderGraph::AddBufferNode(const std::string& name, const Rhi::BufferDesc& desc)
    {
        RGBufferNode node;
        node.Name = name;
        node.Desc = desc;
        node.ImportedBuffer = nullptr;
        node.InternalBuffer = nullptr;
        node.CurrentState = Rhi::ResourceState::Undefined;
        bufferNodes_.push_back(node);
        return RGResourceHandle{static_cast<uint32_t>(bufferNodes_.size() - 1), 0, RGResourceType::Buffer};
    }
    void RenderGraph::Reset()
    {
        passes_.clear();
        textureNodes_.clear();
        textureNodes_.emplace_back(); // Dummy

        bufferNodes_.clear();
        bufferNodes_.emplace_back(); // Dummy

        // 把池子里的所有纹理标记为“未使用”, 不销毁它们，而是留给下一帧复用
        for (auto& pt : texturePool_)
        {
            pt.IsUsed = false;
        }
        for (auto& pb : bufferPool_)
        {
            pb.IsUsed = false;
        }
    }
    RenderGraph::RenderGraph(Rhi::RhiDevice* device) : device_(device)
    {
        // 初始化资源池，推入一个无效的 Dummy 节点 (Index 0)
        // 这样 IsValid() (Index != 0) 逻辑才能成立
        textureNodes_.emplace_back();
        bufferNodes_.emplace_back();
    }
    RenderGraph::~RenderGraph()
    {
    }



    RGResourceHandle RenderGraph::ImportTexture(const std::string& name, Rhi::RhiTexture* texture,
                                                Rhi::ResourceState initialState)
    {
        RGTextureNode node;
        node.Name = name;
        node.ImportedTexture = texture;
        if (texture)
        {
            node.Desc = texture->GetDesc();
        }
        node.CurrentState = initialState;
        textureNodes_.push_back(node);

        
        return RGResourceHandle{static_cast<uint32_t>(textureNodes_.size() - 1), 0, RGResourceType::Texture};
    }
    RGResourceHandle RenderGraph::ImportBuffer(const std::string& name, Rhi::RhiBuffer* buffer,
                                                Rhi::ResourceState initialState)
    {
        RGBufferNode node;
        node.Name = name;
        node.ImportedBuffer = buffer;
        if (buffer)
        {
            node.Desc = buffer->GetDesc();
        }
        node.CurrentState = initialState;
        bufferNodes_.push_back(node);

        return RGResourceHandle{static_cast<uint32_t>(bufferNodes_.size() - 1), 0, RGResourceType::Buffer};
    }

    Rhi::RhiTexture* RenderGraph::GetRhiTexture(RGResourceHandle handle)
    {
        if (handle.IsValid() && handle.Index < textureNodes_.size() && handle.Type == RGResourceType::Texture)
        {
            return textureNodes_[handle.Index].GetRhiTexture();
        }
        return nullptr;
    }
    Rhi::RhiBuffer* RenderGraph::GetRhiBuffer(RGResourceHandle handle)
    {
        if (handle.IsValid() && handle.Index < bufferNodes_.size() && handle.Type == RGResourceType::Buffer)
        {
            return bufferNodes_[handle.Index].GetRhiBuffer();
        }
        return nullptr;
    }
    void RenderGraph::Compile()
    {
        ResetCompileState();
        BuildAdjacencyGraph();
        PerformTopologicalSort();
        AllocateResources();
        GenerateBarriers();
    }
  
    Rhi::RhiTexture* RenderGraph::AllocateTexture(const Rhi::TextureDesc& desc)
    {
        // A. 尝试在池子里找一个闲置的、描述符匹配的纹理
        for (auto& pt : texturePool_)
        {
            if (!pt.IsUsed) // 必须闲置
            {
                auto& tex = pt.Texture;
                // 检查规格是否匹配 (宽、高、格式、用途)
                // 注意：这里可以做宽松匹配(比如池子里的图比需求大)，但为了简单，先做严格匹配
                if (tex->GetWidth() == desc.Width && tex->GetHeight() == desc.Height && tex->GetFormat() == desc.Format &&
                    // 用途必须包含请求的所有用途 (可以更多，不能更少)
                    // 简单的位运算检查：(Existing & Requested) == Requested
                    // 但为了保险，目前先要求完全相等
                    tex->GetDesc().Usage == desc.Usage)
                {
                    pt.IsUsed = true; // 标记为已用
                    return tex.get();
                }
            }
        }

        // B. 如果没找到，创建一个新的
        spdlog::debug("[RenderGraph] Creating new transient texture: {}x{} ({})", desc.Width, desc.Height, (int)desc.Format);

        PooledTexture newPoolItem;
        newPoolItem.Texture = device_->CreateTexture(desc);
        newPoolItem.IsUsed = true;

        Rhi::RhiTexture* ptr = newPoolItem.Texture.get();
        texturePool_.push_back(std::move(newPoolItem));

        return ptr;
    }
    Rhi::RhiBuffer* RenderGraph::AllocateBuffer(const Rhi::BufferDesc& desc)
    {
        for (auto& pb : bufferPool_)
        {
            if (!pb.IsUsed)
            {
                auto& buf = pb.Buffer;
                if (buf->GetSize() == desc.Size && buf->GetDesc().Usage == desc.Usage)
                {
                    pb.IsUsed = true;
                    return buf.get();
                }
            }
        }
        
        spdlog::debug("[RenderGraph] Creating new transient buffer: Size {}", desc.Size);
        PooledBuffer newPoolItem;
        newPoolItem.Buffer = device_->CreateBuffer(desc);
        newPoolItem.IsUsed = true;
        
        Rhi::RhiBuffer* ptr = newPoolItem.Buffer.get();
        bufferPool_.push_back(std::move(newPoolItem));
        return ptr;
    }

    void RenderGraph::Execute(Rhi::CommandList* cmd)
    {
        for (auto& pass : passes_)
        {
            for (const auto& barrier : pass.PreTextureBarriers)
            {
                cmd->SetResourceBarrier(barrier);
            }
            for (const auto& barrier : pass.PreBufferBarriers)
            {
                cmd->SetResourceBarrier(barrier);
            }

            // 2. 开启渲染通道
            // 如果 PassInfo 中有颜色或深度目标，则调用 BeginRendering
            bool isGraphicsPass = !pass.PassInfo.ColorTargets.empty() || pass.PassInfo.DepthStencilTarget.Texture != nullptr;

            if (isGraphicsPass)
            {
                cmd->BeginRendering(pass.PassInfo);
            }

            // 3. 执行用户逻辑
            RGContext ctx(this, pass, *cmd);
            if (pass.ExecuteFunc)
            {
                pass.ExecuteFunc(ctx);
            }

            // 4. 结束渲染通道
            if (isGraphicsPass)
            {
                cmd->EndRendering();
            }
        }
    }
    void RenderGraph::ResetCompileState()
    {
        size_t numPasses = passes_.size();
        size_t numTextures = textureNodes_.size(); // 假设资源ID对应索引
        size_t numBuffers = bufferNodes_.size();

        // 1. 重置拓扑排序容器
        sortedIndices_.clear();
        sortedIndices_.reserve(numPasses);

        inDegree_.assign(numPasses, 0); // 重置并调整大小

        adjacencyList_.assign(numPasses, {}); // 清空邻接表
        for (auto& list : adjacencyList_)
        {
            list.reserve(4); // 预留一点空间
        }

        // 2. 重置依赖追踪器 (使用 vector 替代 map 以获得 O(1) 访问)
        // -1 (或 size_t max) 代表无写入者
        latestTextureWriters_.assign(numTextures, std::numeric_limits<size_t>::max());
        latestBufferWriters_.assign(numBuffers, std::numeric_limits<size_t>::max());

        // 清空并重置读者列表
        if (currentTextureReaders_.size() < numTextures)
        {
            currentTextureReaders_.resize(numTextures);
        }
        for (auto& vec : currentTextureReaders_)
        {
            vec.clear();
        }

        if (currentBufferReaders_.size() < numBuffers)
        {
            currentBufferReaders_.resize(numBuffers);
        }
        for (auto& vec : currentBufferReaders_)
        {
            vec.clear();
        }
    }
    void RenderGraph::BuildAdjacencyGraph()
    {
        // 定义 Helper Lambda 变得更轻量，因为它们直接访问成员
        auto ResolveWar = [&](RGResourceHandle h, size_t writerIdx) {
            auto& readers = (h.Type == RGResourceType::Texture) ? currentTextureReaders_[h.Index] : currentBufferReaders_[h.Index];

            for (size_t readerIdx : readers)
            {
                if (readerIdx != writerIdx)
                {
                    adjacencyList_[readerIdx].push_back(writerIdx);
                    inDegree_[writerIdx]++;
                }
            }
            readers.clear(); // 关键：阻挡解除
        };

        auto AddReader = [&](RGResourceHandle h, size_t readerIdx) {
            if (h.Type == RGResourceType::Texture)
            {
                currentTextureReaders_[h.Index].push_back(readerIdx);
            }
            else
            {
                currentBufferReaders_[h.Index].push_back(readerIdx);
            }
        };

        auto GetLatestWriter = [&](RGResourceHandle h) -> size_t {
            size_t idx = (h.Type == RGResourceType::Texture) ? latestTextureWriters_[h.Index] : latestBufferWriters_[h.Index];
            return idx;
        };

        auto SetLatestWriter = [&](RGResourceHandle h, size_t writerIdx) {
            if (h.Type == RGResourceType::Texture)
            {
                latestTextureWriters_[h.Index] = writerIdx;
            }
            else
            {
                latestBufferWriters_[h.Index] = writerIdx;
            }
        };

        // --- 核心遍历 ---
        for (size_t i = 0; i < passes_.size(); ++i)
        {
            const auto& pass = passes_[i];

            // RAW & Register Reader
            for (const auto& handle : pass.Reads)
            {
                size_t producerIdx = GetLatestWriter(handle);
                if (producerIdx != std::numeric_limits<size_t>::max())
                {
                    adjacencyList_[producerIdx].push_back(i);
                    inDegree_[i]++;
                }
                AddReader(handle, i);
            }

            // WAR & WAW
            for (const auto& handle : pass.Writes)
            {
                ResolveWar(handle, i); // WAR

                size_t prevWriterIdx = GetLatestWriter(handle);
                if (prevWriterIdx != std::numeric_limits<size_t>::max() && prevWriterIdx != i)
                {
                    adjacencyList_[prevWriterIdx].push_back(i); // WAW
                    inDegree_[i]++;
                }

                SetLatestWriter(handle, i);
            }
        }
    }

    void RenderGraph::PerformTopologicalSort()
    {
        // Kahn's Algorithm
        // Queue 比较轻量，可以用局部变量，或者用 vector 模拟 queue 来复用内存
        std::queue<size_t> zeroQueue;

        for (size_t i = 0; i < passes_.size(); ++i)
        {
            if (inDegree_[i] == 0)
            {
                zeroQueue.push(i);
            }
        }

        while (!zeroQueue.empty())
        {
            size_t u = zeroQueue.front();
            zeroQueue.pop();
            sortedIndices_.push_back(u);

            for (size_t v : adjacencyList_[u])
            {
                inDegree_[v]--;
                if (inDegree_[v] == 0)
                {
                    zeroQueue.push(v);
                }
            }
        }

        // 环路检测
        if (sortedIndices_.size() != passes_.size())
        {
            spdlog::error("[RenderGraph] Cyclic dependency! Sorted {}/{}", sortedIndices_.size(), passes_.size());
            throw std::runtime_error("RenderGraph Cycle");
        }

        // 重组 Pass 列表
        // 使用 swap 技巧避免重新分配 Element 内存
        sortedPasses_.clear();
        sortedPasses_.reserve(passes_.size());
        for (size_t idx : sortedIndices_)
        {
            sortedPasses_.push_back(std::move(passes_[idx]));
        }
        passes_ = std::move(sortedPasses_); // 赋值回成员
    }
    void RenderGraph::AllocateResources()
    {
        for (auto& node : textureNodes_)
        {
            if (node.Name.empty())
            {
                continue;
            }
            if (node.ImportedTexture)
            {
                continue;
            }

            node.InternalTexture = AllocateTexture(node.Desc);
            if (!node.InternalTexture)
            {
                spdlog::error("Failed to allocate texture for node: {}", node.Name);
            }
        }

        // Buffer Allocation
        for (auto& node : bufferNodes_)
        {
            if (node.Name.empty())
            {
                continue;
            }
            if (node.ImportedBuffer)
            {
                continue;
            }

            node.InternalBuffer = AllocateBuffer(node.Desc);
            if (!node.InternalBuffer)
            {
                spdlog::error("Failed to allocate buffer for node: {}", node.Name);
            }
        }

    }
    void RenderGraph::GenerateBarriers()
    {
        for (auto& pass : passes_)
        {
            pass.PreTextureBarriers.clear();
            pass.PreBufferBarriers.clear();

            for (auto& binding : pass.ColorBindings)
            {
                uint32_t slotIndex = binding.first;
                RGResourceHandle handle = binding.second; // 从 Pair 中取出 Handle

                RGTextureNode& node = textureNodes_[handle.Index];
                Rhi::ResourceState requiredState = Rhi::ResourceState::RenderTarget;

                // A. 处理 Barrier
                if (node.CurrentState != requiredState)
                {
                    pass.PreTextureBarriers.push_back({node.GetRhiTexture(), node.CurrentState, requiredState});
                    node.CurrentState = requiredState;
                }

                // B. 回填 RHI 指针
                // 因为我们知道确切的 slotIndex，直接填空即可，不需要再由 Handle 反查 Index
                pass.PassInfo.ColorTargets[slotIndex].Texture = node.GetRhiTexture();
            }

            if (pass.DepthBinding.first != -1)
            {
                RGResourceHandle handle = pass.DepthBinding.second;
                RGTextureNode& node = textureNodes_[handle.Index];

                Rhi::ResourceState requiredState = pass.ExpectedDepthState; 

                if (node.CurrentState != requiredState)
                {
                    Rhi::TextureBarrier barrier;
                    barrier.Texture = node.GetRhiTexture();
                    barrier.Before = node.CurrentState;
                    barrier.After = requiredState;

                    pass.PreTextureBarriers.push_back(barrier);
                    node.CurrentState = requiredState;
                }
                pass.PassInfo.DepthStencilTarget.Texture = GetRhiTexture(pass.DepthBinding.second);
            }

            for (RGResourceHandle handle : pass.TextureList)
            {
                RGTextureNode& node = textureNodes_[handle.Index];
                Rhi::ResourceState requiredState = Rhi::ResourceState::ShaderResource;

                bool isWrite = std::find(pass.Writes.begin(), pass.Writes.end(), handle) != pass.Writes.end();

                if (isWrite)
                {
                    // 如果在 TextureList 中且是 Write，说明是 Compute Shader 写入 (UAV / Storage Image)
                    requiredState = Rhi::ResourceState::UnorderedAccess;
                }
                else
                {
                    // 否则是采样读取 (SRV / Sampled Image)
                    requiredState = Rhi::ResourceState::ShaderResource;
                }

                if (node.CurrentState != requiredState)
                {
                    pass.PreTextureBarriers.push_back({node.GetRhiTexture(), node.CurrentState, requiredState});
                    node.CurrentState = requiredState;
                }

            }
            for (RGResourceHandle handle : pass.BufferList)
            {
                RGBufferNode& node = bufferNodes_[handle.Index];
                Rhi::ResourceState requiredState = Rhi::ResourceState::ShaderResource;

                // 判断逻辑：该 Buffer 在本 Pass 是写还是读？
                // 我们去 pass.Writes 里找一下是否存在
                bool isWrite = std::find(pass.Writes.begin(), pass.Writes.end(), handle) != pass.Writes.end();

                if (isWrite)
                {
                    requiredState = Rhi::ResourceState::UnorderedAccess;
                }
                else
                {
                    // 推导读状态
                    if ((node.Desc.Usage & Rhi::BufferUsage::UniformBuffer) != Rhi::BufferUsage::None)
                    {
                        requiredState = Rhi::ResourceState::UniformBuffer;
                    }
                    else if ((node.Desc.Usage & Rhi::BufferUsage::VertexBuffer) != Rhi::BufferUsage::None)
                    {
                        requiredState = Rhi::ResourceState::VertexBuffer;
                    }
                    else if ((node.Desc.Usage & Rhi::BufferUsage::IndexBuffer) != Rhi::BufferUsage::None)
                    {
                        requiredState = Rhi::ResourceState::IndexBuffer;
                    }
                    else
                    {
                        requiredState = Rhi::ResourceState::ShaderResource; // Default Storage Read
                    }
                }

                if (node.CurrentState != requiredState)
                {
                    pass.PreBufferBarriers.push_back({node.GetRhiBuffer(), node.CurrentState, requiredState});
                    node.CurrentState = requiredState;
                }
            }
        }
    }
}