module;
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

module RendererMod;

void Renderer::Run()
{
	InitWindow();
	InitVulkan();
	MainLoop();
	Cleanup();
}
// 得到窗口大小改变的信息
void Renderer::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	// 得到从window传递的this指针
	auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	app->m_framebufferResized = true;
}
/// @brief 初始化窗口
void Renderer::InitWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	// 使用glfw创建窗口
	m_window = glfwCreateWindow(m_width, m_height, "Renderer", nullptr, nullptr);
	// 传递对象this指针给回调函数。这样我们就可以在回调函数中访问类的成员变量, 也就是传递窗口变化信息
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
}
/// @brief 初始化 Vulkan，包括相关设置
void Renderer::InitVulkan()
{
	CreateInstance();
	SetupDebugMessenger(); //先创建实例，相当于先指定和调试的接口，再之后再实际链接
	CreateSurface();
	PickPhysicalDevice();
	//CreateLogicalDevice();
}
/// @brief 1. 检查是否支持验证层 2. 获取需要的扩展（glfw + valid），好像没有检查扩展是否可用
void Renderer::CreateInstance()
{
	// 如果要使用验证层而系统不支持，则抛出异常
	if (m_neededFeatures.validation && !CheckValidationLayerSupport())
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}

	VkApplicationInfo appIF{};
	appIF.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appIF.pApplicationName = "Renderer";
	appIF.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appIF.pEngineName = "No Engine";
	appIF.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appIF.apiVersion = VK_API_VERSION_1_0;
	appIF.pNext = nullptr;

	VkInstanceCreateInfo createIF{};
	createIF.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createIF.pApplicationInfo = &appIF;

	auto extensions = GetRequiredExtensions();

	// 专指扩展数量，创建时会检查扩展是否可用
	createIF.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createIF.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (m_neededFeatures.validation)
	{
		//enabledLayerCount vulkan所需的验证层数量
		createIF.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
		createIF.ppEnabledLayerNames = m_validationLayers.data();

		// 这里是动态创建，先创建实例，再创建调试回调函数，然后再创建将他们链接
		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createIF.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else
	{
		createIF.enabledLayerCount = 0;
		createIF.pNext = nullptr;
	}

	if (auto result = vkCreateInstance(&createIF, nullptr, &m_instance); result != VK_SUCCESS)
	{
		throw std::runtime_error(std::string("failed to create instance! result:") + std::to_string(result));
	}

}
//获取所需的 Vulkan 实例扩展。 包括所需的glfw扩展和可选的验证层扩展
std::vector<const char*> Renderer::GetRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions{ nullptr };
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	//验证层本身不是扩展，但某些调试功能（如 VK_EXT_debug_utils）需要同时启用特定扩展才能工作！
	// 所以这里加入了 调试扩展
	if (m_neededFeatures.validation)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	std::cout << "RequiredExtensions: " << extensions.size() << std::endl;
	// VK_KHR_surface // 窗口渲染所需的扩展
	// VK_KHR_win32_surface //特定于windows 窗口渲染所需的扩展，是对VK_KHR_surface的进一步扩展
	// Vk_EXT_debug_utils
	return extensions;
}

void Renderer::SetRequiredFeatures()
{

}
void Renderer::GetDeviceProperties()
{
	uint32_t layerCount{};
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr); //获取系统中所有可用的Vulkan实例层的总数
	std::cout << "system available layerCount: " << layerCount << std::endl;

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());//实际填充 availableLayers 向量中的每个元素，每个元素包含一个Vulkan实例层的属性信息。

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions{ nullptr };
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
}
/// @brief 检查系统中是否支持验证层
bool Renderer::CheckValidationLayerSupport()
{
	uint32_t layerCount{};
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr); //获取系统中所有可用的Vulkan实例层的总数
	std::cout << "system available layerCount: " << layerCount << std::endl;

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());//实际填充 availableLayers 向量中的每个元素，每个元素包含一个Vulkan实例层的属性信息。


	// 检查 validationLayers 中的每个层是否都在 availableLayers 中
	// 验证层是否在所有可行层中
	for (const char* layerName : m_validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (std::strcmp(layerName, layerProperties.layerName) == 0)
			{
				std::cout << "layerName: " << layerName << " Available" << std::endl;
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

/// @brief 设置 createInfo 的信息，包括回调函数
/// @param createInfo 
void Renderer::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
}
VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{

	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}
/// @brief 设置Vulkan实例的调试回调函数
void Renderer::SetupDebugMessenger()
{
	if (!m_neededFeatures.validation) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to set up debug messenger!");
	}
}
VkResult Renderer::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	// 这是做了cast？这个函数在扩展中提供，所以需要GetAddr
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

