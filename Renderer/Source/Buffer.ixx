module;
#include "vulkan/vulkan.h"
export module BufferMod;
import std;
import ToolMod;

export class Buffer
{
public:
	// VkResult Create(VkDevice vulkanDevice, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
	VkResult Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void Unmap();
	VkResult Bind(VkDeviceSize offset = 0);
	void SetupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void CopyTo(void *data, VkDeviceSize size);
	VkResult Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	// VkResult Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void Destroy();

	VkDevice device{};
	VkBuffer buffer{};
	VkDeviceMemory memory{};
	VkDescriptorBufferInfo descriptor{};
	VkDeviceSize size{};
	VkDeviceSize alignment{};
	void *mapped{};
	///** @brief Usage flags to be filled by external source at buffer creation (to query at some later point) */
	VkBufferUsageFlags usageFlags{};
	///** @brief Memory property flags to be filled by external source at buffer creation (to query at some later point) */
	VkMemoryPropertyFlags memoryPropertyFlags{};

private:
};