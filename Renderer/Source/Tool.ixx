module;
#include "vulkan/vulkan.h"
#include "assert.h"
export module ToolMod;
import std;


namespace Tool
{
	export VkShaderModule LoadShader(const char* fileName, VkDevice device);
	export std::string ErrorString(VkResult errorCode);

}