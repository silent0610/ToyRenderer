module;
#include "vulkan/vulkan.h"
#include "assert.h"
#include "memory"
#include "ktx.h"
#include "StbImg/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

module TextureMod;
import InitMod;
import ToolMod;
import DeviceMod;
import Logger;


void Texture::UpdateDescriptor()
{
	descriptor.sampler = sampler;
	descriptor.imageView = view;
	descriptor.imageLayout = imageLayout;
}

void Texture::Destroy()
{
	vkDestroyImageView(device->logicalDevice, view, nullptr);
	vkDestroyImage(device->logicalDevice, image, nullptr);
	if (sampler)
	{
		vkDestroySampler(device->logicalDevice, sampler, nullptr);
	}
	vkFreeMemory(device->logicalDevice, deviceMemory, nullptr);
}
void Texture2D::LoadFromFile(std::string filename, VkFormat format, OldVulkanDevice* device, VkQueue copyQueue, VkFilter filter, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout, bool forceLinear)
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * texChannels;
	//width = texWidth;
	//height = texHeight;
	//mipLevels = 1;
	//this->device = device;

	if (!pixels)
	{
		throw std::runtime_error("failed to load texture image!");
	}

	FromBuffer(pixels, imageSize, format, texWidth, texHeight, device, copyQueue, filter);
	stbi_image_free(pixels);
	//VkBuffer stagingBuffer;
	//VkMemoryAllocateInfo memAllocInfo = Init::memoryAllocateInfo();
	//VkMemoryRequirements memReqs;
	//VkDeviceMemory stagingBufferMemory;

	//VkBufferCreateInfo bufferCreateInfo = Init::bufferCreateInfo();
	//bufferCreateInfo.size = imageSize;
	//// This buffer is used as a transfer source for the buffer copy
	//bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	//bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	//Tool::CheckResult(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	//vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);
	//memAllocInfo.allocationSize = memReqs.size;
	//// Get memory type index for a host visible buffer
	//memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	//Tool::CheckResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingBufferMemory));

	//Tool::CheckResult(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingBufferMemory, 0));


	//uint8_t* data;
	//vkMapMemory(device->logicalDevice, stagingBufferMemory, 0, imageSize, 0, (void**)&data);
	//memcpy(data, pixels, static_cast<size_t>(imageSize));
	//vkUnmapMemory(device->logicalDevice, stagingBufferMemory);



	//createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

	//transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	//copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	//transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	//vkDestroyBuffer(device, stagingBuffer, nullptr);
	//vkFreeMemory(device, stagingBufferMemory, nullptr);
}
void Texture2DArray::FromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat format,
	uint32_t texWidth, uint32_t texHeight, uint32_t layerCount,
	OldVulkanDevice* device, VkQueue copyQueue, VkFilter filter,
	VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
	assert(buffer);
	assert(layerCount > 1); // 2D数组至少需要2层

	this->device = device;
	width = texWidth;
	height = texHeight;
	layerCount = layerCount;
	mipLevels = 1;

	VkMemoryAllocateInfo memAllocInfo = Init::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	// 使用单独的命令缓冲区进行纹理加载
	VkCommandBuffer copyCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// 创建包含原始图像数据的暂存缓冲区
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo = Init::bufferCreateInfo();
	bufferCreateInfo.size = bufferSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	Tool::CheckResult(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	// 获取暂存缓冲区的内存需求
	vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	Tool::CheckResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
	Tool::CheckResult(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

	// 将纹理数据复制到暂存缓冲区
	uint8_t* data;
	Tool::CheckResult(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	memcpy(data, buffer, bufferSize);
	vkUnmapMemory(device->logicalDevice, stagingMemory);

	// 设置每个层的缓冲区图像拷贝区域
	std::vector<VkBufferImageCopy> bufferCopyRegions;
	VkDeviceSize layerSize = bufferSize / layerCount;

	for (uint32_t layer = 0; layer < layerCount; layer++)
	{
		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = width;
		bufferCopyRegion.imageExtent.height = height;
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = layer * layerSize;
		bufferCopyRegions.push_back(bufferCopyRegion);
	}

	// 创建优化的2D数组图像
	VkImageCreateInfo imageCreateInfo = Init::imageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.mipLevels = mipLevels;
	imageCreateInfo.arrayLayers = layerCount; // 关键区别：设置层数
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.extent = { width, height, 1 };
	imageCreateInfo.usage = imageUsageFlags;

	// 确保设置了TRANSFER_DST标志
	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	Tool::CheckResult(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

	vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	Tool::CheckResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
	Tool::CheckResult(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = mipLevels;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = layerCount; // 包含所有层

	// 图像屏障 - 将图像布局从UNDEFINED转换为TRANSFER_DST_OPTIMAL
	Tool::SetImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);

	// 从暂存缓冲区复制所有层
	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<uint32_t>(bufferCopyRegions.size()),
		bufferCopyRegions.data());

	// 将所有层转换为最终布局
	this->imageLayout = imageLayout;
	Tool::SetImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		imageLayout,
		subresourceRange);

	device->FlushCommandBuffer(copyCmd, copyQueue);

	// 清理暂存资源
	vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);

	// 创建采样器
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = filter;
	samplerCreateInfo.minFilter = filter;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	Tool::CheckResult(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

	// 创建2D数组图像视图 (关键区别)
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; // 关键区别：使用2D_ARRAY类型
	viewCreateInfo.format = format;
	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, layerCount }; // 包含所有层
	viewCreateInfo.image = image;
	Tool::CheckResult(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

	// 更新描述符图像信息
	UpdateDescriptor();
}
void Texture2D::FromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat format, uint32_t texWidth, uint32_t texHeight, OldVulkanDevice* device, VkQueue copyQueue, VkFilter filter, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
	assert(buffer);

	this->device = device;
	width = texWidth;
	height = texHeight;
	mipLevels = 1;

	VkMemoryAllocateInfo memAllocInfo = Init::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	// Use a separate command buffer for texture loading
	VkCommandBuffer copyCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Create a host-visible staging buffer that contains the raw image data
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo = Init::bufferCreateInfo();
	bufferCreateInfo.size = bufferSize;
	// This buffer is used as a transfer source for the buffer copy
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	Tool::CheckResult(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	// Get memory requirements for the staging buffer (alignment, memory type bits)
	vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	// Get memory type index for a host visible buffer
	memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	Tool::CheckResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
	Tool::CheckResult(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

	// Copy texture data into staging buffer
	uint8_t* data;
	Tool::CheckResult(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	memcpy(data, buffer, bufferSize);
	vkUnmapMemory(device->logicalDevice, stagingMemory);

	VkBufferImageCopy bufferCopyRegion = {};
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.mipLevel = 0;
	bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = width;
	bufferCopyRegion.imageExtent.height = height;
	bufferCopyRegion.imageExtent.depth = 1;
	bufferCopyRegion.bufferOffset = 0;

	// Create optimal tiled target image
	VkImageCreateInfo imageCreateInfo = Init::imageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.mipLevels = mipLevels;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.extent = { width, height, 1 };
	imageCreateInfo.usage = imageUsageFlags;
	// Ensure that the TRANSFER_DST bit is set for staging
	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	Tool::CheckResult(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

	vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;

	memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	Tool::CheckResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
	Tool::CheckResult(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = mipLevels;
	subresourceRange.layerCount = 1;

	// Image barrier for optimal image (target)
	// Optimal image will be used as destination for the copy
	Tool::SetImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);

	// Copy mip levels from staging buffer
	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&bufferCopyRegion
	);

	// Change texture image layout to shader read after all mip levels have been copied
	this->imageLayout = imageLayout;
	Tool::SetImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		imageLayout,
		subresourceRange);

	device->FlushCommandBuffer(copyCmd, copyQueue);

	// Clean up staging resources
	vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = filter;
	samplerCreateInfo.minFilter = filter;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	Tool::CheckResult(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

	// Create image view
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.pNext = NULL;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = format;
	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.image = image;
	Tool::CheckResult(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

	// Update descriptor image info member that can be used for setting up descriptor sets
	UpdateDescriptor();
}
ktxResult Texture::LoadKTXFile(std::string filename, ktxTexture** target)
{
	ktxResult result = KTX_SUCCESS;

	if (!Tool::CheckFileExists(filename))
	{
		Tool::ExitFatal("Could not load texture from " + filename + "\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
	}
	result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, target);

	return result;
}
void TextureCubeMap::LoadFromFile(std::string filename, VkFormat format, OldVulkanDevice* device, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
	ktxTexture* ktxTexture;
	ktxResult result = LoadKTXFile(filename, &ktxTexture);
	assert(result == KTX_SUCCESS);

	this->device = device;
	width = ktxTexture->baseWidth;
	height = ktxTexture->baseHeight;
	mipLevels = ktxTexture->numLevels;

	ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
	ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

	VkMemoryAllocateInfo memAllocInfo = Init::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	// Create a host-visible staging buffer that contains the raw image data
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo = Init::bufferCreateInfo();
	bufferCreateInfo.size = ktxTextureSize;
	// This buffer is used as a transfer source for the buffer copy
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	Tool::CheckResult(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	// Get memory requirements for the staging buffer (alignment, memory type bits)
	vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	// Get memory type index for a host visible buffer
	memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	Tool::CheckResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
	Tool::CheckResult(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

	// Copy texture data into staging buffer
	uint8_t* data;
	Tool::CheckResult(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	memcpy(data, ktxTextureData, ktxTextureSize);
	vkUnmapMemory(device->logicalDevice, stagingMemory);

	// Setup buffer copy regions for each face including all of its mip levels
	std::vector<VkBufferImageCopy> bufferCopyRegions;

	for (uint32_t face = 0; face < 6; face++)
	{
		for (uint32_t level = 0; level < mipLevels; level++)
		{
			ktx_size_t offset;
			KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);
			assert(result == KTX_SUCCESS);

			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = level;
			bufferCopyRegion.imageSubresource.baseArrayLayer = face;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
			bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;

			bufferCopyRegions.push_back(bufferCopyRegion);
		}
	}

	// Create optimal tiled target image
	VkImageCreateInfo imageCreateInfo = Init::imageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.mipLevels = mipLevels;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.extent = { width, height, 1 };
	imageCreateInfo.usage = imageUsageFlags;
	// Ensure that the TRANSFER_DST bit is set for staging
	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	// Cube faces count as array layers in Vulkan
	imageCreateInfo.arrayLayers = 6;
	// This flag is required for cube map images
	imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;


	Tool::CheckResult(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

	vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	Tool::CheckResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
	Tool::CheckResult(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

	// Use a separate command buffer for texture loading
	VkCommandBuffer copyCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Image barrier for optimal image (target)
	// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = mipLevels;
	subresourceRange.layerCount = 6;

	Tool::SetImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);

	// Copy the cube map faces from the staging buffer to the optimal tiled image
	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<uint32_t>(bufferCopyRegions.size()),
		bufferCopyRegions.data());

	// Change texture image layout to shader read after all faces have been copied
	this->imageLayout = imageLayout;
	Tool::SetImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		imageLayout,
		subresourceRange);

	device->FlushCommandBuffer(copyCmd, copyQueue);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo = Init::samplerCreateInfo();
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
	samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f;
	samplerCreateInfo.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = (float)mipLevels;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	Tool::CheckResult(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

	// Create image view
	VkImageViewCreateInfo viewCreateInfo = Init::imageViewCreateInfo();
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewCreateInfo.format = format;
	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	viewCreateInfo.subresourceRange.layerCount = 6;
	viewCreateInfo.subresourceRange.levelCount = mipLevels;
	viewCreateInfo.image = image;
	Tool::CheckResult(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

	// Clean up staging resources
	ktxTexture_Destroy(ktxTexture);
	vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);

	// Update descriptor image info member that can be used for setting up descriptor sets
	UpdateDescriptor();
}

void Texture2DArray::Create(uint32_t width, uint32_t height, uint32_t layerCount, VkFormat format, VkImageUsageFlags usage, VkQueue copyQueue)
{
	this->device = device;
	this->width = width;
	this->height = height;
	this->layerCount = layerCount;
	this->mipLevels = 1;
	this->format = format;

	VkImageCreateInfo imageCreateInfo = Init::imageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.extent = { width, height, 1 };
	imageCreateInfo.mipLevels = mipLevels;
	imageCreateInfo.arrayLayers = layerCount;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = usage;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	Tool::CheckResult(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

	VkMemoryAllocateInfo memAllocInfo = Init::memoryAllocateInfo();
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	Tool::CheckResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
	Tool::CheckResult(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

	// Create a default sampler
	VkSamplerCreateInfo samplerCreateInfo = Init::samplerCreateInfo();
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 1.0f;
	Tool::CheckResult(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

	// Create image view
	VkImageViewCreateInfo viewCreateInfo = Init::imageViewCreateInfo();
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewCreateInfo.image = image;
	viewCreateInfo.format = format;
	viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = layerCount;

	Tool::CheckResult(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

	// Set initial layout
	imageLayout = VK_IMAGE_LAYOUT_GENERAL; // General layout is suitable for storage images
	VkCommandBuffer cmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	Tool::SetImageLayout(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, imageLayout, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layerCount });
	device->FlushCommandBuffer(cmd, copyQueue);

	UpdateDescriptor();
}


void Texture3D::Create(uint32_t dimX, uint32_t dimY, uint32_t dimZ, OldVulkanDevice* device, VkQueue queue, VkFormat format, VkImageUsageFlags usage, VkImageLayout imageLayout)
{
	this->device = device;
	this->format = format;
	this->imageLayout = imageLayout;
	this->width = dimX;
	this->height = dimY;
	this->dimZ = dimZ;
	this->mipLevels = 1;
	this->layerCount = 1;
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_3D;
	imageInfo.format = format;
	imageInfo.extent = { dimX,dimY,dimZ };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = usage;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	Tool::CheckResult(vkCreateImage(device->logicalDevice, &imageInfo, nullptr, &image));

	VkMemoryRequirements memRequirement;
	vkGetImageMemoryRequirements(device->logicalDevice, image, &memRequirement);
	VkMemoryAllocateInfo memAllocInfo = Init::memoryAllocateInfo();
	memAllocInfo.allocationSize = memRequirement.size;
	memAllocInfo.memoryTypeIndex = device->GetMemoryType(memRequirement.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	Tool::CheckResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));

	vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
	viewInfo.format = format;
	viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	Tool::CheckResult(vkCreateImageView(device->logicalDevice, &viewInfo, nullptr, &view));

	// Create a default sampler
	VkSamplerCreateInfo samplerCreateInfo = Init::samplerCreateInfo();
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 1.0f;
	Tool::CheckResult(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

	VkCommandBuffer cmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	Tool::SetImageLayout(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, imageLayout, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layerCount });
	device->FlushCommandBuffer(cmd, queue);

	UpdateDescriptor();
}

void Texture3D::LoadFromRawFile(
	const std::string& filename,
	uint32_t dimX,
	uint32_t dimY,
	uint32_t dimZ,
	VkFormat format,
	OldVulkanDevice* device,
	VkQueue copyQueue,
	VkImageUsageFlags imageUsageFlags,
	VkImageLayout imageLayout)
{
	// Read raw binary file containing float data
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		Log::Error(std::format("Failed to open raw file: {}", filename));
		return;
	}

	// Get file size and calculate expected size
	std::streamsize fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	size_t expectedSize = static_cast<size_t>(dimX) * dimY * dimZ * sizeof(float);
	if (fileSize != expectedSize)
	{
		Log::Warn(std::format("File size mismatch. Expected: {} bytes, Got: {} bytes", expectedSize, fileSize));
	}

	// Read float data from file
	std::vector<float> sdfData(dimX * dimY * dimZ);
	if (!file.read(reinterpret_cast<char*>(sdfData.data()), fileSize))
	{
		Log::Error(std::format("Failed to read data from raw file: {}", filename));
		file.close();
		return;
	}
	file.close();

	Log::Info(std::format("Loaded raw SDF file: {}", filename));
	Log::Info(std::format("  Dimensions: {}x{}x{}", dimX, dimY, dimZ));
	Log::Info(std::format("  Data size: {} MB", fileSize / (1024.0f * 1024.0f)));

	// Initialize texture members
	this->device = device;
	this->format = format;
	this->imageLayout = imageLayout;
	this->width = dimX;
	this->height = dimY;
	this->dimZ = dimZ;
	this->mipLevels = 1;
	this->layerCount = 1;

	VkMemoryAllocateInfo memAllocInfo = Init::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	// Create staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo = Init::bufferCreateInfo();
	bufferCreateInfo.size = fileSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	Tool::CheckResult(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	Tool::CheckResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
	Tool::CheckResult(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

	// Copy SDF data to staging buffer
	uint8_t* data;
	Tool::CheckResult(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	memcpy(data, sdfData.data(), fileSize);
	vkUnmapMemory(device->logicalDevice, stagingMemory);

	// Create 3D image
	VkImageCreateInfo imageCreateInfo = Init::imageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
	imageCreateInfo.format = format;
	imageCreateInfo.extent = { dimX, dimY, dimZ };
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = imageUsageFlags;

	// Ensure TRANSFER_DST flag is set
	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	Tool::CheckResult(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

	vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	Tool::CheckResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
	Tool::CheckResult(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

	// Create command buffer for copying
	VkCommandBuffer copyCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Transition image layout from UNDEFINED to TRANSFER_DST_OPTIMAL
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;

	Tool::SetImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);

	// Copy buffer to image
	VkBufferImageCopy bufferCopyRegion = {};
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.mipLevel = 0;
	bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = dimX;
	bufferCopyRegion.imageExtent.height = dimY;
	bufferCopyRegion.imageExtent.depth = dimZ;
	bufferCopyRegion.bufferOffset = 0;

	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&bufferCopyRegion);

	// Transition to final layout
	this->imageLayout = imageLayout;
	Tool::SetImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		imageLayout,
		subresourceRange);

	device->FlushCommandBuffer(copyCmd, copyQueue);

	// Clean up staging resources
	vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo = Init::samplerCreateInfo();
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 1.0f;
	Tool::CheckResult(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

	// Create 3D image view
	VkImageViewCreateInfo viewCreateInfo = Init::imageViewCreateInfo();
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
	viewCreateInfo.format = format;
	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	viewCreateInfo.image = image;
	Tool::CheckResult(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

	// Update descriptor
	UpdateDescriptor();

	Log::Info(std::format("Successfully created 3D texture from raw file: {}", filename));
}