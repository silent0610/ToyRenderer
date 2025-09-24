module;
#include "vulkan/vulkan.h"

module VulkanCommandBuffer;
import VulkanBuffer;
import VulkanPipeline;
import VulkanRenderPass;
import VulkanDescriptor;
import VulkanTexture;
import RhiPipeline;
import RhiRenderPass;
import RhiBarrier;
import std;

VulkanCommandBuffer::VulkanCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer)
    : device_(device), commandBuffer_(commandBuffer), isRecording_(false), currentPipelineLayout_(VK_NULL_HANDLE)
{
    Log::Debug("VulkanCommandBuffer created");
}

RhiResult VulkanCommandBuffer::Begin() {
    if (isRecording_) {
        Log::Warn("CommandBuffer already recording");
        return RhiResult::Success; // Already recording
    }
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    VkResult result = vkBeginCommandBuffer(commandBuffer_, &beginInfo);
    if (result != VK_SUCCESS) {
        Log::Error(std::format("Failed to begin command buffer: {}", static_cast<int>(result)));
        return RhiResult::ErrorInitializationFailed;
    }
    
    isRecording_ = true;
    Log::Debug("Command buffer recording started");
    return RhiResult::Success;
}

RhiResult VulkanCommandBuffer::End() {
    if (!isRecording_) {
        Log::Warn("CommandBuffer not recording");
        return RhiResult::Success;
    }
    
    VkResult result = vkEndCommandBuffer(commandBuffer_);
    if (result != VK_SUCCESS) {
        Log::Error(std::format("Failed to end command buffer: {}", static_cast<int>(result)));
        return RhiResult::ErrorInitializationFailed;
    }
    
    isRecording_ = false;
    Log::Debug("Command buffer recording ended");
    return RhiResult::Success;
}

RhiResult VulkanCommandBuffer::Reset() {
    if (isRecording_) {
        Log::Warn("Attempting to reset command buffer while recording - ending first");
        End();
    }
    
    VkResult result = vkResetCommandBuffer(commandBuffer_, 0);
    if (result != VK_SUCCESS) {
        Log::Error(std::format("Failed to reset command buffer: {}", static_cast<int>(result)));
        return RhiResult::ErrorInitializationFailed;
    }
    
    // Reset tracking state
    isRecording_ = false;
    currentPipelineLayout_ = VK_NULL_HANDLE;
    
    Log::Debug("Command buffer reset successfully");
    return RhiResult::Success;
}

