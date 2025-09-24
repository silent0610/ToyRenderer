module;
#include "vulkan/vulkan.h"
#include "GLFW/glfw3.h"

module NewRenderer;
import VulkanFactory;
import RhiFactory;
import Logger;
import Math;
import std;

NewRenderer::NewRenderer(void *windowHandle)
    : windowHandle_(windowHandle), commandBuffer_(VK_NULL_HANDLE),
      vkDevice_(VK_NULL_HANDLE), testModel_(nullptr)
{
    Log::Info("[NewRenderer] Initializing modern renderer architecture...");

    // Initialize GLFW if no window handle provided
    if (!windowHandle_)
    {
        Log::Info("[NewRenderer] Creating GLFW window for testing...");
        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        GLFWwindow *window = glfwCreateWindow(800, 600, "MyToyRenderer - Camera Controls: WASD + Mouse", nullptr, nullptr);
        if (!window)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        // Setup mouse input
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        
        windowHandle_ = window;
        Log::Info("[NewRenderer] GLFW window created successfully!");
    }
    
    // Initialize camera
    camera_ = Core::MakeUnique<Camera>();
    camera_->SetPosition(Math::Vector3(0.0f, 0.0f, 3.0f));
    
    // Setup mouse callback for camera control
    if (windowHandle_) {
        GLFWwindow* window = static_cast<GLFWwindow*>(windowHandle_);
        glfwSetWindowUserPointer(window, this);
        glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
            NewRenderer* renderer = static_cast<NewRenderer*>(glfwGetWindowUserPointer(window));
            if (renderer && renderer->camera_) {
                renderer->camera_->MouseCallback(window, xpos, ypos);
            }
        });
    }
    
    Log::Info("[NewRenderer] Camera initialized");
}

void NewRenderer::Initialize()
{
    Log::Info("[NewRenderer] Creating RHI Device...");

    // Create RHI device using our factory
    device_ = Rhi::CreateVulkanDevice(static_cast<GLFWwindow *>(windowHandle_));
    if (!device_)
    {
        Log::Error("[NewRenderer] Failed to create RHI Device!");
        return;
    }
    Log::Info("[NewRenderer] RHI Device created successfully!");

    // Create resource managers
    pipelineManager_ = Core::MakeUnique<PipelineManager>(device_.get());
    Log::Info("[NewRenderer] PipelineManager created");
    
    renderTargetManager_ = Core::MakeUnique<RenderTargetManager>(device_.get());
    Log::Info("[NewRenderer] RenderTargetManager created");

    // Create CommandBufferPool for efficient buffer reuse
    commandBufferPool_ = Core::MakeUnique<CommandBufferPool>(device_.get());
    commandBufferPool_->Preallocate(3); // Preallocate for triple buffering
    Log::Info("[NewRenderer] CommandBufferPool created and preallocated");

    // Create RenderGraph with managers
    renderGraph_ = Core::MakeUnique<RenderGraph>(device_.get(), pipelineManager_.get(), renderTargetManager_.get());
    Log::Info("[NewRenderer] RenderGraph created");

    // Create ForwardPass
    forwardPass_ = Core::MakeUnique<ForwardPass>(device_.get());
    Log::Info("[NewRenderer] ForwardPass created");

    // Setup passes
    SetupPasses();
}

NewRenderer::~NewRenderer()
{
    // Check if cleanup is needed (if Shutdown() wasn't called)
    if (device_ || renderGraph_ || forwardPass_ || renderTargetManager_ || pipelineManager_) {
        Log::Info("[NewRenderer] Destructor cleanup (Shutdown() not called)");
        
        // Wait for device to be idle before cleanup
        if (device_) {
            Log::Debug("[NewRenderer] Waiting for device idle...");
            device_->WaitIdle();
            Log::Debug("[NewRenderer] Device is now idle");
        }
        
        // Cleanup in reverse dependency order:
        // 1. Clear render graph (releases command buffer references)
        if (renderGraph_) {
            Log::Debug("[NewRenderer] Clearing RenderGraph...");
            renderGraph_.reset();
            Log::Debug("[NewRenderer] RenderGraph cleared");
        }
        
        // 2. Clear passes (releases pipeline and render target references)
        if (forwardPass_) {
            Log::Debug("[NewRenderer] Clearing ForwardPass...");
            forwardPass_.reset();
            Log::Debug("[NewRenderer] ForwardPass cleared");
        }
        
        // 3. Clear managers - explicitly clear caches while device is still valid
        if (renderTargetManager_) {
            Log::Debug("[NewRenderer] Clearing RenderTargetManager cache...");
            renderTargetManager_->ClearCache();
            Log::Debug("[NewRenderer] RenderTargetManager cache cleared, destroying manager...");
            renderTargetManager_.reset();
            Log::Debug("[NewRenderer] RenderTargetManager destroyed");
        }
        
        if (pipelineManager_) {
            Log::Debug("[NewRenderer] Clearing PipelineManager cache...");
            pipelineManager_->ClearCache();
            Log::Debug("[NewRenderer] PipelineManager cache cleared, destroying manager...");
            pipelineManager_.reset();
            Log::Debug("[NewRenderer] PipelineManager destroyed");
        }
        
        // 4. Finally clear device (this should destroy all remaining Vulkan objects)
        if (device_) {
            Log::Debug("[NewRenderer] Destroying RHI device...");
            device_.reset();
            Log::Debug("[NewRenderer] RHI device destroyed");
        }
        
        Log::Info("[NewRenderer] Destructor cleanup complete");
    } else {
        Log::Debug("[NewRenderer] Destructor: cleanup already done by Shutdown()");
    }
}

