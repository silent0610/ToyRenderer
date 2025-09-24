module;
#include "vulkan/vulkan.h"

export module VulkanPipeline;
import RhiPipeline;
import Logger;

// Vulkan implementation of RhiPipeline
export class VulkanPipeline : public RhiPipeline {
private:
    VkPipeline pipeline_;
    VkPipelineLayout pipelineLayout_;
    VkRenderPass renderPass_; // Compatible render pass created for this pipeline
    VkDevice device_;
    
public:
    VulkanPipeline(VkDevice device, VkPipeline pipeline, VkPipelineLayout pipelineLayout);
    VulkanPipeline(VkDevice device, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkRenderPass renderPass);
    ~VulkanPipeline() override;
    
    // RhiPipeline interface
    void* GetNativeHandle() const override { return (void*)pipeline_; }
    
    // Vulkan-specific getters
    VkPipeline GetVkPipeline() const { return pipeline_; }
    VkPipelineLayout GetVkPipelineLayout() const { return pipelineLayout_; }
};