void VulkanCommandBuffer::BeginRenderPass(RhiRenderPass* renderPass) {
    if (!renderPass) {
        Log::Warn("Attempted to begin null render pass");
        return;
    }
    
    // Cast to VulkanRenderPass
    auto* vulkanRenderPass = static_cast<VulkanRenderPass*>(renderPass);
    
    // Setup render pass begin info
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = vulkanRenderPass->GetVkRenderPass();
    renderPassBeginInfo.framebuffer = vulkanRenderPass->GetCurrentFramebuffer();
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent = {vulkanRenderPass->GetWidth(), vulkanRenderPass->GetHeight()};
    
    // Setup clear values (color + depth if present)
    std::vector<VkClearValue> clearValues;
    
    // Color attachment clear value
    VkClearValue colorClear{};
    colorClear.color = {{0.0f, 0.0f, 0.2f, 1.0f}};
    clearValues.push_back(colorClear);
    
    // Depth attachment clear value (if present)
    if (vulkanRenderPass->GetDepthFormat() != RhiFormat::Undefined) {
        VkClearValue depthClear{};
        depthClear.depthStencil = {1.0f, 0};
        clearValues.push_back(depthClear);
    }
    
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues = clearValues.data();
    
    // Begin the render pass
    vkCmdBeginRenderPass(commandBuffer_, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Set default viewport and scissor
    uint32_t width = vulkanRenderPass->GetWidth();
    uint32_t height = vulkanRenderPass->GetHeight();
    
    RhiViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    SetViewport(viewport);
    
    RhiRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {width, height};
    SetScissor(scissor);
    
    Log::Debug(std::format("Render pass begun ({}x{}) with {} clear values", width, height, clearValues.size()));
}

void VulkanCommandBuffer::EndRenderPass() {
    vkCmdEndRenderPass(commandBuffer_);
    Log::Debug("Render pass ended");
}

void VulkanCommandBuffer::BindPipeline(RhiPipeline* pipeline) {
    if (!pipeline) {
        Log::Warn("Attempted to bind null pipeline");
        currentPipelineLayout_ = VK_NULL_HANDLE;
        return;
    }
    
    // Cast to VulkanPipeline to get both pipeline and layout
    auto* vulkanPipeline = static_cast<VulkanPipeline*>(pipeline);
    VkPipeline vkPipeline = vulkanPipeline->GetVkPipeline();
    currentPipelineLayout_ = vulkanPipeline->GetVkPipelineLayout();
    
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
    
    Log::Debug("Graphics pipeline bound with pipeline layout");
}

void VulkanCommandBuffer::BindVertexBuffer(RhiBuffer* buffer, uint32_t binding) {
    if (!buffer) {
        Log::Warn("Attempted to bind null vertex buffer");
        return;
    }
    
    auto vulkanBuffer = static_cast<VulkanBuffer*>(buffer);
    VkBuffer vertexBuffers[] = { vulkanBuffer->GetVkBuffer() };
    VkDeviceSize offsets[] = { 0 };
    
    vkCmdBindVertexBuffers(commandBuffer_, binding, 1, vertexBuffers, offsets);
    
    Log::Debug(std::format("Vertex buffer bound to binding {}", binding));
}

void VulkanCommandBuffer::BindIndexBuffer(RhiBuffer* buffer) {
    if (!buffer) {
        Log::Warn("Attempted to bind null index buffer");
        return;
    }
    
    auto vulkanBuffer = static_cast<VulkanBuffer*>(buffer);
    vkCmdBindIndexBuffer(commandBuffer_, vulkanBuffer->GetVkBuffer(), 0, VK_INDEX_TYPE_UINT32);
    
    Log::Debug("Index buffer bound");
}

void VulkanCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, 
                               uint32_t firstVertex, uint32_t firstInstance) {
    vkCmdDraw(commandBuffer_, vertexCount, instanceCount, firstVertex, firstInstance);
    
    Log::Debug(std::format("Draw: {} vertices, {} instances", vertexCount, instanceCount));
}

void VulkanCommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                      uint32_t firstIndex, int32_t vertexOffset, 
                                      uint32_t firstInstance) {
    vkCmdDrawIndexed(commandBuffer_, indexCount, instanceCount, 
                     firstIndex, vertexOffset, firstInstance);
    
    Log::Debug(std::format("DrawIndexed: {} indices, {} instances", indexCount, instanceCount));
}

void VulkanCommandBuffer::SetViewport(const RhiViewport& viewport) {
    VkViewport vkViewport{};
    vkViewport.x = viewport.x;
    vkViewport.y = viewport.y;
    vkViewport.width = viewport.width;
    vkViewport.height = viewport.height;
    vkViewport.minDepth = viewport.minDepth;
    vkViewport.maxDepth = viewport.maxDepth;
    
    vkCmdSetViewport(commandBuffer_, 0, 1, &vkViewport);
    
    Log::Debug(std::format("Viewport set: {}x{} at ({}, {})", 
        viewport.width, viewport.height, viewport.x, viewport.y));
}

void VulkanCommandBuffer::SetScissor(const RhiRect2D& scissor) {
    VkRect2D vkScissor{};
    vkScissor.offset.x = scissor.offset.x;
    vkScissor.offset.y = scissor.offset.y;
    vkScissor.extent.width = scissor.extent.width;
    vkScissor.extent.height = scissor.extent.height;
    
    vkCmdSetScissor(commandBuffer_, 0, 1, &vkScissor);
    
    Log::Debug(std::format("Scissor set: {}x{} at ({}, {})",
        static_cast<uint32_t>(scissor.extent.width), static_cast<uint32_t>(scissor.extent.height), 
        scissor.offset.x, scissor.offset.y));
}

