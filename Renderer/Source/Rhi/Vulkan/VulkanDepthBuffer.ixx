module;
#include "vulkan/vulkan.h"
#include <cstdint>

export module VulkanDepthBuffer;
import Logger;

// RAII-managed depth buffer for Vulkan
export class VulkanDepthBuffer {
private:
    VkDevice device_;
    VkImage image_;
    VkDeviceMemory memory_;
    VkImageView imageView_;
    uint32_t width_;
    uint32_t height_;
    VkFormat format_;
    
    // Helper function to find memory type
    uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
public:
    VulkanDepthBuffer(VkDevice device, VkPhysicalDevice physicalDevice, 
                     uint32_t width, uint32_t height, VkFormat format);
    ~VulkanDepthBuffer();
    
    // Non-copyable but movable
    VulkanDepthBuffer(const VulkanDepthBuffer&) = delete;
    VulkanDepthBuffer& operator=(const VulkanDepthBuffer&) = delete;
    VulkanDepthBuffer(VulkanDepthBuffer&& other) noexcept;
    VulkanDepthBuffer& operator=(VulkanDepthBuffer&& other) noexcept;
    
    // Accessors
    VkImage GetVkImage() const { return image_; }
    VkImageView GetVkImageView() const { return imageView_; }
    VkFormat GetFormat() const { return format_; }
    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    bool IsValid() const { return image_ != VK_NULL_HANDLE && imageView_ != VK_NULL_HANDLE; }
    
private:
    void Cleanup();
};