module;
#include "vulkan/vulkan.h"
#include <vector>
#include <functional>
#include <string>

export module RenderGraph;
import Core;
import RhiDevice;
import RhiCommandBuffer;
import PipelineManager;
import RenderTargetManager;

// Simplified RenderGraph implementation as per migration plan
// Focuses on sequential execution without complex dependency analysis
export class RenderGraph 
{
public:
    // Modern RHI pass execution callback
    using RhiPassExecuteFunction = std::function<void(RhiCommandBuffer*, PipelineManager*, RenderTargetManager*)>;
    
    // Legacy pass execution callback type
    using PassExecuteFunction = std::function<void()>;
    
    // Vulkan-specific pass execution (for migration period)
    using VulkanPassFunction = std::function<void(VkCommandBuffer)>;

private:
    // Simple sequential list of render tasks
    struct RenderTask {
        Core::String name;
        RhiPassExecuteFunction rhiExecuteFunc;  // Modern RHI execution
        PassExecuteFunction executeFunc;        // Legacy execution
        VulkanPassFunction vulkanFunc;          // For transition period
        bool enabled = true;
    };
    
    std::vector<RenderTask> renderTasks_;
    
    // RHI resources
    RhiDevice* device_;
    PipelineManager* pipelineManager_;
    RenderTargetManager* renderTargetManager_;
    
    // Vulkan resources for transition period
    VkCommandBuffer commandBuffer_;

public:
    RenderGraph(RhiDevice* device, PipelineManager* pipelineManager, RenderTargetManager* renderTargetManager);
    ~RenderGraph() = default;

    // Modern RHI-based pass registration
    void AddRhiPass(const Core::String& name, RhiPassExecuteFunction func);
    
    // Modern RHI-based pass registration with template
    template<typename PassType>
    void AddPass(const Core::String& name, PassType& pass) {
        RenderTask task;
        task.name = name;
        task.executeFunc = [&pass]() { 
            // TODO: Execute using RHI when passes support it
            // pass.Execute(); 
        };
        renderTasks_.push_back(task);
    }
    
    // Legacy Vulkan pass registration (for migration)
    void AddVulkanPass(const Core::String& name, VulkanPassFunction func);
    
    // Execute all passes in sequence with external CommandBuffer
    void Execute(Core::UniquePtr<RhiCommandBuffer> commandBuffer);
    
    // Execute with Vulkan command buffer (during migration)
    void ExecuteVulkan(VkCommandBuffer commandBuffer);
    
    // Pass management
    void EnablePass(const Core::String& name, bool enabled = true);
    void DisablePass(const Core::String& name);
    void Clear();
    
    // Resource access
    PipelineManager* GetPipelineManager() const { return pipelineManager_; }
    RenderTargetManager* GetRenderTargetManager() const { return renderTargetManager_; }
    
    // Debugging/inspection
    size_t GetPassCount() const { return renderTasks_.size(); }
    const Core::String& GetPassName(size_t index) const;
    bool IsPassEnabled(const Core::String& name) const;
};