module;
#include "spdlog/spdlog.h"
#include "vulkan/vulkan.h"
module Engine.Rhi.Vulkan.Tool;
import std;
namespace Engine::Rhi::Tool
{

    VkShaderStageFlags ConvertShaderStage(ShaderStage stage)
    {
        VkShaderStageFlags flags = 0;

        // 这种写法支持任意组合：Vertex | Fragment | Compute ...
        if ((stage & ShaderStage::Vertex) != ShaderStage::None)
            flags |= VK_SHADER_STAGE_VERTEX_BIT;
        if ((stage & ShaderStage::Fragment) != ShaderStage::None)
            flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if ((stage & ShaderStage::Geometry) != ShaderStage::None)
            flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
        if ((stage & ShaderStage::Compute) != ShaderStage::None)
            flags |= VK_SHADER_STAGE_COMPUTE_BIT;

        return flags;
    }
    uint32_t GetVertexFormatSize(VertexFormat format)
    {
        switch (format)
        {
        case VertexFormat::Float3:
            return sizeof(float) * 3;
        case VertexFormat::Float2:
            return sizeof(float) * 2;
        case VertexFormat::Float4:
            return sizeof(float) * 4;
        case VertexFormat::UByte4Norm:
            return sizeof(uint32_t); // 4 bytes
        default:
            return 0;
        }
    }
    std::string ErrorString(VkResult errorCode)
    {
        switch (errorCode)
        {
#define STR(r)   \
    case VK_##r: \
        return #r
            STR(NOT_READY);
            STR(TIMEOUT);
            STR(EVENT_SET);
            STR(EVENT_RESET);
            STR(INCOMPLETE);
            STR(ERROR_OUT_OF_HOST_MEMORY);
            STR(ERROR_OUT_OF_DEVICE_MEMORY);
            STR(ERROR_INITIALIZATION_FAILED);
            STR(ERROR_DEVICE_LOST);
            STR(ERROR_MEMORY_MAP_FAILED);
            STR(ERROR_LAYER_NOT_PRESENT);
            STR(ERROR_EXTENSION_NOT_PRESENT);
            STR(ERROR_FEATURE_NOT_PRESENT);
            STR(ERROR_INCOMPATIBLE_DRIVER);
            STR(ERROR_TOO_MANY_OBJECTS);
            STR(ERROR_FORMAT_NOT_SUPPORTED);
            STR(ERROR_SURFACE_LOST_KHR);
            STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
            STR(SUBOPTIMAL_KHR);
            STR(ERROR_OUT_OF_DATE_KHR);
            STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
            STR(ERROR_VALIDATION_FAILED_EXT);
            STR(ERROR_INVALID_SHADER_NV);
            STR(ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);
#undef STR
        default:
            return "UNKNOWN_ERROR";
        }
    };
    inline void CheckResult(VkResult res, const std::source_location &loc)
    {
        if (res != VK_SUCCESS)
        {

            spdlog::error("Error Code:{}\nFatal : VkResult is \"{}\" in {} at line {} in function {}", static_cast<int>(res), Tool::ErrorString(res),
                          loc.file_name(), loc.line(), loc.function_name());
            assert(res == VK_SUCCESS);
        }
    }
    VkFormat ConvertVertexFormat(VertexFormat format)
    {
        switch (format)
        {
        case VertexFormat::Float3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexFormat::Float2:
            return VK_FORMAT_R32G32_SFLOAT;
        case VertexFormat::Float4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VertexFormat::UByte4Norm:
            return VK_FORMAT_R8G8B8A8_UNORM;
        default:
            return VK_FORMAT_UNDEFINED;
        }
    }
    VkCullModeFlags ConvertCullMode(CullMode mode)
    {
        switch (mode)
        {
        case CullMode::None:
            return VK_CULL_MODE_NONE;
        case CullMode::Front:
            return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:
            return VK_CULL_MODE_BACK_BIT;
        default:
            return VK_CULL_MODE_BACK_BIT;
        }
    }

