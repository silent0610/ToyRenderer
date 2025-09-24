module;
#include <vector>
#include <cstdint>

export module RhiRenderPassDesc;

import RhiTypes;

export {
    enum class RhiLoadOp
    {
        DontCare = 0,
        Load = 1,
        Clear = 2
    };

    enum class RhiStoreOp
    {
        DontCare = 0,
        Store = 1
    };

    enum class RhiImageLayout
    {
        Undefined = 0,
        General = 1,
        ColorAttachmentOptimal = 2,
        DepthStencilAttachmentOptimal = 3,
        DepthStencilReadOnlyOptimal = 4,
        ShaderReadOnlyOptimal = 5,
        TransferSrcOptimal = 6,
        TransferDstOptimal = 7,
        Preinitialized = 8,
        PresentSrcKHR = 1000001002
    };

    struct RhiAttachmentDesc
    {
        RhiFormat format = RhiFormat::Undefined;
        RhiLoadOp loadOp = RhiLoadOp::Clear;
        RhiStoreOp storeOp = RhiStoreOp::Store;
        RhiLoadOp stencilLoadOp = RhiLoadOp::DontCare;
        RhiStoreOp stencilStoreOp = RhiStoreOp::DontCare;
        RhiImageLayout initialLayout = RhiImageLayout::Undefined;
        RhiImageLayout finalLayout = RhiImageLayout::ColorAttachmentOptimal;
    };

    struct RhiRenderPassDesc
    {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<RhiAttachmentDesc> colorAttachments;
        RhiAttachmentDesc depthAttachment;
        bool hasDepthAttachment = false;
    };
}