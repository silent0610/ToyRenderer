module;
#include <cstdint>
export module Engine.Rhi.Definition;
import std;

export namespace Engine::Rhi
{
    enum class ShaderStage : uint32_t
    {
        None = 0,
        Vertex = 1 << 0,   // 1
        Fragment = 1 << 1, // 2
        Geometry = 1 << 2, // 4
        Compute = 1 << 3,  // 8

        // 2. 预定义的常用组合 (可选)
        AllGraphics = Vertex | Fragment | Geometry,
        All = 0x7FFFFFFF
    };

    // 3. 重载位运算符 (这就叫优雅)
    inline ShaderStage operator|(ShaderStage a, ShaderStage b)
    {
        return static_cast<ShaderStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline ShaderStage operator&(ShaderStage a, ShaderStage b)
    {
        return static_cast<ShaderStage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline ShaderStage &operator|=(ShaderStage &a, ShaderStage b)
    {
        a = a | b;
        return a;
    }
    enum class GraphicsBackend : uint8_t
    {
        Vulkan,
        DX12,
        Metal,
        DX11,
        OpenGL,
    };

    enum class PixelFormat : uint8_t
    {
        Unknown = 0,
        R8G8B8A8UNORM,
        B8G8R8A8UNORM,
        D32FLOAT,
    };
    enum class QueueType : uint8_t
    {
        Graphics,
        Compute,
        Transfer
    };

    enum class ResourceState : uint8_t
    {
        Undefined,
        General,
        VertexBuffer,
        IndexBuffer,
        UniformBuffer,
        RenderTarget,
        ShaderResource,
        DepthStencilWrite,
        DepthStencilRead,
        Present,
        TransferSrc,
        TransferDst,
        UnorderedAccess
    };

    enum class CullMode : uint8_t
    {
        None,
        Front,
        Back
    };

    enum class BlendMode : uint8_t
    {
        Opaque,
        AlphaBlend,
        Additive,
        Premultiplied
    };
    enum class CompareOp : uint8_t
    {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum class PolygonMode : uint8_t
    {
        Fill,
        Line,
        Point
    };

    enum class VertexFormat : uint8_t
    {
        Float3,    // x, y, z
        Float2,    // u, v
        Float4,    // x, y, z, w
        UByte4Norm // 颜色
    };

    enum class TextureType : uint8_t
    {
        Texture2D,
        Texture3D,
        Texture2DArray
    };

    enum class TextureUsage : uint32_t
    {
        None = 0,
        TransferSrc = 1 << 0,           // 可以作为拷贝源
        TransferDst = 1 << 1,           // 可以作为拷贝目标
        Sampled = 1 << 2,               // 可以被 Shader 采样 (作为贴图)
        Storage = 1 << 3,               // 可以作为 Compute Shader 的读写目标 (UAV)
        ColorAttachment = 1 << 4,       // 可以作为渲染目标 (RTV)
        DepthStencilAttachment = 1 << 5 // 可以作为深度缓冲 (DSV)
    };
    inline TextureUsage operator|(TextureUsage a, TextureUsage b)
    {
        return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline TextureUsage operator&(TextureUsage a, TextureUsage b)
    {
        return static_cast<TextureUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    enum class LoadOp : uint8_t
    {
        Load,
        Clear,
        DontCare
    };
    enum class StoreOp : uint8_t
    {
        Store,
        DontCare
    };
    struct ClearColor
    {
        float r = 0;
        float g = 0;
        float b = 0;
        float a = 1;
    };
    enum class BufferUsage : uint32_t
    {
        None = 0,
        VertexBuffer = 1 << 0,  // 顶点数据
        IndexBuffer = 1 << 1,   // 索引数据
        UniformBuffer = 1 << 2, // Uniform 数据 (UBO)
        StorageBuffer = 1 << 3, // 结构化存储数据 (SSBO)
        TransferSrc = 1 << 4,   // 作为拷贝源 (CPU -> Staging)
        TransferDst = 1 << 5,   // 作为拷贝目标 (Staging -> GPU)
        IndirectBuffer = 1 << 6 // 间接绘制命令
    };
    inline BufferUsage operator|(BufferUsage a, BufferUsage b)
    {
        return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline BufferUsage operator&(BufferUsage a, BufferUsage b)
    {
        return static_cast<BufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }
    enum class BufferMemoryUsage
    {
        CpuToGpu, // 经常变化的数据 (HostVisible | HostCoherent) -> Uniform Buffer
        GpuOnly,  // 静态数据 (DeviceLocal) -> Vertex/Index Buffer (性能最高)
        CpuOnly   // 回读数据 (HostVisible | HostCached)
    };

    struct BufferDesc
    {
        uint64_t Size = 0;
        BufferUsage Usage = BufferUsage::None;
        BufferMemoryUsage MemoryUsage = BufferMemoryUsage::GpuOnly;
    };
    struct TextureDesc
    {
        uint32_t Width = 1;
        uint32_t Height = 1;
        uint32_t Depth = 1;
        PixelFormat Format = PixelFormat::B8G8R8A8UNORM;
        TextureUsage Usage = TextureUsage::Sampled;
        TextureType Type = TextureType::Texture2D;
        uint32_t MipLevels = 1;
        std::string Name; // 调试名字
    };

    enum class WaitStage : uint8_t
    {
        // 默认：在光栅化写入颜色之前等待 (用于常规图形渲染)
        // 对应 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        ColorAttachmentOutput,

        // 传输/拷贝：在执行 Copy/Blit 之前等待 (用于离屏渲染后 Blit 到屏幕)
        // 对应 VK_PIPELINE_STAGE_TRANSFER_BIT
        Transfer,

        // 计算：在执行 Compute Shader 之前等待 (用于计算着色器写屏)
        // 对应 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        ComputeShader,

        // 片元着色器：在 Pixel Shader 读取纹理之前等待 (用于把上一帧当纹理读)
        // 对应 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        FragmentShader,

        // 顶点着色器：在 Vertex Shader 读取数据前等待
        // 对应 VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
        VertexShader,

        // 最保守：在任何指令开始前等待 (性能最低，但最安全)
        // 对应 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
        AllCommands
    };

    // Sampler
    enum class FilterMode : uint8_t
    {
        Nearest,
        Linear
    };

    enum class SamplerAddressMode : uint8_t
    {
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder
    };
    struct SamplerDesc
    {
        FilterMode MinFilter = FilterMode::Linear;
        FilterMode MagFilter = FilterMode::Linear;
        SamplerAddressMode AddressU = SamplerAddressMode::Repeat;
        SamplerAddressMode AddressV = SamplerAddressMode::Repeat;
        SamplerAddressMode AddressW = SamplerAddressMode::Repeat;
        // 未来可以加 MipLODBias, Anisotropy 等
    };

    enum class DescriptorType : uint8_t
    {
        UniformBuffer,        // UBO (小数据，如 MVP 矩阵，但比 PushConstant 大)
        StorageBuffer,        // SSBO (大数据，如粒子、骨骼矩阵)
        CombinedImageSampler, // 纹理 + 采样器 (最常用)
        StorageImage          // RWTexture (Compute Shader 写入用)
    };
    struct DescriptorBinding
    {
        uint32_t Binding = 0;
        DescriptorType Type = DescriptorType::CombinedImageSampler;
        uint32_t Count = 1;                        // 数组大小 (通常是 1)
        ShaderStage Stage = ShaderStage::Fragment; // 哪个阶段可见
    };
    struct DescriptorSetLayoutDesc
    {
        std::vector<DescriptorBinding> Bindings;
    };
    /// @brief 拷贝区域描述
    struct BufferTextureCopyRegion
    {
        uint64_t BufferOffset = 0;
        uint32_t TextureWidth = 0;
        uint32_t TextureHeight = 0;
        uint32_t TextureDepth = 1;
    };
}
