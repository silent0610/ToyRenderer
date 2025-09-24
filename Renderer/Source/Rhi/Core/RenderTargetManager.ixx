module;
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

export module RenderTargetManager;
import Core;
import RhiDevice;
import RhiTexture;
import RhiRenderPass;
import RhiTypes;
import RhiRenderPassDesc;

export {
    // Render target descriptor for requesting render targets
    struct RenderTargetDesc {
        uint32_t width = 0;
        uint32_t height = 0;
        RhiFormat colorFormat = RhiFormat::R8G8B8A8_UNORM;
        RhiFormat depthFormat = RhiFormat::Undefined; // Optional depth
        bool isSwapchain = false; // True for main screen rendering
        std::string debugName;
        
        // Equality and hash support
        bool operator==(const RenderTargetDesc& other) const;
    };
    
    // Hash function for render target descriptors
    struct RenderTargetDescHash {
        std::size_t operator()(const RenderTargetDesc& desc) const;
    };
    
    // Render target resource bundle
    struct RenderTarget {
        Core::UniquePtr<RhiTexture> colorTexture;
        Core::UniquePtr<RhiTexture> depthTexture; // Optional
        Core::UniquePtr<RhiRenderPass> renderPass;
        RenderTargetDesc desc;
        
        RenderTarget() = default;
        RenderTarget(RenderTarget&&) = default;
        RenderTarget& operator=(RenderTarget&&) = default;
        
        bool IsValid() const {
            // For swapchain render targets, colorTexture is managed by swapchain itself
            bool hasValidColor = desc.isSwapchain || (colorTexture != nullptr);
            bool hasValidDepth = (desc.depthFormat == RhiFormat::Undefined || depthTexture != nullptr);
            return hasValidColor && renderPass && hasValidDepth;
        }
    };
    
    // Manager for creating and caching render targets
    class RenderTargetManager {
    private:
        RhiDevice* device_;
        
        // Cache for offscreen render targets
        std::unordered_map<RenderTargetDesc, std::unique_ptr<RenderTarget>, RenderTargetDescHash> renderTargetCache_;
        
        // Swapchain render target (special case)
        std::unique_ptr<RenderTarget> swapchainRenderTarget_;
        
        // Statistics
        mutable size_t cacheHits_;
        mutable size_t cacheMisses_;
        
    public:
        explicit RenderTargetManager(RhiDevice* device);
        ~RenderTargetManager() = default;
        
        // Non-copyable but movable
        RenderTargetManager(const RenderTargetManager&) = delete;
        RenderTargetManager& operator=(const RenderTargetManager&) = delete;
        RenderTargetManager(RenderTargetManager&&) = default;
        RenderTargetManager& operator=(RenderTargetManager&&) = default;
        
        // Get or create render target
        RenderTarget* GetRenderTarget(const RenderTargetDesc& desc);
        
        // Get swapchain render target (for main screen rendering)
        RenderTarget* GetSwapchainRenderTarget();
        
        // Update swapchain render target when window is resized
        void InvalidateSwapchainRenderTarget();
        
        // Update swapchain framebuffer index for current frame
        void UpdateSwapchainFramebufferIndex();
        
        // Cache management
        void ClearCache();
        void RemoveUnusedRenderTargets(); // TODO: Implement reference counting
        
        // Statistics
        size_t GetCacheHits() const { return cacheHits_; }
        size_t GetCacheMisses() const { return cacheMisses_; }
        size_t GetRenderTargetCount() const { return renderTargetCache_.size(); }
        float GetCacheHitRatio() const;
        
    private:
        std::unique_ptr<RenderTarget> CreateRenderTarget(const RenderTargetDesc& desc);
        std::unique_ptr<RenderTarget> CreateSwapchainRenderTarget();
    };
}