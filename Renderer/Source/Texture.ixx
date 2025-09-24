module;
#include "vulkan/vulkan.h"
#include "ktx.h"
#include <ktxvulkan.h>
export module TextureMod;
import std;
import DeviceMod;

export class Texture
{
public:
	OldVulkanDevice* device;
	VkFormat              format;
	VkImage               image;
	VkImageLayout         imageLayout;
	VkDeviceMemory        deviceMemory;
	VkImageView           view;
	uint32_t              width, height;
	uint32_t 		      dimZ;
	uint32_t              mipLevels;
	uint32_t              layerCount;
	VkDescriptorImageInfo descriptor;
	VkSampler             sampler;

	void      UpdateDescriptor();
	void      Destroy();
	ktxResult LoadKTXFile(std::string filename, ktxTexture** target);
};


export class Texture2D : public Texture
{
public:
	void LoadFromFile(
		std::string        filename,
		VkFormat           format,
		OldVulkanDevice* device,
		VkQueue            copyQueue,
		VkFilter filter,
		VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout      imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		bool               forceLinear = false);
	void FromBuffer(
		void* buffer,
		VkDeviceSize       bufferSize,
		VkFormat           format,
		uint32_t           texWidth,
		uint32_t           texHeight,
		OldVulkanDevice* device,
		VkQueue            copyQueue,
		VkFilter           filter = VK_FILTER_LINEAR,
		VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout      imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};


export class Texture2DArray : public Texture
{
public:
	void LoadFromFile(
		std::string        filename,
		VkFormat           format,
		OldVulkanDevice* device,
		VkQueue            copyQueue,
		VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout      imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	void FromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat format,
		uint32_t texWidth, uint32_t texHeight, uint32_t layerCount,
		OldVulkanDevice* device, VkQueue copyQueue, VkFilter filter,
		VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout);
	void Create(uint32_t width, uint32_t height, uint32_t layerCount, VkFormat format, VkImageUsageFlags usage, VkQueue copyQueue);
};
export class TextureCubeMap : public Texture
{
public:
	void LoadFromFile(
		std::string        filename,
		VkFormat           format,
		OldVulkanDevice* device,
		VkQueue            copyQueue,
		VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout      imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};

export class Texture3D :public Texture
{
public:
	void Create(uint32_t dimX, uint32_t dimY, uint32_t dimZ, OldVulkanDevice* device, VkQueue queue, VkFormat format, VkImageUsageFlags usage, VkImageLayout imageLayout);
};

