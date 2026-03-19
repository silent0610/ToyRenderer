module;
#include "vulkan/vulkan.h"
export module Engine.Rhi.Vulkan.Buffer;
import Engine.Rhi.Definition;
import std;
import Engine.Rhi.Buffer;
import Engine.Rhi.Vulkan.Device;
export namespace Engine::Rhi
{
    class VulkanBuffer final : public RhiBuffer
    {
    public:
        VulkanBuffer(VulkanDevice *device, const BufferDesc &desc);
        ~VulkanBuffer() override;

        // --- RhiBuffer 接口 ---
        const BufferDesc &GetDesc() const override { return desc_; }
        void *GetNativeHandle() const override { return (void *)buffer_; }
        uint64_t GetSize() const override;
        void WriteData(const void* data, uint64_t size, uint64_t offset = 0) override;
        void *Map() override;
        void Unmap() override;

        // --- Vulkan 特有访问器 ---
        VkBuffer GetBuffer() const { return buffer_; }
        VkDeviceMemory GetMemory() const { return memory_; }

    private:
        VulkanDevice *device_;
        BufferDesc desc_;

        VkBuffer buffer_ = VK_NULL_HANDLE;
        VkDeviceMemory memory_ = VK_NULL_HANDLE;

        // 缓存映射指针，避免重复 Map
        void *mappedPtr_ = nullptr;
    };
}