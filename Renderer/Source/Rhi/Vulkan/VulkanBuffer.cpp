module;
#include "vulkan/vulkan.h"

module VulkanBuffer;
import std;

VulkanBuffer::VulkanBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, 
                           size_t size, RhiBufferUsage usage)
    : device_(device), buffer_(buffer), memory_(memory), size_(size), 
      usage_(usage), mappedData_(nullptr)
{
    Log::Info("VulkanBuffer created");
}

VulkanBuffer::~VulkanBuffer() {
    if (mappedData_) {
        Unmap();
    }
    
    if (buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer_, nullptr);
    }
    
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, nullptr);
    }
    
    Log::Info("VulkanBuffer destroyed");
}

void* VulkanBuffer::Map() {
    if (mappedData_) {
        Log::Warn("Buffer already mapped");
        return mappedData_;
    }
    
    VkResult result = vkMapMemory(device_, memory_, 0, size_, 0, &mappedData_);
    if (result != VK_SUCCESS) {
        Log::Error(std::format("Failed to map buffer memory: {}", static_cast<int>(result)));
        return nullptr;
    }
    
    Log::Debug("Buffer mapped successfully");
    return mappedData_;
}

void VulkanBuffer::Unmap() {
    if (!mappedData_) {
        Log::Warn("Buffer not mapped");
        return;
    }
    
    vkUnmapMemory(device_, memory_);
    mappedData_ = nullptr;
    
    Log::Debug("Buffer unmapped");
}

void VulkanBuffer::UpdateData(const void* data, size_t dataSize, size_t offset) {
    if (!data) {
        Log::Warn("Attempted to update buffer with null data");
        return;
    }
    
    if (offset + dataSize > size_) {
        Log::Error(std::format("Data update exceeds buffer size: offset={}, dataSize={}, bufferSize={}", 
                              offset, dataSize, size_));
        return;
    }
    
    // Map memory, copy data, then unmap
    void* mapped = Map();
    if (mapped) {
        std::memcpy(static_cast<char*>(mapped) + offset, data, dataSize);
        Unmap();
        
        Log::Debug(std::format("Buffer updated: {} bytes at offset {}", dataSize, offset));
    }
}