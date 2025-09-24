module;
#include <cstdint>
#include <string>

export module RhiTypes;

// Core RHI result codes
export enum class RhiResult : uint32_t
{
    Success = 0,
    ErrorOutOfMemory,
    ErrorDeviceLost,
    ErrorInvalidParameter,
    ErrorInitializationFailed
};

// Buffer usage flags
export enum class RhiBufferUsage : uint32_t
{
    None = 0,
    Vertex = 1 << 0,
    Index = 1 << 1,
    Uniform = 1 << 2,
    Storage = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5
};

// Enable bitwise operations for RhiBufferUsage
export inline RhiBufferUsage operator|(RhiBufferUsage a, RhiBufferUsage b) {
    return static_cast<RhiBufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

export inline RhiBufferUsage operator&(RhiBufferUsage a, RhiBufferUsage b) {
    return static_cast<RhiBufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}


// Texture formats (comprehensive set for rendering)
export enum class RhiFormat : uint32_t
{
    Undefined = 0,
    
    // 8-bit normalized formats
    R8_UNORM,
    R8G8_UNORM,
    R8G8B8_UNORM,
    R8G8B8A8_UNORM,
    R8G8B8A8_SRGB,     // SRGB variant
    B8G8R8A8_UNORM,    // Common swapchain format
    B8G8R8A8_SRGB,     // SRGB variant for swapchain
    A8B8G8R8_UNORM_PACK32,
    
    // 16-bit float formats
    R16_SFLOAT,
    R16G16_SFLOAT,
    R16G16B16_SFLOAT,
    R16G16B16A16_SFLOAT,
    
    // 32-bit float formats
    R32_SFLOAT,
    R32G32_SFLOAT,     // 2 component float
    R32G32B32_SFLOAT,  // 3 component float
    R32G32B32A32_SFLOAT,
    
    // Depth formats
    D16_UNORM,
    D24_UNORM_S8_UINT,
    D32_SFLOAT,
    D32_SFLOAT_S8_UINT,
    
    // Integer formats
    R32_UINT,
    R32G32_UINT,
    R32G32B32_UINT,
    R32G32B32A32_UINT
};

// Basic viewport
export struct RhiViewport
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

// Basic scissor rectangle
export struct RhiRect2D
{
    struct {
        int32_t x = 0;
        int32_t y = 0;
    } offset;
    
    struct {
        uint32_t width = 0;
        uint32_t height = 0;
    } extent;
};

// Memory usage types for buffers
export enum class RhiMemoryUsage {
    GPU_Only,        // GPU memory, not accessible by CPU
    CPU_Only,        // CPU memory, not accessible by GPU (staging)
    CPU_TO_GPU,      // CPU can write, GPU can read (uniform buffers)
    GPU_TO_CPU       // GPU can write, CPU can read (readback)
};

// Vertex input rate
export enum class RhiVertexInputRate {
    Vertex,     // Input rate per vertex
    Instance    // Input rate per instance
};

// Vertex input attribute description
export struct RhiVertexAttributeDesc {
    uint32_t location;
    uint32_t binding; 
    RhiFormat format;
    uint32_t offset;
};

// Vertex input binding description  
export struct RhiVertexBindingDesc {
    uint32_t binding;
    uint32_t stride;
    RhiVertexInputRate inputRate;
};

// Buffer descriptor
export struct RhiBufferDesc
{
    size_t size = 0;
    RhiBufferUsage usage = RhiBufferUsage::None;
    RhiMemoryUsage memoryUsage = RhiMemoryUsage::GPU_Only;
    const void* initialData = nullptr;
    std::string debugName;
};

// Device creation descriptor
export struct RhiDeviceDesc
{
    void* windowHandle = nullptr;
    bool enableValidation = false;
    const char* applicationName = "RHI Application";
};