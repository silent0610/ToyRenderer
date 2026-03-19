module;
#include "vulkan/vulkan.h"
#include "assert.h"

export module Engine.Rhi.Vulkan.Tool;
import std;
import Engine.Rhi.Definition;
export namespace Engine::Rhi::Tool
{
    VkFormat ConvertVertexFormat(VertexFormat format);
    VkCullModeFlags ConvertCullMode(CullMode mode);
    VkPolygonMode ConvertPolygonMode(PolygonMode mode);
    VkCompareOp ConvertCompareOp(CompareOp op);
    VkFormat ConvertPixelFormat(PixelFormat format);
    VkImageUsageFlags ConvertImageUsage(TextureUsage usage);
    VkAttachmentLoadOp ConvertLoadOp(LoadOp op);
    VkPipelineStageFlags ConvertWaitStage(WaitStage stage);
    // --- 辅助函数：转换 StoreOp ---
    VkAttachmentStoreOp ConvertStoreOp(StoreOp op);
    std::string ErrorString(VkResult errorCode);
    inline void CheckResult(VkResult res, const std::source_location &loc = std::source_location::current());
    uint32_t GetVertexFormatSize(VertexFormat format);
    VkBufferUsageFlags ConvertUsage(BufferUsage usage);
    VkShaderStageFlags ConvertShaderStage(ShaderStage stage);
    bool IsDepthFormat(PixelFormat format);
}