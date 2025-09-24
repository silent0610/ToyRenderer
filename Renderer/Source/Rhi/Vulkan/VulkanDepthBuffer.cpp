module;
#include "vulkan/vulkan.h"

module VulkanDepthBuffer;
import Logger;
import std;

uint32_t VulkanDepthBuffer::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    return UINT32_MAX;
}

VulkanDepthBuffer::VulkanDepthBuffer(VkDevice device, VkPhysicalDevice physicalDevice, 
                                   uint32_t width, uint32_t height, VkFormat format)
    : device_(device), image_(VK_NULL_HANDLE), memory_(VK_NULL_HANDLE), imageView_(VK_NULL_HANDLE),
      width_(width), height_(height), format_(format)
{
    // Create depth image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device_, &imageInfo, nullptr, &image_) != VK_SUCCESS) {
        Log::Error("Failed to create depth image");
        return;
    }

    // Allocate memory for depth image
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_, image_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, 
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        Log::Error("Failed to find suitable memory type for depth buffer");
        Cleanup();
        return;
    }

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
        Log::Error("Failed to allocate depth image memory");
        Cleanup();
        return;
    }

    if (vkBindImageMemory(device_, image_, memory_, 0) != VK_SUCCESS) {
        Log::Error("Failed to bind depth image memory");
        Cleanup();
        return;
    }

    // Create depth image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    
    // Handle stencil aspect for combined depth-stencil formats
    if (format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device_, &viewInfo, nullptr, &imageView_) != VK_SUCCESS) {
        Log::Error("Failed to create depth image view");
        Cleanup();
        return;
    }

    Log::Debug(std::format("VulkanDepthBuffer created successfully: {}x{}, format={}", 
                          width, height, static_cast<int>(format)));
}

VulkanDepthBuffer::~VulkanDepthBuffer() {
    Cleanup();
}

VulkanDepthBuffer::VulkanDepthBuffer(VulkanDepthBuffer&& other) noexcept
    : device_(other.device_), image_(other.image_), memory_(other.memory_), imageView_(other.imageView_),
      width_(other.width_), height_(other.height_), format_(other.format_)
{
    // Transfer ownership
    other.image_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.imageView_ = VK_NULL_HANDLE;
    other.width_ = 0;
    other.height_ = 0;
}

VulkanDepthBuffer& VulkanDepthBuffer::operator=(VulkanDepthBuffer&& other) noexcept {
    if (this != &other) {
        // Cleanup current resources
        Cleanup();
        
        // Transfer ownership
        device_ = other.device_;
        image_ = other.image_;
        memory_ = other.memory_;
        imageView_ = other.imageView_;
        width_ = other.width_;
        height_ = other.height_;
        format_ = other.format_;
        
        // Reset other object
        other.image_ = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.imageView_ = VK_NULL_HANDLE;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

void VulkanDepthBuffer::Cleanup() {
    if (imageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, imageView_, nullptr);
        imageView_ = VK_NULL_HANDLE;
    }
    
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }
    
    if (image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, image_, nullptr);
        image_ = VK_NULL_HANDLE;
    }
}