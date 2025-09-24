module;
#include "vulkan/vulkan.h"
#include <cstdint>

export module VulkanBuffer;
import RhiBuffer;
import RhiTypes;
import Logger;

// Vulkan implementation of RhiBuffer
export class VulkanBuffer : public RhiBuffer {
private:
    VkBuffer buffer_;
    VkDeviceMemory memory_;
    VkDevice device_;
    size_t size_;
    RhiBufferUsage usage_;
    void* mappedData_;
    
public:
    VulkanBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, 
                 size_t size, RhiBufferUsage usage);
    ~VulkanBuffer() override;
    
    // RhiBuffer interface
    void* Map() override;
    void Unmap() override;
    void UpdateData(const void* data, size_t size, size_t offset = 0) override;
    
    // Properties
    size_t GetSize() const override { return size_; }
    RhiBufferUsage GetUsage() const override { return usage_; }
    void* GetNativeHandle() const override { return static_cast<void*>(buffer_); }
    
    // Vulkan-specific getters
    VkBuffer GetVkBuffer() const { return buffer_; }
    VkDeviceMemory GetVkMemory() const { return memory_; }
};