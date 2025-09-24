module;
#include "vulkan/vulkan.h"
#include <vector>
#include <string>

export module VulkanUtils;
import std;
import RhiTypes;
import Logger;

// Vulkan utility functions for basic setup
export namespace VulkanUtils
{
    // Check if validation layers are available
    bool CheckValidationLayerSupport(const std::vector<const char *> &validationLayers);

    // Create Vulkan instance with basic extensions
    VkInstance CreateInstance(const char *applicationName, bool enableValidation);

    // Create surface from GLFW window
    VkSurfaceKHR CreateSurface(VkInstance instance, void *windowHandle);

    // Pick suitable physical device
    VkPhysicalDevice PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);

    // Find queue family indices
    struct QueueFamilyIndices
    {
        uint32_t graphicsFamily = UINT32_MAX;
        uint32_t presentFamily = UINT32_MAX;
        uint32_t computeFamily = UINT32_MAX;

        bool IsComplete() const
        {
            return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
        }

        bool HasDedicatedCompute() const
        {
            return computeFamily != UINT32_MAX && computeFamily != graphicsFamily;
        }
    };
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

    // Create logical device
    VkDevice CreateLogicalDevice(VkPhysicalDevice physicalDevice,
                                 const QueueFamilyIndices &indices,
                                 bool enableValidation);

    // Swapchain support details
    struct SwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };
    SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    // Choose swapchain settings
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, uint32_t width, uint32_t height);
    // Helper function to convert RHI format to Vulkan format
    VkFormat RhiFormatToVkFormat(RhiFormat format);
}
export VkFormat RhiFormatToVkFormat(RhiFormat format)
{
    switch (format)
    {
    case RhiFormat::Undefined:
        return VK_FORMAT_UNDEFINED;

    // Color formats
    case RhiFormat::R8_UNORM:
        return VK_FORMAT_R8_UNORM;
    case RhiFormat::R8G8_UNORM:
        return VK_FORMAT_R8G8_UNORM;
    case RhiFormat::R8G8B8_UNORM:
        return VK_FORMAT_R8G8B8_UNORM;
    case RhiFormat::R8G8B8A8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case RhiFormat::R8G8B8A8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case RhiFormat::B8G8R8A8_UNORM:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case RhiFormat::B8G8R8A8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case RhiFormat::A8B8G8R8_UNORM_PACK32:
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;

    // Float formats
    case RhiFormat::R16_SFLOAT:
        return VK_FORMAT_R16_SFLOAT;
    case RhiFormat::R16G16_SFLOAT:
        return VK_FORMAT_R16G16_SFLOAT;
    case RhiFormat::R16G16B16_SFLOAT:
        return VK_FORMAT_R16G16B16_SFLOAT;
    case RhiFormat::R16G16B16A16_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case RhiFormat::R32_SFLOAT:
        return VK_FORMAT_R32_SFLOAT;
    case RhiFormat::R32G32_SFLOAT:
        return VK_FORMAT_R32G32_SFLOAT;
    case RhiFormat::R32G32B32_SFLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case RhiFormat::R32G32B32A32_SFLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;

    // Depth formats
    case RhiFormat::D16_UNORM:
        return VK_FORMAT_D16_UNORM;
    case RhiFormat::D24_UNORM_S8_UINT:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case RhiFormat::D32_SFLOAT:
        return VK_FORMAT_D32_SFLOAT;
    case RhiFormat::D32_SFLOAT_S8_UINT:
        return VK_FORMAT_D32_SFLOAT_S8_UINT;

    // Integer formats
    case RhiFormat::R32_UINT:
        return VK_FORMAT_R32_UINT;
    case RhiFormat::R32G32_UINT:
        return VK_FORMAT_R32G32_UINT;
    case RhiFormat::R32G32B32_UINT:
        return VK_FORMAT_R32G32B32_UINT;
    case RhiFormat::R32G32B32A32_UINT:
        return VK_FORMAT_R32G32B32A32_UINT;

    default:
        Log::Warn(std::format("Unsupported RhiFormat: {}", static_cast<int>(format)));
        return VK_FORMAT_UNDEFINED;
    }
}