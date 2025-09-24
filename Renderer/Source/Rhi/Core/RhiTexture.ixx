module;
#include <cstdint>
#include <string>

export module RhiTexture;
import RhiTypes;

export {
    enum class RhiTextureType {
        Texture2D,
        Texture2DArray,
        Texture3D,
        TextureCube
    };

    enum class RhiTextureUsage : uint32_t {
        None = 0,
        Sampled = 1 << 0,
        Storage = 1 << 1,
        ColorAttachment = 1 << 2,
        DepthStencilAttachment = 1 << 3,
        TransferSrc = 1 << 4,
        TransferDst = 1 << 5
    };

    inline RhiTextureUsage operator|(RhiTextureUsage a, RhiTextureUsage b) {
        return static_cast<RhiTextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline RhiTextureUsage operator&(RhiTextureUsage a, RhiTextureUsage b) {
        return static_cast<RhiTextureUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    enum class RhiFilter {
        Nearest,
        Linear
    };

    enum class RhiSamplerAddressMode {
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder
    };

    struct RhiTextureDesc {
        RhiTextureType type = RhiTextureType::Texture2D;
        RhiFormat format = RhiFormat::R8G8B8A8_UNORM;
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        RhiTextureUsage usage = RhiTextureUsage::Sampled;
        std::string debugName;
    };

    struct RhiSamplerDesc {
        RhiFilter minFilter = RhiFilter::Linear;
        RhiFilter magFilter = RhiFilter::Linear;
        RhiSamplerAddressMode addressModeU = RhiSamplerAddressMode::Repeat;
        RhiSamplerAddressMode addressModeV = RhiSamplerAddressMode::Repeat;
        RhiSamplerAddressMode addressModeW = RhiSamplerAddressMode::Repeat;
        float maxAnisotropy = 1.0f;
        bool enableAnisotropy = false;
    };

    struct RhiTextureUploadDesc {
        const void* data = nullptr;
        uint32_t dataSize = 0;
        uint32_t mipLevel = 0;
        uint32_t arrayLayer = 0;
    };
}

// Abstract RHI texture interface
export class RhiTexture {
public:
    virtual ~RhiTexture() = default;
    
    // Resource management
    virtual void* GetNativeHandle() const = 0;
    virtual void* GetNativeImageView() const = 0;
    
    // Texture properties
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual uint32_t GetDepth() const = 0;
    virtual uint32_t GetMipLevels() const = 0;
    virtual uint32_t GetArrayLayers() const = 0;
    virtual RhiFormat GetFormat() const = 0;
    virtual RhiTextureType GetType() const = 0;
};

// Abstract RHI sampler interface
export class RhiSampler {
public:
    virtual ~RhiSampler() = default;
    virtual void* GetNativeHandle() const = 0;
};