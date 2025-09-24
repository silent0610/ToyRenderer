module;
#include "vulkan/vulkan.h"

module VulkanTexture;
import Logger;
import std;

VulkanTexture::VulkanTexture(VkDevice device, VkImage image, VkDeviceMemory memory, 
                             VkImageView imageView, const RhiTextureDesc& desc)
    : device_(device), image_(image), memory_(memory), imageView_(imageView), desc_(desc)
{
    Log::Debug(std::format("VulkanTexture created: {}x{}x{}, format={}", 
                          desc_.width, desc_.height, desc_.depth, static_cast<int>(desc_.format)));
}

VulkanTexture::~VulkanTexture() {
    if (imageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, imageView_, nullptr);
    }
    
    if (image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, image_, nullptr);
    }
    
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, nullptr);
    }
    
    Log::Debug("VulkanTexture destroyed");
}

VulkanSampler::VulkanSampler(VkDevice device, VkSampler sampler)
    : device_(device), sampler_(sampler)
{
    Log::Debug("VulkanSampler created");
}

VulkanSampler::~VulkanSampler() {
    if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, sampler_, nullptr);
    }
    
    Log::Debug("VulkanSampler destroyed");
}