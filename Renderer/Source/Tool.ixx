module;
#include "vulkan/vulkan.h"
#include "assert.h"
export module ToolMod;
import std;


namespace Tool
{
	export VkShaderModule LoadShader(const char* fileName, VkDevice device);
	export std::string ErrorString(VkResult errorCode);
	export void SetImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkImageSubresourceRange subresourceRange,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	// Uses a fixed sub resource layout with first mip level and layer
	export void SetImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	export std::string GetProjectPath();
	export std::string GetShadersPath();
	export std::string GetAssetsPath();
}