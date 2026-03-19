module;
#include "vulkan/vulkan.h"
#include "spdlog/spdlog.h"
module Engine.Rhi.Vulkan.Buffer;
import Engine.Rhi.Vulkan.Tool;
namespace Engine::Rhi
{
    VulkanBuffer::VulkanBuffer(VulkanDevice *device, const BufferDesc &desc)
        : device_(device), desc_(desc)
    {
        // 1. 创建 Buffer 对象 (此时还没分配显存)
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = desc.Size;
        bufferInfo.usage = Tool::ConvertUsage(desc.Usage);
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        Tool::CheckResult(vkCreateBuffer(device_->GetDevice(), &bufferInfo, nullptr, &buffer_));

        // 2. 获取内存需求 (Size 和 Alignment)
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device_->GetDevice(), buffer_, &memRequirements);

        // 3. 确定内存属性 (Memory Properties)
        VkMemoryPropertyFlags properties = 0;
        switch (desc.MemoryUsage)
        {
        case BufferMemoryUsage::CpuToGpu:
            // Host Visible (CPU可见) | Host Coherent (自动同步，不需要手动Flush)
            properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            break;
        case BufferMemoryUsage::GpuOnly:
            // Device Local (显卡专用，最快)
            properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        case BufferMemoryUsage::CpuOnly:
            // Cached (读得快)
            properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            break;
        }

        // 4. 分配内存
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;

        // 使用 VulkanDevice 中已经实现的 FindMemoryType
        allocInfo.memoryTypeIndex = device_->FindMemoryType(memRequirements.memoryTypeBits, properties);

        Tool::CheckResult(vkAllocateMemory(device_->GetDevice(), &allocInfo, nullptr, &memory_));

        // 5. 绑定内存到 Buffer
        Tool::CheckResult(vkBindBufferMemory(device_->GetDevice(), buffer_, memory_, 0));

        // spdlog::debug("Buffer Created: Size={}, Usage={}", desc.Size, (int)desc.Usage);
    }

    void *VulkanBuffer::Map()
    {
        // 只有 CPU 可见的内存才能 Map
        if (desc_.MemoryUsage == BufferMemoryUsage::GpuOnly)
        {
            spdlog::error("Cannot map DeviceLocal (GpuOnly) buffer!");
            return nullptr;
        }

        if (!mappedPtr_)
        {
            // Map 整个 Buffer
            Tool::CheckResult(vkMapMemory(device_->GetDevice(), memory_, 0, desc_.Size, 0, &mappedPtr_));
        }
        return mappedPtr_;
    }

    void VulkanBuffer::Unmap()
    {
        if (mappedPtr_)
        {
            vkUnmapMemory(device_->GetDevice(), memory_);
            mappedPtr_ = nullptr;
        }
    }
    VulkanBuffer::~VulkanBuffer()
    {
        if (buffer_)
        {
            vkDestroyBuffer(device_->GetDevice(), buffer_, nullptr);
        }
        if (memory_)
        {
            vkFreeMemory(device_->GetDevice(), memory_, nullptr);
        }
    }
    uint64_t VulkanBuffer::GetSize() const
    {
        return desc_.Size;
    }
    void VulkanBuffer::WriteData(const void *data, uint64_t size, uint64_t offset)
    {
        // 1. 安全检查：防止越界写
        if (offset + size > desc_.Size)
        {
            spdlog::error("[VulkanBuffer] WriteData Overflow! Buffer Size: {}, Write Range: {}-{}",
                          desc_.Size, offset, offset + size);
            return;
        }

        // 2. 映射内存
        // 注意：Map() 内部会检查 MemoryUsage。如果是 GpuOnly (DeviceLocal)，Map 会失败返回 nullptr。
        void *ptr = Map();

        if (ptr)
        {
            // 3. 指针偏移 (注意要转成 uint8_t* 或 char* 才能做字节偏移)
            uint8_t *destPtr = static_cast<uint8_t *>(ptr) + offset;

            // 4. 内存拷贝
            std::memcpy(destPtr, data, size);

            // 5. 解除映射
            // 对于 Staging Buffer，通常写完就 Unmap
            // 我们的构造函数中使用了 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT，所以不需要手动 vkFlushMappedMemoryRanges
            Unmap();
        }
        else
        {
            // 如果 Map 失败（通常是因为试图写入 DeviceLocal 显存）
            spdlog::error("[VulkanBuffer] Failed to map memory. Cannot WriteData directly to GpuOnly buffer. Use Staging Buffer + Copy instead.");
        }
    }
}