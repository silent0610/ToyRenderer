module;
#include "vulkan/vulkan.h"
#include "assert.h"
export module ToolMod;

namespace Tool
{
	export VkShaderModule LoadShader(const char* fileName, VkDevice device);
}