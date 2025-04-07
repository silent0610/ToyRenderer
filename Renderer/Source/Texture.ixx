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
	VulkanDevice* device;
	VkImage               image;
	VkImageLayout         imageLayout;
	VkDeviceMemory        deviceMemory;
	VkImageView           view;
	uint32_t              width, height;
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
		VulkanDevice* device,
		VkQueue            copyQueue,
		VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout      imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		bool               forceLinear = false);
	void FromBuffer(
		void* buffer,
		VkDeviceSize       bufferSize,
		VkFormat           format,
		uint32_t           texWidth,
		uint32_t           texHeight,
		VulkanDevice* device,
		VkQueue            copyQueue,
		VkFilter           filter = VK_FILTER_LINEAR,
		VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout      imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};

class Texture2DArray : public Texture
{
public:
	void LoadFromFile(
		std::string        filename,
		VkFormat           format,
		VulkanDevice* device,
		VkQueue            copyQueue,
		VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout      imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};

export class TextureCubeMap : public Texture
{
public:
	void LoadFromFile(
		std::string        filename,
		VkFormat           format,
		VulkanDevice* device,
		VkQueue            copyQueue,
		VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout      imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};