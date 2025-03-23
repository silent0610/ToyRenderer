module;
#include "vulkan/vulkan.h"
#include "assert.h"
module ToolMod;
import std;
namespace Tool
{
	VkShaderModule LoadShader(const char* fileName, VkDevice device)
	{
		std::ifstream is(fileName, std::ios::binary | std::ios::in | std::ios::ate);

		if (is.is_open())
		{
			size_t size = is.tellg();
			is.seekg(0, std::ios::beg);
			char* shaderCode = new char[size];
			is.read(shaderCode, size);
			is.close();

			assert(size > 0);

			VkShaderModule shaderModule;
			VkShaderModuleCreateInfo moduleCreateInfo{};
			moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			moduleCreateInfo.codeSize = size;
			moduleCreateInfo.pCode = (uint32_t*)shaderCode;

			if (vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule) != VK_SUCCESS)
			{
				throw std::runtime_error("fail to create module" + std::string(fileName));
			};

			delete[] shaderCode;

			return shaderModule;
		}
		else
		{
			std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << "\n";
			return VK_NULL_HANDLE;
		}
	}
	std::string ErrorString(VkResult errorCode)
	{
		switch (errorCode)
		{
#define STR(r) case VK_ ##r: return #r
			STR(NOT_READY);
			STR(TIMEOUT);
			STR(EVENT_SET);
			STR(EVENT_RESET);
			STR(INCOMPLETE);
			STR(ERROR_OUT_OF_HOST_MEMORY);
			STR(ERROR_OUT_OF_DEVICE_MEMORY);
			STR(ERROR_INITIALIZATION_FAILED);
			STR(ERROR_DEVICE_LOST);
			STR(ERROR_MEMORY_MAP_FAILED);
			STR(ERROR_LAYER_NOT_PRESENT);
			STR(ERROR_EXTENSION_NOT_PRESENT);
			STR(ERROR_FEATURE_NOT_PRESENT);
			STR(ERROR_INCOMPATIBLE_DRIVER);
			STR(ERROR_TOO_MANY_OBJECTS);
			STR(ERROR_FORMAT_NOT_SUPPORTED);
			STR(ERROR_SURFACE_LOST_KHR);
			STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
			STR(SUBOPTIMAL_KHR);
			STR(ERROR_OUT_OF_DATE_KHR);
			STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
			STR(ERROR_VALIDATION_FAILED_EXT);
			STR(ERROR_INVALID_SHADER_NV);
			STR(ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);
#undef STR
		default:
			return "UNKNOWN_ERROR";
		}
	}
}