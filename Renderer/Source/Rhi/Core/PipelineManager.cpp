module;
#include <functional>

module PipelineManager;
import Logger;
import std;

// Hash implementations for graphics pipeline descriptors
std::size_t GraphicsPipelineDescHash::operator()(const RhiGraphicsPipelineDesc& desc) const {
    std::size_t hash = 0;
    
    // Hash shaders
    for (const auto& shader : desc.shaders) {
        std::hash<std::string> stringHash;
        hash ^= stringHash(shader.filePath) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= static_cast<std::size_t>(shader.stage) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= stringHash(shader.entryPoint) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    // Hash vertex input state
    for (const auto& binding : desc.vertexInput.bindings) {
        hash ^= binding.binding + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= binding.stride + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= static_cast<std::size_t>(binding.inputRate) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    for (const auto& attribute : desc.vertexInput.attributes) {
        hash ^= attribute.location + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= attribute.binding + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= static_cast<std::size_t>(attribute.format) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= attribute.offset + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    // Hash input assembly
    hash ^= static_cast<std::size_t>(desc.inputAssembly.topology) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= desc.inputAssembly.primitiveRestartEnable ? 1 : 0;
    
    // Hash rasterization state
    hash ^= static_cast<std::size_t>(desc.rasterization.polygonMode) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= static_cast<std::size_t>(desc.rasterization.cullMode) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= static_cast<std::size_t>(desc.rasterization.frontFace) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    
    // Hash depth stencil state
    hash ^= desc.depthStencil.depthTestEnable ? 1 : 0;
    hash ^= desc.depthStencil.depthWriteEnable ? 1 : 0;
    hash ^= static_cast<std::size_t>(desc.depthStencil.depthCompareOp) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= desc.depthStencil.stencilTestEnable ? 1 : 0;
    
    // Hash descriptor set layouts (pointer addresses)
    for (const auto& layout : desc.descriptorSetLayouts) {
        hash ^= reinterpret_cast<std::uintptr_t>(layout) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    return hash;
}


PipelineManager::PipelineManager(RhiDevice* device)
    : device_(device), cacheHits_(0), cacheMisses_(0)
{
    Log::Info("PipelineManager initialized");
}

RhiPipeline* PipelineManager::GetGraphicsPipeline(const RhiGraphicsPipelineDesc& desc) {
    auto it = graphicsPipelineCache_.find(desc);
    
    if (it != graphicsPipelineCache_.end()) {
        ++cacheHits_;
        Log::Debug("Graphics pipeline cache hit");
        return it->second.get();
    }
    
    // Cache miss - create new pipeline
    ++cacheMisses_;
    Log::Debug("Graphics pipeline cache miss - creating new pipeline");
    
    auto pipeline = device_->CreateGraphicsPipeline(desc);
    if (!pipeline) {
        Log::Error("Failed to create graphics pipeline");
        return nullptr;
    }
    
    auto* pipelinePtr = pipeline.get();
    
    // Store in cache
    graphicsPipelineCache_[desc] = std::move(pipeline);
    
    Log::Debug(std::format("Graphics pipeline cached. Total count: {}", graphicsPipelineCache_.size()));
    return pipelinePtr;
}


void PipelineManager::ClearCache() {
    Log::Debug(std::format("PipelineManager::ClearCache() - destroying {} cached pipelines", graphicsPipelineCache_.size()));
    
    // Explicitly destroy each pipeline before clearing
    for (auto& [desc, pipeline] : graphicsPipelineCache_) {
        if (pipeline) {
            Log::Debug("Destroying cached pipeline...");
            pipeline.reset(); // Explicit destruction
        }
    }
    
    graphicsPipelineCache_.clear();
    cacheHits_ = 0;
    cacheMisses_ = 0;
    
    Log::Debug("PipelineManager cache cleared successfully");
}

void PipelineManager::RemoveUnusedPipelines() {
    // TODO: Implement reference counting to track pipeline usage
    // For now, this is a placeholder
    Log::Warn("RemoveUnusedPipelines not yet implemented - requires reference counting");
}

float PipelineManager::GetCacheHitRatio() const {
    size_t total = cacheHits_ + cacheMisses_;
    if (total == 0) {
        return 0.0f;
    }
    return static_cast<float>(cacheHits_) / static_cast<float>(total);
}