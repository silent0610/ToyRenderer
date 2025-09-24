module;
#include "vulkan/vulkan.h"

module VulkanRenderPass;
import std;
import VulkanUtils; // Import the utils to get access to the helper

// Constructor and Destructor remain the same...

VulkanRenderPass::VulkanRenderPass(VkDevice device, uint32_t width, uint32_t height,
                                   RhiFormat colorFormat, RhiFormat depthFormat)
    : device_(device), width_(width), height_(height),
      colorFormat_(colorFormat), depthFormat_(depthFormat),
      renderPass_(VK_NULL_HANDLE), currentFramebuffer_(0)
{
    Log::Info("VulkanRenderPass created");
}

VulkanRenderPass::~VulkanRenderPass()
{
    // Clean up framebuffers
    for (auto framebuffer : framebuffers_)
    {
        if (framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }

    // Clean up render pass
    if (renderPass_ != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
    }

    Log::Info("VulkanRenderPass destroyed");
}

bool VulkanRenderPass::Initialize(const std::vector<VkImageView> &colorImageViews, VkImageView depthImageView)
{
    bool hasDepth = (depthImageView != VK_NULL_HANDLE && depthFormat_ != RhiFormat::Undefined);

    // Create render pass attachments
    std::vector<VkAttachmentDescription> attachments;

    // Color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = RhiFormatToVkFormat(colorFormat_);
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments.push_back(colorAttachment);

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment (optional)
    VkAttachmentDescription depthAttachment{};
    VkAttachmentReference depthAttachmentRef{};
    if (hasDepth)
    {
        depthAttachment.format = RhiFormatToVkFormat(depthFormat_); // TODO: Convert from RhiFormat
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depthAttachment);

        depthAttachmentRef.attachment = 1; // Second attachment
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = hasDepth ? &depthAttachmentRef : nullptr;

    // Subpass dependencies
    std::vector<VkSubpassDependency> dependencies;

    // Color attachment dependency
    VkSubpassDependency colorDependency{};
    colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    colorDependency.dstSubpass = 0;
    colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.srcAccessMask = 0;
    colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies.push_back(colorDependency);

    // Depth attachment dependency (if present)
    if (hasDepth)
    {
        VkSubpassDependency depthDependency{};
        depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        depthDependency.dstSubpass = 0;
        depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.srcAccessMask = 0;
        depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies.push_back(depthDependency);
    }

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS)
    {
        Log::Error("Failed to create Vulkan render pass");
        return false;
    }

    // Create framebuffers for each swapchain image
    framebuffers_.resize(colorImageViews.size());
    for (size_t i = 0; i < colorImageViews.size(); i++)
    {
        std::vector<VkImageView> framebufferAttachments;
        framebufferAttachments.push_back(colorImageViews[i]);

        if (hasDepth)
        {
            framebufferAttachments.push_back(depthImageView);
        }

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(framebufferAttachments.size());
        framebufferInfo.pAttachments = framebufferAttachments.data();
        framebufferInfo.width = width_;
        framebufferInfo.height = height_;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS)
        {
            Log::Error(std::format("Failed to create framebuffer {}", i));
            return false;
        }
    }

    Log::Info(std::format("VulkanRenderPass initialized: {}x{}, {} framebuffers",
                          width_, height_, framebuffers_.size()));
    return true;
}

void VulkanRenderPass::Begin(uint32_t width, uint32_t height)
{
    // This will be called by VulkanCommandBuffer
    // The actual render pass begin will be handled there
    Log::Debug(std::format("VulkanRenderPass::Begin ({}x{})", width, height));
}

void VulkanRenderPass::End()
{
    // This will be called by VulkanCommandBuffer
    // The actual render pass end will be handled there
    Log::Debug("VulkanRenderPass::End");
}