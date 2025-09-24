module;
#include <vector>
#include <unordered_map>
#include <functional>

export module RenderPassDependency;
import Core;
import RhiCommandBuffer;
import RenderResource;
import PipelineManager;
import RenderTargetManager;
import Logger;
import std;

// Resource access specification for a render pass
export struct RenderResourceAccess {
    RenderResourceHandle resource;
    RenderResourceUsage usage;
    
    // Optional: specific mip level, array layer, etc.
    uint32_t mipLevel = 0;
    uint32_t arrayLayer = 0;
    
    RenderResourceAccess() = default;
    RenderResourceAccess(const RenderResourceHandle& _resource, RenderResourceUsage _usage)
        : resource(_resource), usage(_usage) {}
};

// Modern render pass descriptor with resource dependencies
export struct RenderPassDesc {
    std::string name;
    
    // Resource inputs (what this pass reads)
    std::vector<RenderResourceAccess> inputs;
    
    // Resource outputs (what this pass writes/creates)
    std::vector<RenderResourceAccess> outputs;
    
    // Execution function with resource access
    std::function<void(RhiCommandBuffer*, PipelineManager*, RenderTargetManager*, 
                      const std::unordered_map<RenderResourceHandle, RenderResourceStorage*, RenderResourceHandleHash>&)> executeFunc;
    
    // Optional: render pass can declare resources it needs created
    std::vector<RenderResourceDesc> declaredResources;
    
    RenderPassDesc(const std::string& _name) : name(_name) {}
    
    // Builder pattern methods
    RenderPassDesc& Input(const RenderResourceHandle& resource, RenderResourceUsage usage) {
        inputs.emplace_back(resource, usage);
        return *this;
    }
    
    RenderPassDesc& Output(const RenderResourceHandle& resource, RenderResourceUsage usage) {
        outputs.emplace_back(resource, usage);
        return *this;
    }
    
    RenderPassDesc& DeclareResource(const RenderResourceDesc& desc) {
        declaredResources.push_back(desc);
        return *this;
    }
    
    RenderPassDesc& Execute(std::function<void(RhiCommandBuffer*, PipelineManager*, RenderTargetManager*, 
                                              const std::unordered_map<RenderResourceHandle, RenderResourceStorage*, RenderResourceHandleHash>&)> func) {
        executeFunc = std::move(func);
        return *this;
    }
};

// Resource dependency analysis result
export struct RenderPassDependencyInfo {
    std::vector<uint32_t> dependsOn;     // Pass indices this pass depends on
    std::vector<uint32_t> dependents;    // Pass indices that depend on this pass
    bool canExecuteInParallel = false;   // Future: parallel execution support
};

// Dependency analyzer and scheduler
export class RenderPassScheduler {
private:
    std::vector<RenderPassDesc> passes_;
    std::vector<RenderPassDependencyInfo> dependencies_;
    std::unordered_map<std::string, uint32_t> passNameToIndex_;
    
public:
    RenderPassScheduler() = default;
    
    // Add a render pass to the scheduler
    uint32_t AddPass(RenderPassDesc&& passDesc) {
        uint32_t index = static_cast<uint32_t>(passes_.size());
        passNameToIndex_[passDesc.name] = index;
        
        passes_.emplace_back(std::move(passDesc));
        dependencies_.emplace_back();
        
        Log::Debug(std::format("RenderPassScheduler: Added pass '{}' at index {}", passes_[index].name, index));
        return index;
    }
    
    // Analyze dependencies between all passes
    void AnalyzeDependencies() {
        Log::Info(std::format("RenderPassScheduler: Analyzing dependencies for {} passes", passes_.size()));
        
        // Clear existing dependencies
        for (auto& dep : dependencies_) {
            dep.dependsOn.clear();
            dep.dependents.clear();
        }
        
        // Build resource producer/consumer map
        std::unordered_map<RenderResourceHandle, uint32_t, RenderResourceHandleHash> resourceProducers;
        
        // First pass: identify resource producers
        for (size_t i = 0; i < passes_.size(); ++i) {
            const auto& pass = passes_[i];
            
            for (const auto& output : pass.outputs) {
                resourceProducers[output.resource] = static_cast<uint32_t>(i);
                Log::Debug(std::format("Pass '{}' produces resource '{}'", pass.name, output.resource.name));
            }
        }
        
        // Second pass: identify dependencies
        for (size_t i = 0; i < passes_.size(); ++i) {
            const auto& pass = passes_[i];
            
            for (const auto& input : pass.inputs) {
                auto it = resourceProducers.find(input.resource);
                if (it != resourceProducers.end()) {
                    uint32_t producerIndex = it->second;
                    if (producerIndex != i) { // Don't depend on self
                        dependencies_[i].dependsOn.push_back(producerIndex);
                        dependencies_[producerIndex].dependents.push_back(static_cast<uint32_t>(i));
                        
                        Log::Debug(std::format("Pass '{}' depends on pass '{}' (resource '{}')", 
                                              pass.name, passes_[producerIndex].name, input.resource.name));
                    }
                }
            }
        }
        
        Log::Info("RenderPassScheduler: Dependency analysis complete");
    }
    
    // Get execution order (topological sort)
    std::vector<uint32_t> GetExecutionOrder() const {
        std::vector<uint32_t> result;
        std::vector<bool> visited(passes_.size(), false);
        std::vector<bool> inProgress(passes_.size(), false);
        
        std::function<bool(uint32_t)> visit = [&](uint32_t passIndex) -> bool {
            if (inProgress[passIndex]) {
                Log::Error(std::format("Circular dependency detected involving pass '{}'", passes_[passIndex].name));
                return false; // Circular dependency
            }
            
            if (visited[passIndex]) {
                return true; // Already processed
            }
            
            inProgress[passIndex] = true;
            
            // Visit all dependencies first
            for (uint32_t dependency : dependencies_[passIndex].dependsOn) {
                if (!visit(dependency)) {
                    return false;
                }
            }
            
            inProgress[passIndex] = false;
            visited[passIndex] = true;
            result.push_back(passIndex);
            
            return true;
        };
        
        // Visit all passes
        for (size_t i = 0; i < passes_.size(); ++i) {
            if (!visit(static_cast<uint32_t>(i))) {
                Log::Error("Failed to resolve render pass dependencies");
                return {}; // Return empty on error
            }
        }
        
        Log::Info(std::format("RenderPassScheduler: Execution order resolved for {} passes", result.size()));
        return result;
    }
    
    // Get pass by index
    const RenderPassDesc& GetPass(uint32_t index) const {
        return passes_[index];
    }
    
    const RenderPassDependencyInfo& GetDependencyInfo(uint32_t index) const {
        return dependencies_[index];
    }
    
    size_t GetPassCount() const {
        return passes_.size();
    }
};