/// @brief  创建surface,surface是对窗口的抽象
void Renderer::CreateSurface()
{
	// 调用glfw的函数创建surface，方便 //需要定义 glfw include vulkan宏
	if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create window surface!");
	}
}
VkSampleCountFlagBits Renderer::GetMaxUsableSampleCount()
{
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & VK_SAMPLE_COUNT_64_BIT)
	{
		return VK_SAMPLE_COUNT_64_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_32_BIT)
	{
		return VK_SAMPLE_COUNT_32_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_16_BIT)
	{
		return VK_SAMPLE_COUNT_16_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_8_BIT)
	{
		return VK_SAMPLE_COUNT_8_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_4_BIT)
	{
		return VK_SAMPLE_COUNT_4_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_2_BIT)
	{
		return VK_SAMPLE_COUNT_2_BIT;
	}

	return VK_SAMPLE_COUNT_1_BIT;
}
// 选取物理设备
void Renderer::PickPhysicalDevice()
{
	uint32_t deviceCount{ 0 };
	// 获取支持vulkan的gpu，经典两段式，先获取数量，再填充数据
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	std::cout << "support deviceCount: " << deviceCount << std::endl;
	if (deviceCount == 0)
	{
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	// 选择合适的 device
	for (const auto& device : devices)
	{
		if (IsDeviceSuitable(device))
		{
			m_physicalDevice = device;

			// 获取支持的采样数
			VkPhysicalDeviceProperties deviceProperties;
			m_msaaSamples = GetMaxUsableSampleCount();

			// 获取支持的各向异性
			vkGetPhysicalDeviceProperties(device, &deviceProperties);
			m_maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;
			break;
		}
	}
	//FindQueueFamilies(mPhysicalDevice);
	if (m_physicalDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("failed to find a suitable GPU!");
	}
}

/// @brief 寻找需要的队列族
/// @param device 
/// @return 
QueueFamilyIndices Renderer::FindQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	// std::cout << "device queueFamilyCount: " << queueFamilyCount << std::endl;

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	VkBool32 presentSupport = false;
	for (int i = 0; const auto & queueFamily : queueFamilies)
	{
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) // queueflags。按位与，如果有相同位为1，则true
		{
			indices.graphicsFamily = i;
		}

		//用于查询物理设备（VkPhysicalDevice）的某个队列族（queue family）是否支持特定的surface（VkSurfaceKHR）的呈现（presentation）。
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
		if (presentSupport)
		{
			indices.presentFamily = i;
		}
		// break; 多此一举？没有多此一举，complete里面还有
		if (indices.IsComplete())
		{
			break;
		}
		++i;
	}
	return indices;
}

/// @brief 检查设备是否支持所有所需的扩展、设备扩展和vulkan实例扩展是不同的
/// @param device 
/// @return 
bool Renderer::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	// 所有可用的扩展
	uint32_t extensionCount{};
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

	// 从需求的扩展中删除可用的扩展，如果结果为空，说明所有需求的扩展都可用
	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

/// @brief 判断某个device是否合适 // 支持队列族、交换链扩展、交换链合适、且支持各向异性采样
/// @param device 
/// @return 
bool Renderer::IsDeviceSuitable(VkPhysicalDevice device)
{
	// 支持队列族（渲染和显示）
	QueueFamilyIndices indices = FindQueueFamilies(device);
	if (!indices.IsComplete())return false;

	// 支持设备扩展（这里只有swapchain）
	bool extensionsSupported = CheckDeviceExtensionSupport(device);
	if (!extensionsSupported)return false;

	// 不止支持交换链扩展，还需要交换链合适
	bool swapChainAdequate = false;
	// 交换链必须支持至少一种format和一种presentmode
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
	swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	if (!swapChainAdequate)return false;

	// 且支持各向异性采样
	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return supportedFeatures.samplerAnisotropy;
}

/// @brief 查询并返回交换链支持的细节信息，如format，和present mode 数组
SwapChainSupportDetails Renderer::QuerySwapChainSupport(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
	}
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
	}
	return details;
}
// void Renderer::MainLoop()
//{
//	while (!glfwWindowShouldClose(m_window))
//	{
//		glfwPollEvents();
//		DrawFrame();
//	}
//	vkDeviceWaitIdle(m_device);
// }
void Renderer::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

void Renderer::MainLoop()
{
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
		//DrawFrame();
	}
	vkDeviceWaitIdle(m_device);
}

void Renderer::Cleanup()
{
	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);

	vkDestroyDevice(m_device, nullptr);
	if (m_neededFeatures.validation)
	{
		DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
	}
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyInstance(m_instance, nullptr);

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

