module;
#include "vulkan/vulkan.h"
#include <array>

export module ForwardPass;
import Core;
import Math;
import RhiDevice;
import RhiCommandBuffer;
import RhiRenderPass;
import RhiBuffer;
import RhiDescriptor;
import PipelineManager;
import RenderTargetManager;
import RhiGltfModel;

// MVP transform data for shaders
export struct MVPUniformData {
    alignas(16) Math::Matrix4 model;
    alignas(16) Math::Matrix4 view;  
    alignas(16) Math::Matrix4 proj;
    alignas(16) Math::Matrix4 mvp; // Pre-computed MVP matrix
};

// ForwardPass - Direct forward rendering pass with model support
// Renders glTF models with MVP transformation to swapchain
export class ForwardPass 
{
private:
    // RHI resources
    RhiDevice* device_;
    
    // Model rendering
    Core::UniquePtr<RhiGltfModel> testModel_;
    
    // MVP transformation
    MVPUniformData mvpData_;
    Core::UniquePtr<RhiBuffer> mvpBuffer_;
    Core::UniquePtr<RhiDescriptorSet> descriptorSet_;
    Core::UniquePtr<RhiDescriptorSetLayout> descriptorSetLayout_;
    Core::UniquePtr<RhiDescriptorPool> descriptorPool_;  // Store pool to prevent premature destruction
    
    // Camera parameters
    Math::Vector3 cameraPos_{0.0f, 0.0f, 3.0f};
    Math::Vector3 cameraTarget_{0.0f, 0.0f, 0.0f};
    Math::Vector3 cameraUp_{0.0f, 1.0f, 0.0f};
    
    // Render target dimensions
    uint32_t width_;
    uint32_t height_;
    
    // Clear values for color and depth
    std::array<VkClearValue, 2> clearValues_;

public:
    ForwardPass(RhiDevice* device);
    ~ForwardPass() = default;

    // Initialize pass resources (model, buffers, descriptors)
    bool Initialize();
    
    // Modern RHI execution with model rendering
    void Execute(RhiCommandBuffer* cmd, PipelineManager* pipelineManager, RenderTargetManager* renderTargetManager);
    
    // Resource management
    void SetDimensions(uint32_t width, uint32_t height);
    void SetCameraPosition(const Math::Vector3& pos, const Math::Vector3& target, const Math::Vector3& up = Math::Vector3(0,1,0));
    void SetModelTransform(const Math::Matrix4& transform);
    
    // Update MVP matrices
    void UpdateMVPMatrices();
    
    // Getters
    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    bool HasModel() const { return testModel_ && testModel_->IsValid(); }
    
private:
    // Helper methods
    bool CreateUniformBuffer();
    bool CreateDescriptorSet();
    bool LoadTestModel();
};