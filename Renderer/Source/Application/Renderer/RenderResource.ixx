module;
#include <string>
#include <memory>

export module RenderResource;
import Core;
import RhiTexture;
import RhiBuffer;
import RhiTypes;
import std;

// Forward declarations
export class RenderTarget;

// Render resource types for render graph
export enum class RenderResourceType {
    Texture2D,
    TextureCube,
    Buffer,
    RenderTarget
};

export enum class RenderResourceUsage {
    // Input usages
    ShaderRead,        // Texture sampling in shaders
    UniformBuffer,     // Uniform buffer binding
    StorageRead,       // Storage buffer/image read
    
    // Output usages  
    RenderTarget,      // Color attachment
    DepthStencil,      // Depth/stencil attachment
    StorageWrite,      // Storage buffer/image write
    
    // Input/Output
    StorageReadWrite   // Read-modify-write
};

// Resource handle for render graph dependency tracking
export struct RenderResourceHandle {
    uint32_t id;
    std::string name;
    RenderResourceType type;
    
    RenderResourceHandle() : id(0) {}
    RenderResourceHandle(uint32_t _id, const std::string& _name, RenderResourceType _type)
        : id(_id), name(_name), type(_type) {}
    
    bool IsValid() const { return id != 0; }
    
    bool operator==(const RenderResourceHandle& other) const {
        return id == other.id;
    }
    
    bool operator!=(const RenderResourceHandle& other) const {
        return !(*this == other);
    }
};

// Resource descriptor for creation
export struct RenderResourceDesc {
    std::string name;
    RenderResourceType type;
    
    // Texture properties
    uint32_t width = 0;
    uint32_t height = 0; 
    uint32_t depth = 1;
    RhiFormat format = RhiFormat::R8G8B8A8_UNORM;
    RhiTextureUsage usage = RhiTextureUsage::None;
    
    // Buffer properties
    uint64_t bufferSize = 0;
    RhiBufferUsage bufferUsage = RhiBufferUsage::None;
    
    // Render target properties
    bool isSwapchain = false;
    RhiFormat depthFormat = RhiFormat::Undefined;
};

// Actual resource storage
export struct RenderResourceStorage {
    RenderResourceHandle handle;
    RenderResourceDesc desc;
    
    // Actual RHI resources (only one will be valid)
    Core::UniquePtr<RhiTexture> texture;
    Core::UniquePtr<RhiBuffer> buffer;
    RenderTarget* renderTarget = nullptr; // Managed by RenderTargetManager
    
    RenderResourceStorage() = default;
    RenderResourceStorage(RenderResourceStorage&&) = default;
    RenderResourceStorage& operator=(RenderResourceStorage&&) = default;
    
    bool IsValid() const {
        return handle.IsValid() && (texture || buffer || renderTarget);
    }
    
    void* GetNativeHandle() const {
        if (texture) return texture->GetNativeHandle();
        if (buffer) return buffer->GetNativeHandle();
        if (renderTarget) return renderTarget;
        return nullptr;
    }
};

// Hash specialization for RenderResourceHandle
export struct RenderResourceHandleHash {
    std::size_t operator()(const RenderResourceHandle& handle) const {
        return std::hash<uint32_t>{}(handle.id);
    }
};