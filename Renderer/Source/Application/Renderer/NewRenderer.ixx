module;
#include "vulkan/vulkan.h"

export module NewRenderer;
import Core;
import RhiDevice;
import RhiTypes;
import RenderGraph;
import ForwardPass;
import PipelineManager;
import RenderTargetManager;
import CommandBufferPool;
import PooledCommandBuffer;
import VkglTFModel;
import Camera;
import std;

// NewRenderer - Modern composition-based renderer architecture
// Uses RenderGraph and individual Pass classes instead of monolithic approach
export class NewRenderer 
{
private:
    // Core RHI resources
    Core::UniquePtr<RhiDevice> device_;
    Core::UniquePtr<RenderGraph> renderGraph_;
    
    // Resource managers
    Core::UniquePtr<PipelineManager> pipelineManager_;
    Core::UniquePtr<RenderTargetManager> renderTargetManager_;
    Core::UniquePtr<CommandBufferPool> commandBufferPool_;
    
    // Individual pass instances (composition over inheritance)
    Core::UniquePtr<ForwardPass> forwardPass_;
    
    // Camera system
    Core::UniquePtr<Camera> camera_;
    
    // Vulkan resources for transition period
    VkCommandBuffer commandBuffer_;
    VkDevice vkDevice_;
    
    // Test model
    vkglTF::Model* testModel_;
    
    // Window handle
    void* windowHandle_;

public:
    NewRenderer(void* windowHandle);
    ~NewRenderer();

    // High-level rendering interface
    void Run();         // Main render loop
    void Render();      // Single frame render
    
    // Setup and configuration  
    void Initialize();
    void SetupPasses();
    void Shutdown();
    
    // Model management
    void SetModel(vkglTF::Model* model) { testModel_ = model; }
    
    // Resource management
    RhiDevice* GetDevice() { return device_.get(); }
    RenderGraph* GetRenderGraph() { return renderGraph_.get(); }
    PipelineManager* GetPipelineManager() { return pipelineManager_.get(); }
    RenderTargetManager* GetRenderTargetManager() { return renderTargetManager_.get(); }
    
    // Pass access for configuration
    ForwardPass* GetForwardPass() { return forwardPass_.get(); }
    
    // Camera access
    Camera* GetCamera() { return camera_.get(); }
    
    // Input processing
    void ProcessInput();
    
    // Legacy Vulkan integration (during migration)
    void SetVulkanResources(VkDevice device, VkCommandBuffer cmdBuffer);
};