void VulkanCommandBuffer::BindDescriptorSet(RhiDescriptorSet* descriptorSet, uint32_t setIndex) {
    if (!descriptorSet) {
        Log::Warn("Attempted to bind null descriptor set");
        return;
    }
    
    if (currentPipelineLayout_ == VK_NULL_HANDLE) {
        Log::Error("Cannot bind descriptor set: no pipeline bound or pipeline has null layout");
        return;
    }
    
    auto* vulkanDescriptorSet = dynamic_cast<VulkanDescriptorSet*>(descriptorSet);
    if (!vulkanDescriptorSet) {
        Log::Error("Invalid descriptor set type");
        return;
    }
    VkDescriptorSet vkDescriptorSet = vulkanDescriptorSet->GetVkDescriptorSet();
    
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           currentPipelineLayout_, setIndex, 1, &vkDescriptorSet, 0, nullptr);
    
    Log::Debug(std::format("Descriptor set bound to set index {} with pipeline layout", setIndex));
}

void VulkanCommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    vkCmdDispatch(commandBuffer_, groupCountX, groupCountY, groupCountZ);
    
    Log::Debug(std::format("Dispatched compute: {}x{}x{} groups", groupCountX, groupCountY, groupCountZ));
}

void VulkanCommandBuffer::CopyBuffer(RhiBuffer* srcBuffer, RhiBuffer* dstBuffer, 
                                    uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
    if (!srcBuffer || !dstBuffer) {
        Log::Warn("Attempted to copy with null buffer(s)");
        return;
    }
    
    auto* vulkanSrcBuffer = static_cast<VulkanBuffer*>(srcBuffer);
    auto* vulkanDstBuffer = static_cast<VulkanBuffer*>(dstBuffer);
    
    // If size is 0, copy the entire source buffer
    if (size == 0) {
        size = vulkanSrcBuffer->GetSize();
    }
    
    // Ensure copy doesn't exceed buffer boundaries
    if (srcOffset + size > vulkanSrcBuffer->GetSize() || 
        dstOffset + size > vulkanDstBuffer->GetSize()) {
        Log::Error("Buffer copy would exceed buffer boundaries");
        return;
    }
    
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;
    
    vkCmdCopyBuffer(commandBuffer_, vulkanSrcBuffer->GetVkBuffer(), 
                    vulkanDstBuffer->GetVkBuffer(), 1, &copyRegion);
    
    Log::Debug(std::format("Buffer copied: {} bytes from offset {} to offset {}", 
               size, srcOffset, dstOffset));
}

void VulkanCommandBuffer::CopyImage(RhiTexture* srcTexture, RhiTexture* dstTexture,
                                   uint32_t srcMipLevel, uint32_t dstMipLevel,
                                   uint32_t srcArrayLayer, uint32_t dstArrayLayer) {
    if (!srcTexture || !dstTexture) {
        Log::Warn("Attempted to copy with null texture(s)");
        return;
    }
    
    auto* vulkanSrcTexture = static_cast<VulkanTexture*>(srcTexture);
    auto* vulkanDstTexture = static_cast<VulkanTexture*>(dstTexture);
    
    VkImageCopy copyRegion{};
    
    // Source subresource
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.mipLevel = srcMipLevel;
    copyRegion.srcSubresource.baseArrayLayer = srcArrayLayer;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.srcOffset = {0, 0, 0};
    
    // Destination subresource
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.mipLevel = dstMipLevel;
    copyRegion.dstSubresource.baseArrayLayer = dstArrayLayer;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.dstOffset = {0, 0, 0};
    
    // Copy extent (use smaller dimensions for safety)
    uint32_t srcWidth = std::max(1u, srcTexture->GetWidth() >> srcMipLevel);
    uint32_t srcHeight = std::max(1u, srcTexture->GetHeight() >> srcMipLevel);
    uint32_t dstWidth = std::max(1u, dstTexture->GetWidth() >> dstMipLevel);
    uint32_t dstHeight = std::max(1u, dstTexture->GetHeight() >> dstMipLevel);
    
    copyRegion.extent.width = std::min(srcWidth, dstWidth);
    copyRegion.extent.height = std::min(srcHeight, dstHeight);
    copyRegion.extent.depth = std::min(srcTexture->GetDepth(), dstTexture->GetDepth());
    
    vkCmdCopyImage(commandBuffer_, 
                   vulkanSrcTexture->GetVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   vulkanDstTexture->GetVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &copyRegion);
    
    Log::Debug(std::format("Image copied: mip {} to mip {}, layer {} to layer {}", 
               srcMipLevel, dstMipLevel, srcArrayLayer, dstArrayLayer));
}

