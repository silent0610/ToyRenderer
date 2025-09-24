module;
#include <unordered_map>
#include <memory>
#include <functional>
#include <string>

export module PipelineManager;
import Core;
import RhiDevice;
import RhiPipeline;
import RhiPipelineDesc;

export {
    // Hash function for graphics pipeline descriptors
    struct GraphicsPipelineDescHash {
        std::size_t operator()(const RhiGraphicsPipelineDesc& desc) const;
    };

    // Pipeline cache manager for avoiding duplicate pipeline creation
    class PipelineManager {
    private:
        RhiDevice* device_;
        
        // Graphics pipeline cache
        std::unordered_map<RhiGraphicsPipelineDesc, Core::UniquePtr<RhiPipeline>, GraphicsPipelineDescHash> graphicsPipelineCache_;
        
        // Statistics
        mutable size_t cacheHits_;
        mutable size_t cacheMisses_;
        
    public:
        explicit PipelineManager(RhiDevice* device);
        ~PipelineManager() = default;
        
        // Non-copyable but movable
        PipelineManager(const PipelineManager&) = delete;
        PipelineManager& operator=(const PipelineManager&) = delete;
        PipelineManager(PipelineManager&&) = default;
        PipelineManager& operator=(PipelineManager&&) = default;
        
        // Get or create graphics pipeline
        RhiPipeline* GetGraphicsPipeline(const RhiGraphicsPipelineDesc& desc);
        
        // Cache management
        void ClearCache();
        void RemoveUnusedPipelines(); // TODO: Implement reference counting
        
        // Statistics
        size_t GetCacheHits() const { return cacheHits_; }
        size_t GetCacheMisses() const { return cacheMisses_; }
        size_t GetGraphicsPipelineCount() const { return graphicsPipelineCache_.size(); }
        float GetCacheHitRatio() const;
    };
}