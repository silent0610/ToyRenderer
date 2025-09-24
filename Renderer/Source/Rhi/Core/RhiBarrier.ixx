module;
#include <vector>
#include <cstdint>

export module RhiBarrier;

import RhiTypes;

export {
    enum class RhiPipelineStage : uint32_t
    {
        TopOfPipe = 1 << 0,
        VertexInput = 1 << 1,
        VertexShader = 1 << 2,
        FragmentShader = 1 << 3,
        EarlyFragmentTests = 1 << 4,
        LateFragmentTests = 1 << 5,
        ColorAttachmentOutput = 1 << 6,
        ComputeShader = 1 << 7,
        Transfer = 1 << 8,
        BottomOfPipe = 1 << 9,
        AllCommands = 1 << 10
    };

    enum class RhiAccessFlags : uint32_t
    {
        None = 0,
        MemoryRead = 1 << 0,
        MemoryWrite = 1 << 1,
        ColorAttachmentRead = 1 << 2,
        ColorAttachmentWrite = 1 << 3,
        DepthStencilAttachmentRead = 1 << 4,
        DepthStencilAttachmentWrite = 1 << 5,
        TransferRead = 1 << 6,
        TransferWrite = 1 << 7,
        ShaderRead = 1 << 8,
        ShaderWrite = 1 << 9
    };

    struct RhiMemoryBarrier
    {
        RhiAccessFlags srcAccessMask = RhiAccessFlags::None;
        RhiAccessFlags dstAccessMask = RhiAccessFlags::None;
    };

    struct RhiBufferMemoryBarrier
    {
        RhiAccessFlags srcAccessMask = RhiAccessFlags::None;
        RhiAccessFlags dstAccessMask = RhiAccessFlags::None;
        void* buffer = nullptr; // RhiBuffer* stored as void*
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    struct RhiBarrierDesc
    {
        RhiPipelineStage srcStage = RhiPipelineStage::TopOfPipe;
        RhiPipelineStage dstStage = RhiPipelineStage::BottomOfPipe;
        std::vector<RhiMemoryBarrier> memoryBarriers;
        std::vector<RhiBufferMemoryBarrier> bufferMemoryBarriers;
    };
    
    // Operator overloads for bitwise operations
    inline RhiPipelineStage operator|(RhiPipelineStage a, RhiPipelineStage b)
    {
        return static_cast<RhiPipelineStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    
    inline RhiAccessFlags operator|(RhiAccessFlags a, RhiAccessFlags b)
    {
        return static_cast<RhiAccessFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
}