void VulkanCommandBuffer::CopyBufferToImage(RhiBuffer* srcBuffer, RhiTexture* dstTexture,
                                           uint32_t bufferOffset, uint32_t mipLevel, uint32_t arrayLayer) {
    if (!srcBuffer || !dstTexture) {
        Log::Warn("Attempted to copy with null buffer or texture");
        return;
    }
    
    auto* vulkanBuffer = static_cast<VulkanBuffer*>(srcBuffer);
    auto* vulkanTexture = static_cast<VulkanTexture*>(dstTexture);
    
    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = bufferOffset;
    copyRegion.bufferRowLength = 0;   // Tightly packed
    copyRegion.bufferImageHeight = 0; // Tightly packed
    
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = mipLevel;
    copyRegion.imageSubresource.baseArrayLayer = arrayLayer;
    copyRegion.imageSubresource.layerCount = 1;
    
    copyRegion.imageOffset = {0, 0, 0};
    
    // Calculate extent for specific mip level
    uint32_t mipWidth = std::max(1u, dstTexture->GetWidth() >> mipLevel);
    uint32_t mipHeight = std::max(1u, dstTexture->GetHeight() >> mipLevel);
    uint32_t mipDepth = std::max(1u, dstTexture->GetDepth() >> mipLevel);
    
    copyRegion.imageExtent = {mipWidth, mipHeight, mipDepth};
    
    vkCmdCopyBufferToImage(commandBuffer_,
                          vulkanBuffer->GetVkBuffer(),
                          vulkanTexture->GetVkImage(),
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          1, &copyRegion);
    
    Log::Debug(std::format("Buffer to image copied: buffer offset {} to mip {} layer {}", 
               bufferOffset, mipLevel, arrayLayer));
}

