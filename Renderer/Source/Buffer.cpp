module;
#include "vulkan/vulkan.h"
#include <stdexcept>
#include <assert.h>

module BufferMod;

VkResult Buffer::Map(VkDeviceSize size, VkDeviceSize offset)
{
	return vkMapMemory(device, memory, offset, size, 0, &mapped);
}
void Buffer::Unmap()
{
	if (mapped)
	{
		vkUnmapMemory(device, memory);
		mapped = nullptr;
	}
}

VkResult Buffer::Bind(VkDeviceSize offset)
{
	return vkBindBufferMemory(device, buffer, memory, offset);
}
void Buffer::SetupDescriptor(VkDeviceSize size, VkDeviceSize offset)
{
	descriptor.offset = offset;
	descriptor.buffer = buffer;
	descriptor.range = size;
}
void Buffer::CopyTo(void *data, VkDeviceSize size)
{
	assert(mapped);
	memcpy(mapped, data, size);
}

VkResult Buffer::Flush(VkDeviceSize size, VkDeviceSize offset)
{
	VkMappedMemoryRange mappedRange = {};
	mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedRange.memory = memory;
	mappedRange.offset = offset;
	mappedRange.size = size;
	return vkFlushMappedMemoryRanges(device, 1, &mappedRange);
}
void Buffer::Destroy()
{
	if (buffer)
	{
		vkDestroyBuffer(device, buffer, nullptr);
	}
	if (memory)
	{
		vkFreeMemory(device, memory, nullptr);
	}
}
// VkResult Buffer::Create(VkDevice vulkanDevice, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
// {
// 	this->device = vulkanDevice;
// 	size = bufferSize;
// 	usageFlags = usage;
// 	memoryPropertyFlags = properties;

// 	// 创建缓冲区
// 	VkBufferCreateInfo bufferCreateInfo{};
// 	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
// 	bufferCreateInfo.size = size;
// 	bufferCreateInfo.usage = usage;
// 	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

// 	CheckResult(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer));

// 	VkMemoryRequirements memRequirements;
// 	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

// 	// 3. 分配内存
// 	VkMemoryAllocateInfo allocInfo{};
// 	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
// 	allocInfo.allocationSize = memRequirements.size;
// 	allocInfo.memoryTypeIndex = vulkanDevice->GetMemoryType(memRequirements.memoryTypeBits, properties);

// 	return VK_SUCCESS;
// }