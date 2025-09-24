module;
#include "vulkan/vulkan.h"
#include <array>

export module DeferredPass;
import Core;
import Math;
import RhiDevice;
import RhiCommandBuffer;
import VkglTFModel;  // Import actual Model class instead of forward declaration

// DeferredPass - G-Buffer generation for deferred rendering
// Extracted from the monolithic Renderer::BuildDeferredCommandBuffer()
export class DeferredPass 
{
private:
    // RHI resources
    RhiDevice* device_;
    
    // Vulkan resources for the transition period
    VkDevice vkDevice_;
    VkRenderPass renderPass_;
    VkFramebuffer framebuffer_;
    VkPipeline pipeline_;
    VkPipelineLayout pipelineLayout_;
    VkDescriptorSet descriptorSet_;
    
    // G-Buffer dimensions
    uint32_t width_;
    uint32_t height_;
    
    // Clear values for G-Buffer attachments
    std::array<VkClearValue, 5> clearValues_;

public:
    struct SetupData {
        VkDevice device;
        VkRenderPass renderPass;
        VkFramebuffer framebuffer;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSet descriptorSet;
        uint32_t width;
        uint32_t height;
    };

    DeferredPass(RhiDevice* device);
    ~DeferredPass() = default;

    // Setup with existing Vulkan resources (for migration period)
    void Setup(const SetupData& data);
    
    // Execute the G-Buffer generation pass
    void Execute(RhiCommandBuffer* cmd);
    
    // Legacy Vulkan execution (during migration)
    void ExecuteVulkan(VkCommandBuffer commandBuffer, const vkglTF::Model& model);

    // Resource management
    void SetDimensions(uint32_t width, uint32_t height);
    
    // Getters for debugging/validation
    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
};