void NewRenderer::SetupPasses()
{
    Log::Info("[NewRenderer] SetupPasses() called");
    
    if (!renderGraph_)
    {
        Log::Error("[NewRenderer] RenderGraph not initialized!");
        return;
    }
    
    Log::Info("[NewRenderer] RenderGraph is valid, proceeding to initialize and add ForwardPass");
    
    // Initialize ForwardPass (creates MVP buffers, descriptors, loads model)
    if (forwardPass_) {
        if (!forwardPass_->Initialize()) {
            Log::Error("[NewRenderer] Failed to initialize ForwardPass!");
            return;
        }
        Log::Info("[NewRenderer] ForwardPass initialized successfully");
    }

    // Add forward rendering pass to render graph using modern RHI method
    renderGraph_->AddRhiPass("ForwardPass", [this](RhiCommandBuffer* cmd, PipelineManager* pipelineManager, RenderTargetManager* renderTargetManager)
                            {
        if (forwardPass_) {
            // For testing triangle rendering, we don't need a model
            // The triangle vertices are defined in the vertex shader itself
            Log::Debug("[NewRenderer] Executing ForwardPass");
            forwardPass_->Execute(cmd, pipelineManager, renderTargetManager);
        } else {
            Log::Warn("[NewRenderer] ForwardPass not available for execution");
        } });

    Log::Info("[NewRenderer] Passes setup complete");
}

void NewRenderer::SetVulkanResources(VkDevice device, VkCommandBuffer cmdBuffer)
{
    vkDevice_ = device;
    commandBuffer_ = cmdBuffer;

    Log::Info("[NewRenderer] Vulkan resources set");
}

void NewRenderer::Run()
{
    Log::Info("[NewRenderer] Starting main render loop...");
    Log::Info(std::string("[NewRenderer] - RHI Device: ") + (device_ ? "Available" : "Not Available"));
    Log::Info(std::string("[NewRenderer] - RenderGraph: ") + (renderGraph_ ? "Available" : "Not Available"));
    Log::Info(std::string("[NewRenderer] - ForwardPass: ") + (forwardPass_ ? "Available" : "Not Available"));

    if (!windowHandle_)
    {
        Log::Warn("[NewRenderer] No window available for render loop");
        return;
    }

    GLFWwindow *window = static_cast<GLFWwindow *>(windowHandle_);

    // Main render loop
    Log::Info("[NewRenderer] Running render loop...");
    int frameCount = 0;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        
        // Process input for camera controls
        ProcessInput();

        // Test RHI rendering via RenderGraph
        if (device_ && renderGraph_)
        {
            // Step 1: Acquire the next swapchain image (signals imageAvailable semaphore)
            auto acquireResult = device_->AcquireNextImage();
            if (acquireResult == RhiResult::Success)
            {
                // Step 2: Acquire reusable command buffer from pool (RAII managed)
                auto cmdBuffer = commandBufferPool_->AcquirePooledBuffer();
                if (cmdBuffer.IsValid())
                {
                    // Update camera and pass camera data to forward pass
                    if (forwardPass_ && camera_) {
                        forwardPass_->SetDimensions(800, 600); // Default window size
                        
                        // Update camera position and view
                        Math::Vector3 cameraPos = camera_->GetPosition();
                        Math::Vector3 cameraTarget = Math::Add(cameraPos, camera_->GetFront());
                        Math::Vector3 cameraUp = camera_->GetUp();
                        forwardPass_->SetCameraPosition(cameraPos, cameraTarget, cameraUp);
                    }
                    
                    // Step 3: Execute all passes through RenderGraph (submits CommandBuffer)
                    // This will submit the command buffer which waits on imageAvailable semaphore
                    // and signals renderFinished semaphore
                    renderGraph_->Execute(cmdBuffer.Release()); // Transfer ownership to RenderGraph
                    
                    // Step 4: Present the rendered image (waits on renderFinished semaphore)
                    device_->Present();
                    
                    // Step 5: CommandBuffer will be automatically returned to pool when RenderGraph is done
                    // (via RAII PooledCommandBuffer destructor)
                }
            }
        }

        frameCount++;
        if (frameCount % 50 == 0)
        {
            Log::Debug("[NewRenderer] Frame " + std::to_string(frameCount));
        }
    }

    Log::Info("[NewRenderer] Render loop completed after " + std::to_string(frameCount) + " frames");
}

