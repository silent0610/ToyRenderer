module;
#include "vulkan/vulkan.h"
#include <algorithm>

module RenderGraph;
import Logger;
import std;

// RenderGraph implementation
RenderGraph::RenderGraph(RhiDevice* device, PipelineManager* pipelineManager, RenderTargetManager* renderTargetManager)
    : device_(device), pipelineManager_(pipelineManager), renderTargetManager_(renderTargetManager), commandBuffer_(VK_NULL_HANDLE)
{
    
    Log::Info("RenderGraph initialized with PipelineManager and RenderTargetManager");
}

void RenderGraph::AddRhiPass(const Core::String& name, RhiPassExecuteFunction func)
{
    RenderTask task;
    task.name = name;
    task.rhiExecuteFunc = func;
    task.enabled = true;
    renderTasks_.push_back(task);
    
    Log::Info(std::format("RenderGraph: Added RHI pass: {} (total passes: {})", name.c_str(), renderTasks_.size()));
}

void RenderGraph::AddVulkanPass(const Core::String& name, VulkanPassFunction func)
{
    RenderTask task;
    task.name = name;
    task.vulkanFunc = func;
    task.enabled = true;
    renderTasks_.push_back(task);
    
    Log::Debug(std::format("Added Vulkan pass: {}", name.c_str()));
}

void RenderGraph::Execute(Core::UniquePtr<RhiCommandBuffer> commandBuffer)
{
    // Modern RHI-based execution
    if (!device_) {
        Log::Error("RenderGraph::Execute - No device available");
        return;
    }
    
    if (!commandBuffer) {
        Log::Error("RenderGraph::Execute - Invalid command buffer provided");
        return;
    }
    
    // Begin command buffer
    if (commandBuffer->Begin() != RhiResult::Success) {
        Log::Error("Failed to begin command buffer");
        return;
    }
    
    Log::Debug(std::format("RenderGraph: Starting execution with {} passes", renderTasks_.size()));
    
    // Update swapchain framebuffer index for current frame
    if (renderTargetManager_) {
        renderTargetManager_->UpdateSwapchainFramebufferIndex();
    }
    
    // Execute all enabled passes
    for (size_t i = 0; i < renderTasks_.size(); ++i) {
        const auto& task = renderTasks_[i];
        if (!task.enabled) {
            Log::Debug(std::format("Skipping disabled pass: {}", task.name.c_str()));
            continue;
        }
        
        Log::Debug(std::format("RenderGraph: Executing pass [{}]: {}", i, task.name.c_str()));
        
        // Execute based on pass type
        if (task.rhiExecuteFunc) {
            // Modern RHI execution
            Log::Debug(std::format("RenderGraph: Calling RHI execute function for pass {}", task.name.c_str()));
            task.rhiExecuteFunc(commandBuffer.get(), pipelineManager_, renderTargetManager_);
            Log::Debug(std::format("RenderGraph: Completed execution of pass {}", task.name.c_str()));
        } else if (task.executeFunc) {
            // Legacy execution
            Log::Debug(std::format("RenderGraph: Calling legacy execute function for pass {}", task.name.c_str()));
            task.executeFunc();
        } else {
            Log::Warn(std::format("Pass {} has no valid execution function", task.name.c_str()));
        }
    }
    
    // End command buffer
    if (commandBuffer->End() != RhiResult::Success) {
        Log::Error("Failed to end command buffer");
        return;
    }
    
    // Submit command buffer
    if (device_->Submit(commandBuffer.get()) != RhiResult::Success) {
        Log::Error("Failed to submit command buffer");
        return;
    }
    
    Log::Debug("RenderGraph execution completed successfully");
}

void RenderGraph::ExecuteVulkan(VkCommandBuffer commandBuffer)
{
    // Legacy Vulkan execution during migration period
    commandBuffer_ = commandBuffer;
    
    for (const auto& task : renderTasks_) {
        if (task.enabled && task.vulkanFunc) {
            // Execute the Vulkan pass function
            task.vulkanFunc(commandBuffer);
        }
    }
}

void RenderGraph::EnablePass(const Core::String& name, bool enabled)
{
    auto it = std::find_if(renderTasks_.begin(), renderTasks_.end(),
        [&name](const RenderTask& task) { return task.name == name; });
    
    if (it != renderTasks_.end()) {
        it->enabled = enabled;
    }
}

void RenderGraph::DisablePass(const Core::String& name)
{
    EnablePass(name, false);
}

void RenderGraph::Clear()
{
    renderTasks_.clear();
}

const Core::String& RenderGraph::GetPassName(size_t index) const
{
    static const Core::String emptyString = "";
    if (index < renderTasks_.size()) {
        return renderTasks_[index].name;
    }
    return emptyString;
}

bool RenderGraph::IsPassEnabled(const Core::String& name) const
{
    auto it = std::find_if(renderTasks_.begin(), renderTasks_.end(),
        [&name](const RenderTask& task) { return task.name == name; });
    
    return (it != renderTasks_.end()) ? it->enabled : false;
}