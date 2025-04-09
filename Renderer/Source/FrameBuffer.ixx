module;
#include <assert.h>
#include "vulkan/vulkan.h"

export module FrameBufferMod;
import DeviceMod;
import std;


export struct FramebufferAttachment
{
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkFormat format;
	VkImageSubresourceRange subresourceRange;
	VkAttachmentDescription description;

	/**
	* @brief Returns true if the attachment has a depth component
	*/
	bool HasDepth();

	/**
	* @brief Returns true if the attachment has a stencil component
	*/
	bool HasStencil();

	/**
	* @brief Returns true if the attachment is a depth and/or stencil attachment
	*/
	bool IsDepthStencil();

	void Destroy();

};


/**
* @brief Describes the attributes of an attachment to be created
*/
export struct AttachmentCreateInfo
{
	uint32_t width, height;
	uint32_t layerCount;
	VkFormat format;
	VkImageUsageFlags usage;
	VkSampleCountFlagBits imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
};

/**
	* @brief Encapsulates a complete Vulkan framebuffer with an arbitrary number and combination of attachments
	*/
export struct Framebuffer
{
private:
	VulkanDevice* vulkanDevice;
public:
	uint32_t width, height;
	VkFramebuffer framebuffer;
	VkRenderPass renderPass;
	VkSampler sampler;
	std::vector<FramebufferAttachment> attachments;
	std::vector<FramebufferAttachment> defaultMaterials;
	/**
	* Default constructor
	*
	* @param vulkanDevice Pointer to a valid VulkanDevice
	*/
	Framebuffer(VulkanDevice* vulkanDevice);

	/**
	* Destroy and free Vulkan resources used for the framebuffer and all of its attachments
	*/
	~Framebuffer();
	/**
	* Add a new attachment described by createinfo to the framebuffer's attachment list
	*
	* @param createinfo Structure that specifies the framebuffer to be constructed
	*
	* @return Index of the new attachment
	*/
	uint32_t AddAttachment(AttachmentCreateInfo createinfo);

	/**
	* Creates a default sampler for sampling from any of the framebuffer attachments
	* Applications are free to create their own samplers for different use cases
	*
	* @param magFilter Magnification filter for lookups
	* @param minFilter Minification filter for lookups
	* @param adressMode Addressing mode for the U,V and W coordinates
	*
	* @return VkResult for the sampler creation
	*/
	VkResult CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode adressMode);

	/**
	* Creates a default render pass setup with one sub pass
	*
	* @return VK_SUCCESS if all resources have been created successfully
	*/
	VkResult CreateRenderPass();
};