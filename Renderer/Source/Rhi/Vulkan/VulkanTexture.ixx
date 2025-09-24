module;
#include "vulkan/vulkan.h"
#include <cstdint>

export module VulkanTexture;
import RhiTexture;
import Logger;

// Vulkan implementation of RhiTexture
export class VulkanTexture : public RhiTexture {
private:
    VkDevice device_;
    VkImage image_;
    VkDeviceMemory memory_;
    VkImageView imageView_;
    
    RhiTextureDesc desc_;
    
public:
    VulkanTexture(VkDevice device, VkImage image, VkDeviceMemory memory, 
                  VkImageView imageView, const RhiTextureDesc& desc);
    ~VulkanTexture() override;
    
    // RhiTexture interface
    void* GetNativeHandle() const override { return (void*)image_; }
    void* GetNativeImageView() const override { return (void*)imageView_; }
    
    uint32_t GetWidth() const override { return desc_.width; }
    uint32_t GetHeight() const override { return desc_.height; }
    uint32_t GetDepth() const override { return desc_.depth; }
    uint32_t GetMipLevels() const override { return desc_.mipLevels; }
    uint32_t GetArrayLayers() const override { return desc_.arrayLayers; }
    RhiFormat GetFormat() const override { return desc_.format; }
    RhiTextureType GetType() const override { return desc_.type; }
    
    // Vulkan-specific getters
    VkImage GetVkImage() const { return image_; }
    VkDeviceMemory GetVkMemory() const { return memory_; }
    VkImageView GetVkImageView() const { return imageView_; }
};

// Vulkan implementation of RhiSampler
export class VulkanSampler : public RhiSampler {
private:
    VkDevice device_;
    VkSampler sampler_;
    
public:
    VulkanSampler(VkDevice device, VkSampler sampler);
    ~VulkanSampler() override;
    
    // RhiSampler interface
    void* GetNativeHandle() const override { return (void*)sampler_; }
    
    // Vulkan-specific getters
    VkSampler GetVkSampler() const { return sampler_; }
};