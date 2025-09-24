module;
#include "vulkan/vulkan.h"
#include <cstdint>

export module VulkanRenderPass;
import RhiRenderPass;
import RhiTypes;
import Logger;
import std;
import VulkanUtils;

// Vulkan implementation of RhiRenderPass
export class VulkanRenderPass : public RhiRenderPass
{
private:
    VkRenderPass renderPass_;
    std::vector<VkFramebuffer> framebuffers_;
    VkDevice device_;
    uint32_t width_;
    uint32_t height_;
    RhiFormat colorFormat_;
    RhiFormat depthFormat_;

    // Current framebuffer index for rendering
    uint32_t currentFramebuffer_;

public:
    VulkanRenderPass(VkDevice device, uint32_t width, uint32_t height,
                     RhiFormat colorFormat, RhiFormat depthFormat = RhiFormat::D24_UNORM_S8_UINT);
    ~VulkanRenderPass() override;

    // Initialize with swapchain images
    bool Initialize(const std::vector<VkImageView> &colorImageViews, VkImageView depthImageView = VK_NULL_HANDLE);

    // RhiRenderPass interface
    void Begin(uint32_t width, uint32_t height) override;
    void End() override;

    // Properties
    uint32_t GetWidth() const override { return width_; }
    uint32_t GetHeight() const override { return height_; }
    RhiFormat GetColorFormat() const override { return colorFormat_; }
    RhiFormat GetDepthFormat() const override { return depthFormat_; }

    // Vulkan-specific methods
    VkRenderPass GetVkRenderPass() const { return renderPass_; }
    VkFramebuffer GetCurrentFramebuffer() const
    {
        return currentFramebuffer_ < framebuffers_.size() ? framebuffers_[currentFramebuffer_] : VK_NULL_HANDLE;
    }
    void SetCurrentFramebuffer(uint32_t index) { currentFramebuffer_ = index; }
};