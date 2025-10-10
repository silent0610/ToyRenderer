module;
#include "vulkan/vulkan.h"

module DeferredPass;
import VkglTFModel;

// DeferredPass implementation
DeferredPass::DeferredPass(RhiDevice* device)
    : device_(device), vkDevice_(VK_NULL_HANDLE), renderPass_(VK_NULL_HANDLE),
      framebuffer_(VK_NULL_HANDLE), pipeline_(VK_NULL_HANDLE), 
      pipelineLayout_(VK_NULL_HANDLE), descriptorSet_(VK_NULL_HANDLE),
      width_(0), height_(0)
{
    // Initialize clear values for G-Buffer
    // From original code: clearValues[0-3] for color, clearValues[4] for depth
    clearValues_[0].color = { {0.0f, 0.0f, 0.0f, 0.0f} };  // Position
    clearValues_[1].color = { {0.0f, 0.0f, 0.0f, 0.0f} };  // Normal
    clearValues_[2].color = { {0.0f, 0.0f, 0.0f, 0.0f} };  // Albedo
    clearValues_[3].color = { {0.0f, 0.0f, 0.0f, 0.0f} };  // Material (Specular/MRAO)
    clearValues_[4].depthStencil = { 1.0f, 0 };             // Depth
}

void DeferredPass::Setup(const SetupData& data)
{
    vkDevice_ = data.device;
    renderPass_ = data.renderPass;
    framebuffer_ = data.framebuffer;
    pipeline_ = data.pipeline;
    pipelineLayout_ = data.pipelineLayout;
    descriptorSet_ = data.descriptorSet;
    width_ = data.width;
    height_ = data.height;
}

void DeferredPass::Execute(RhiCommandBuffer* cmd)
{
    // TODO: Implement RHI-based execution
    // This will be the clean interface for the future
    // For now, this is a placeholder
}

void DeferredPass::ExecuteVulkan(VkCommandBuffer commandBuffer, const vkglTF::Model& model)
{
    // This is the extracted deferred pass logic from RecordMainCommandBuffer()
    // Lines 3297-3319 from the original implementation
    
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass_;
    renderPassBeginInfo.framebuffer = framebuffer_;
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent.width = width_;
    renderPassBeginInfo.renderArea.extent.height = height_;
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues_.size());
    renderPassBeginInfo.pClearValues = clearValues_.data();

    // Begin debug label - "Model Pass" from original code
    // BeginDebugLabel(commandBuffer, "Model Pass", 0.0f, 1.0f, 0.0f);
    
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Set viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width_);
    viewport.height = static_cast<float>(height_);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = {width_, height_};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    // Bind deferred pipeline and descriptor sets
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                           pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
    
    // Draw the model - this generates the G-Buffer
    const_cast<vkglTF::Model&>(model).Draw(commandBuffer, 0, pipelineLayout_, 1);
    
    vkCmdEndRenderPass(commandBuffer);
    
    // End debug label
    // EndDebugLabel(commandBuffer);
}

void DeferredPass::SetDimensions(uint32_t width, uint32_t height)
{
    width_ = width;
    height_ = height;
}