module;
#include "vulkan/vulkan.h"
export module DeviceMod;
import std;
import BufferMod;

/// @brief 封装和管理与 Vulkan 物理设备和逻辑设备相关的各种资源和操作。它使得在 Vulkan 中处理设备、命令池、内存管理、队列等复杂的细节变得更加简单和清晰。具
export struct VulkanDevice
{
	/** @brief Physical device representation */
	VkPhysicalDevice physicalDevice;
	/** @brief Logical device representation (application's view of the device) */
	VkDevice logicalDevice;
	/** @brief Properties of the physical device including limits that the application can check against */
	VkPhysicalDeviceProperties properties;
	/** @brief Features of the physical device that an application can use to check if a feature is supported */
	VkPhysicalDeviceFeatures features;
	/** @brief Features that have been enabled for use on the physical device */
	VkPhysicalDeviceFeatures enabledFeatures;
	/** @brief Memory types and heaps of the physical device */
	VkPhysicalDeviceMemoryProperties memoryProperties;
	/** @brief Queue family properties of the physical device */
	std::vector<VkQueueFamilyProperties> queueFamilyProperties;
	/** @brief List of extensions supported by the device */
	std::vector<std::string> supportedExtensions;
	/** @brief Default command pool for the graphics queue family index */
	VkCommandPool commandPool = VK_NULL_HANDLE;
	/** @brief Contains queue family indices */
	struct
	{
		uint32_t graphics;
		uint32_t compute;
		uint32_t transfer;
	} queueFamilyIndices;
	operator VkDevice() const
	{
		return logicalDevice;
	};
	explicit VulkanDevice(VkPhysicalDevice physicalDevice);
	~VulkanDevice();
	uint32_t        GetMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr) const;
	uint32_t        GetQueueFamilyIndex(VkQueueFlags queueFlags) const;
	VkResult        CreateLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
	VkResult        CreateBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, void* data = nullptr);
	VkResult        CreateBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, Buffer* buffer, VkDeviceSize size, void* data = nullptr);
	void            CopyBuffer(Buffer* src, Buffer* dst, VkQueue queue, VkBufferCopy* copyRegion = nullptr);
	VkCommandPool   CreateCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin = false);
	VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level, bool begin = false);
	void            FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true, bool debug = false);
	void            FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true, bool debug = false);
	bool            ExtensionSupported(std::string extension);
	VkFormat        GetSupportedDepthFormat(bool checkSamplingSupport);

};