void VulkanCommandBuffer::PipelineBarrier(const RhiBarrierDesc& barrierDesc) {
    // Convert RHI pipeline stages to Vulkan flags
    auto convertPipelineStage = [](RhiPipelineStage stage) -> VkPipelineStageFlags {
        VkPipelineStageFlags vkStage = 0;
        if (static_cast<uint32_t>(stage) & static_cast<uint32_t>(RhiPipelineStage::TopOfPipe))
            vkStage |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if (static_cast<uint32_t>(stage) & static_cast<uint32_t>(RhiPipelineStage::VertexInput))
            vkStage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        if (static_cast<uint32_t>(stage) & static_cast<uint32_t>(RhiPipelineStage::VertexShader))
            vkStage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        if (static_cast<uint32_t>(stage) & static_cast<uint32_t>(RhiPipelineStage::FragmentShader))
            vkStage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        if (static_cast<uint32_t>(stage) & static_cast<uint32_t>(RhiPipelineStage::EarlyFragmentTests))
            vkStage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        if (static_cast<uint32_t>(stage) & static_cast<uint32_t>(RhiPipelineStage::LateFragmentTests))
            vkStage |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        if (static_cast<uint32_t>(stage) & static_cast<uint32_t>(RhiPipelineStage::ColorAttachmentOutput))
            vkStage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        if (static_cast<uint32_t>(stage) & static_cast<uint32_t>(RhiPipelineStage::ComputeShader))
            vkStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        if (static_cast<uint32_t>(stage) & static_cast<uint32_t>(RhiPipelineStage::Transfer))
            vkStage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        if (static_cast<uint32_t>(stage) & static_cast<uint32_t>(RhiPipelineStage::BottomOfPipe))
            vkStage |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        if (static_cast<uint32_t>(stage) & static_cast<uint32_t>(RhiPipelineStage::AllCommands))
            vkStage |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        return vkStage;
    };
    
    // Convert RHI access flags to Vulkan flags
    auto convertAccessFlags = [](RhiAccessFlags access) -> VkAccessFlags {
        VkAccessFlags vkAccess = 0;
        if (static_cast<uint32_t>(access) & static_cast<uint32_t>(RhiAccessFlags::MemoryRead))
            vkAccess |= VK_ACCESS_MEMORY_READ_BIT;
        if (static_cast<uint32_t>(access) & static_cast<uint32_t>(RhiAccessFlags::MemoryWrite))
            vkAccess |= VK_ACCESS_MEMORY_WRITE_BIT;
        if (static_cast<uint32_t>(access) & static_cast<uint32_t>(RhiAccessFlags::ColorAttachmentRead))
            vkAccess |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        if (static_cast<uint32_t>(access) & static_cast<uint32_t>(RhiAccessFlags::ColorAttachmentWrite))
            vkAccess |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if (static_cast<uint32_t>(access) & static_cast<uint32_t>(RhiAccessFlags::DepthStencilAttachmentRead))
            vkAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        if (static_cast<uint32_t>(access) & static_cast<uint32_t>(RhiAccessFlags::DepthStencilAttachmentWrite))
            vkAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if (static_cast<uint32_t>(access) & static_cast<uint32_t>(RhiAccessFlags::TransferRead))
            vkAccess |= VK_ACCESS_TRANSFER_READ_BIT;
        if (static_cast<uint32_t>(access) & static_cast<uint32_t>(RhiAccessFlags::TransferWrite))
            vkAccess |= VK_ACCESS_TRANSFER_WRITE_BIT;
        if (static_cast<uint32_t>(access) & static_cast<uint32_t>(RhiAccessFlags::ShaderRead))
            vkAccess |= VK_ACCESS_SHADER_READ_BIT;
        if (static_cast<uint32_t>(access) & static_cast<uint32_t>(RhiAccessFlags::ShaderWrite))
            vkAccess |= VK_ACCESS_SHADER_WRITE_BIT;
        return vkAccess;
    };
    
    // Convert memory barriers
    std::vector<VkMemoryBarrier> vkMemoryBarriers;
    for (const auto& barrier : barrierDesc.memoryBarriers) {
        VkMemoryBarrier vkBarrier{};
        vkBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        vkBarrier.srcAccessMask = convertAccessFlags(barrier.srcAccessMask);
        vkBarrier.dstAccessMask = convertAccessFlags(barrier.dstAccessMask);
        vkMemoryBarriers.push_back(vkBarrier);
    }
    
    // Convert buffer memory barriers
    std::vector<VkBufferMemoryBarrier> vkBufferBarriers;
    for (const auto& barrier : barrierDesc.bufferMemoryBarriers) {
        if (barrier.buffer) {
            auto* rhiBuffer = static_cast<RhiBuffer*>(barrier.buffer);
            auto* vulkanBuffer = static_cast<VulkanBuffer*>(rhiBuffer);
            
            VkBufferMemoryBarrier vkBarrier{};
            vkBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            vkBarrier.srcAccessMask = convertAccessFlags(barrier.srcAccessMask);
            vkBarrier.dstAccessMask = convertAccessFlags(barrier.dstAccessMask);
            vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkBarrier.buffer = vulkanBuffer->GetVkBuffer();
            vkBarrier.offset = barrier.offset;
            vkBarrier.size = barrier.size == 0 ? VK_WHOLE_SIZE : barrier.size;
            vkBufferBarriers.push_back(vkBarrier);
        }
    }
    
    VkPipelineStageFlags srcStage = convertPipelineStage(barrierDesc.srcStage);
    VkPipelineStageFlags dstStage = convertPipelineStage(barrierDesc.dstStage);
    
    vkCmdPipelineBarrier(
        commandBuffer_,
        srcStage, dstStage,
        0, // dependency flags
        static_cast<uint32_t>(vkMemoryBarriers.size()), vkMemoryBarriers.data(),
        static_cast<uint32_t>(vkBufferBarriers.size()), vkBufferBarriers.data(),
        0, nullptr // no image barriers for now
    );
    
    Log::Debug(std::format("Pipeline barrier: {} memory barriers, {} buffer barriers", 
               vkMemoryBarriers.size(), vkBufferBarriers.size()));
}