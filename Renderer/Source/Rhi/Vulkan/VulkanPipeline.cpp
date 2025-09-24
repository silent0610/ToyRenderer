module;
#include "vulkan/vulkan.h"

module VulkanPipeline;
import std;

VulkanPipeline::VulkanPipeline(VkDevice device, VkPipeline pipeline, VkPipelineLayout pipelineLayout)
    : device_(device), pipeline_(pipeline), pipelineLayout_(pipelineLayout), renderPass_(VK_NULL_HANDLE)
{
    Log::Debug("VulkanPipeline created");
}

VulkanPipeline::VulkanPipeline(VkDevice device, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkRenderPass renderPass)
    : device_(device), pipeline_(pipeline), pipelineLayout_(pipelineLayout), renderPass_(renderPass)
{
    Log::Debug("VulkanPipeline created with compatible render pass");
}

VulkanPipeline::~VulkanPipeline() {
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
    }
    
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    }
    
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
    }
    
    Log::Debug("VulkanPipeline destroyed");
}