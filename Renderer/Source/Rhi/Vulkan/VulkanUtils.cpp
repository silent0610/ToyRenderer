module;
#include "vulkan/vulkan.h"
#include "GLFW/glfw3.h"
#include <vector>
#include <set>
#include <algorithm>

module VulkanUtils;
import Logger;
import std;
import RhiTexture;
namespace VulkanUtils
{

    // Helper function to convert RHI texture usage to Vulkan usage
    VkImageUsageFlags RhiTextureUsageToVkUsage(RhiTextureUsage usage)
    {
        VkImageUsageFlags vkUsage = 0;

        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(RhiTextureUsage::Sampled))
        {
            vkUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(RhiTextureUsage::Storage))
        {
            vkUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
        }
        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(RhiTextureUsage::ColorAttachment))
        {
            vkUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(RhiTextureUsage::DepthStencilAttachment))
        {
            vkUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(RhiTextureUsage::TransferSrc))
        {
            vkUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(RhiTextureUsage::TransferDst))
        {
            vkUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        return vkUsage;
    }

    // Helper function to find memory type
    uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        return UINT32_MAX;
    }

    bool CheckValidationLayerSupport(const std::vector<const char *> &validationLayers)
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName : validationLayers)
        {
            bool layerFound = false;
            for (const auto &layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound)
            {
                return false;
            }
        }
        return true;
    }

    VkInstance CreateInstance(const char *applicationName, bool enableValidation)
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = applicationName;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "RHI Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // Get GLFW required extensions
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Validation layers
        const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
        if (enableValidation && CheckValidationLayerSupport(validationLayers))
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        VkInstance instance;
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            return VK_NULL_HANDLE;
        }

        return instance;
    }

    VkSurfaceKHR CreateSurface(VkInstance instance, void *windowHandle)
    {
        VkSurfaceKHR surface;
        if (glfwCreateWindowSurface(instance, static_cast<GLFWwindow *>(windowHandle), nullptr, &surface) != VK_SUCCESS)
        {
            return VK_NULL_HANDLE;
        }
        return surface;
    }

    VkPhysicalDevice PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0)
        {
            return VK_NULL_HANDLE;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // Prioritize discrete GPU over integrated GPU over CPU
        VkPhysicalDevice discreteGpu = VK_NULL_HANDLE;
        VkPhysicalDevice integratedGpu = VK_NULL_HANDLE;
        VkPhysicalDevice fallbackDevice = VK_NULL_HANDLE;

        for (const auto &device : devices)
        {
            QueueFamilyIndices indices = FindQueueFamilies(device, surface);
            if (!indices.IsComplete())
                continue;

            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);

            switch (properties.deviceType)
            {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                if (discreteGpu == VK_NULL_HANDLE)
                {
                    discreteGpu = device;
                    Log::Info("Found discrete GPU: " + std::string(properties.deviceName));
                }
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                if (integratedGpu == VK_NULL_HANDLE)
                {
                    integratedGpu = device;
                    Log::Info("Found integrated GPU: " + std::string(properties.deviceName));
                }
                break;
            default:
                if (fallbackDevice == VK_NULL_HANDLE)
                {
                    fallbackDevice = device;
                    Log::Info("Found fallback device: " + std::string(properties.deviceName));
                }
                break;
            }
        }

        // Return best available device
        if (discreteGpu != VK_NULL_HANDLE)
        {
            Log::Info("Selected discrete GPU for rendering");
            return discreteGpu;
        }
        else if (integratedGpu != VK_NULL_HANDLE)
        {
            Log::Info("Selected integrated GPU for rendering");
            return integratedGpu;
        }
        else if (fallbackDevice != VK_NULL_HANDLE)
        {
            Log::Warn("Using fallback device - performance may be limited");
            return fallbackDevice;
        }

        return VK_NULL_HANDLE;
    }

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        uint32_t i = 0;
        for (const auto &queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i;
            }

            // Look for dedicated compute queue (compute but not graphics)
            if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                indices.computeFamily = i;
            }
            // Fall back to graphics queue for compute if no dedicated queue
            else if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                     indices.computeFamily == UINT32_MAX)
            {
                indices.computeFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport)
            {
                indices.presentFamily = i;
            }

            if (indices.IsComplete())
            {
                break;
            }
            i++;
        }

        return indices;
    }

    VkDevice CreateLogicalDevice(VkPhysicalDevice physicalDevice,
                                 const QueueFamilyIndices &indices,
                                 bool enableValidation)
    {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;

        // Device extensions (swapchain)
        const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // Validation layers for device (deprecated but for compatibility)
        const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
        if (enableValidation)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        VkDevice device;
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        {
            return VK_NULL_HANDLE;
        }

        return device;
    }

    SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        SwapchainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
    {
        for (const auto &availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
    {
        for (const auto &availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, uint32_t width, uint32_t height)
    {
        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            return capabilities.currentExtent;
        }
        else
        {
            VkExtent2D actualExtent = {width, height};

            actualExtent.width = std::clamp(actualExtent.width,
                                            capabilities.minImageExtent.width,
                                            capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height,
                                             capabilities.minImageExtent.height,
                                             capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

}