void NewRenderer::Render()
{
    if (!renderGraph_ || commandBuffer_ == VK_NULL_HANDLE)
    {
        Log::Error("[NewRenderer] Not properly initialized for rendering!");
        return;
    }

    if (!testModel_)
    {
        Log::Warn("[NewRenderer] No model set for rendering!");
        return;
    }

    // Execute the render graph
    renderGraph_->ExecuteVulkan(commandBuffer_);
}

void NewRenderer::ProcessInput()
{
    if (!windowHandle_ || !camera_) {
        return;
    }
    
    GLFWwindow* window = static_cast<GLFWwindow*>(windowHandle_);
    
    // Process keyboard input for camera movement
    camera_->ProcessKeyboard(window);
    
    // ESC to close window
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    
    // Toggle wireframe mode with T key
    static bool wireframePressed = false;
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS && !wireframePressed) {
        wireframePressed = true;
        // TODO: Toggle wireframe mode in pipeline
        Log::Info("Wireframe mode toggle requested");
    }
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_RELEASE) {
        wireframePressed = false;
    }
}

void NewRenderer::Shutdown()
{
    Log::Info("[NewRenderer] Shutting down...");

    // Wait for device to be idle before cleanup
    if (device_) {
        Log::Debug("[NewRenderer] Waiting for device idle...");
        device_->WaitIdle();
        Log::Debug("[NewRenderer] Device is now idle");
    }
    
    // Cleanup in reverse dependency order:
    // 1. Clear render graph (releases command buffer references)
    if (renderGraph_) {
        Log::Debug("[NewRenderer] Clearing RenderGraph...");
        renderGraph_.reset();
        Log::Debug("[NewRenderer] RenderGraph cleared");
    }
    
    // 2. Clear passes (releases pipeline and render target references)
    if (forwardPass_) {
        Log::Debug("[NewRenderer] Clearing ForwardPass...");
        forwardPass_.reset();
        Log::Debug("[NewRenderer] ForwardPass cleared");
    }
    
    // 3. Clear managers - explicitly clear caches while device is still valid
    if (commandBufferPool_) {
        Log::Debug("[NewRenderer] Clearing CommandBufferPool...");
        commandBufferPool_->LogStatistics(); // Log pool usage stats
        commandBufferPool_->Clear();
        commandBufferPool_.reset();
        Log::Debug("[NewRenderer] CommandBufferPool destroyed");
    }
    
    if (renderTargetManager_) {
        Log::Debug("[NewRenderer] Clearing RenderTargetManager cache...");
        renderTargetManager_->ClearCache();
        Log::Debug("[NewRenderer] RenderTargetManager cache cleared, destroying manager...");
        renderTargetManager_.reset();
        Log::Debug("[NewRenderer] RenderTargetManager destroyed");
    }
    
    if (pipelineManager_) {
        Log::Debug("[NewRenderer] Clearing PipelineManager cache...");
        pipelineManager_->ClearCache();
        Log::Debug("[NewRenderer] PipelineManager cache cleared, destroying manager...");
        pipelineManager_.reset();
        Log::Debug("[NewRenderer] PipelineManager destroyed");
    }
    
    // 4. Finally clear device (this should destroy all remaining Vulkan objects)
    if (device_) {
        Log::Debug("[NewRenderer] Destroying RHI device...");
        device_.reset();
        Log::Debug("[NewRenderer] RHI device destroyed");
    }

    // Cleanup GLFW if we created the window
    if (windowHandle_)
    {
        GLFWwindow *window = static_cast<GLFWwindow *>(windowHandle_);
        glfwDestroyWindow(window);
        glfwTerminate();
        Log::Info("[NewRenderer] GLFW cleaned up");
    }

    Log::Info("[NewRenderer] Shutdown complete");
}