    VkPolygonMode ConvertPolygonMode(PolygonMode mode)
    {
        switch (mode)
        {
        case PolygonMode::Fill:
            return VK_POLYGON_MODE_FILL;
        case PolygonMode::Line:
            return VK_POLYGON_MODE_LINE;
        case PolygonMode::Point:
            return VK_POLYGON_MODE_POINT;
        default:
            return VK_POLYGON_MODE_FILL;
        }
    }

    VkCompareOp ConvertCompareOp(CompareOp op)
    {
        switch (op)
        {
        case CompareOp::Less:
            return VK_COMPARE_OP_LESS;
        case CompareOp::LessOrEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater:
            return VK_COMPARE_OP_GREATER;
        case CompareOp::Always:
            return VK_COMPARE_OP_ALWAYS;
        // ... 其他 ...
        default:
            return VK_COMPARE_OP_LESS;
        }
    }

    VkFormat ConvertPixelFormat(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::B8G8R8A8UNORM:
        {
            return VK_FORMAT_B8G8R8A8_UNORM;
        }
        case PixelFormat::R8G8B8A8UNORM:
        {
            return VK_FORMAT_R8G8B8A8_UNORM;
        }
        case PixelFormat::D32FLOAT:
        {
            return VK_FORMAT_D32_SFLOAT;
        }
        default:
        {
            return VK_FORMAT_UNDEFINED;
        }
        }
    }
    VkImageUsageFlags ConvertImageUsage(TextureUsage usage)
    {
        VkImageUsageFlags result{0};

        if ((usage & TextureUsage::TransferSrc) != TextureUsage::None)
        {
            result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        if ((usage & TextureUsage::TransferDst) != TextureUsage::None)
        {
            result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        if ((usage & TextureUsage::Sampled) != TextureUsage::None)
        {
            result |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        if ((usage & TextureUsage::ColorAttachment) != TextureUsage::None)
        {
            result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if ((usage & TextureUsage::DepthStencilAttachment) != TextureUsage::None)
        {
            result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }

        return result;
    }
    VkAttachmentLoadOp ConvertLoadOp(LoadOp op)
    {
        switch (op)
        {
        case LoadOp::Load:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case LoadOp::Clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case LoadOp::DontCare:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
    }

    // --- 辅助函数：转换 StoreOp ---
    VkAttachmentStoreOp ConvertStoreOp(StoreOp op)
    {
        switch (op)
        {
        case StoreOp::Store:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case StoreOp::DontCare:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
    }

    VkPipelineStageFlags ConvertWaitStage(Engine::Rhi::WaitStage stage)
    {
        switch (stage)
        {
        case WaitStage::ColorAttachmentOutput:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case WaitStage::Transfer:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case WaitStage::ComputeShader:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case WaitStage::FragmentShader:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case WaitStage::VertexShader:
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        case WaitStage::AllCommands:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // 或者 ALL_COMMANDS_BIT

        // 默认兜底：颜色输出
        default:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
    }

    VkBufferUsageFlags ConvertUsage(BufferUsage usage)
    {
        VkBufferUsageFlags flags = 0;
        if ((usage & BufferUsage::VertexBuffer) != BufferUsage::None)
            flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if ((usage & BufferUsage::IndexBuffer) != BufferUsage::None)
            flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if ((usage & BufferUsage::UniformBuffer) != BufferUsage::None)
            flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if ((usage & BufferUsage::StorageBuffer) != BufferUsage::None)
            flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if ((usage & BufferUsage::TransferSrc) != BufferUsage::None)
            flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if ((usage & BufferUsage::TransferDst) != BufferUsage::None)
            flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        return flags;
    }
    bool IsDepthFormat(PixelFormat format)
    {
        return format == PixelFormat::D32FLOAT;
    }
} // namespace Engine::Rhi::Tool