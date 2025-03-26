module;
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
export module RendererMod;
import std;
import BufferMod;
import CameraMod;
import GLTFModelMod;
import DeviceMod;
import SwapChainMod;
import ConfigMod;
import UIMod;
//vulkanExample = new VulkanExample();															\
//vulkanExample->initVulkan();																	\
//vulkanExample->setupWindow(hInstance, WndProc);													\
//vulkanExample->prepare();																		\
//vulkanExample->renderLoop();

struct DescriptorSetLayouts
{
	VkDescriptorSetLayout Matrices{ nullptr };
	VkDescriptorSetLayout Textures{ nullptr };
};

struct UBOMatrices
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec3 lightPos;
	glm::vec3 camPos;
};

struct UBOLights
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec3 intensity;
};

struct ConstBuffers
{
	Buffer uboBuffer;
	Buffer lightBuffer;

	UBOMatrices uboBufferData;
	UBOLights lightBufferData;
};

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	bool IsComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct FrameBufferAttachment
{
	VkImage image{ nullptr };
	VkImageView view{ nullptr };

	~FrameBufferAttachment()
	{
		// TODO
	}
};

struct VkQueues
{
	VkQueue graphicsQueue{ nullptr };
	VkQueue presentQueue{ nullptr };
};

struct Limits
{
	bool validation{ false };
	float maxAnisotropy{ 0 };
	VkSampleCountFlagBits maxMsaaSamples{ VK_SAMPLE_COUNT_1_BIT };

};
// 应该是键值对 使用map
struct NeededFeatures
{
	bool validation{ false };
	VkBool32 sampleRateShading{ false };
	VkBool32 samplerAnisotropy{ false };

};

struct Image
{
	VkImage image;
	void* mapped;
};

struct Semaphores
{
	VkSemaphore presentComplete;
	VkSemaphore renderComplete;
};

export class Renderer
{
public:
	Renderer(bool enableValidation = false);
	void Run();
	~Renderer() = default;

private:
	float m_timer;
	float m_timerSpeed;
	uint32_t m_frameCounter = 0;
	uint32_t m_lastFPS = 0;
	float m_frameTimer{};
	std::chrono::time_point<std::chrono::high_resolution_clock> m_lastTimestamp, m_tPrevEnd;
	std::string m_title = "Vulkan Example";
	bool click{ false };
	struct
	{
		struct
		{
			bool Left = false;
			bool Right = false;
			bool Middle = false;
		} Buttons;
		glm::vec2 Position;
	} m_mouseState;

	UIOverlay m_UI;
	DescriptorSetLayouts m_descriptorSetLayouts;
	VkPhysicalDeviceFeatures m_enabledFeatures{};
	/** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
	std::vector<const char*> m_enabledDeviceExtensions;
	std::vector<const char*> m_enabledInstanceExtensions;
	/** @brief Optional pNext structure for passing extension structures to device creation */
	void* m_deviceCreatepNextChain = nullptr;


	VulkanDevice* m_vulkanDevice;
	GLTFModel m_glTFModel;


	Camera m_camera;
	uint32_t currentBuffer = 0;
	VkPipelineStageFlags m_submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo m_submitInfo{};
	Semaphores m_semaphores{};
	std::vector<VkShaderModule> m_shaderModules{};
	VkFormat m_depthFormat{};
	Buffer m_uboBuffer{};
	UBOMatrices m_uboMatrices{};
	VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
	VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorPool m_descriptorPool{ nullptr };
	VkDescriptorSet m_descriptorSet{ nullptr };
	VkPipeline m_pipeline{ nullptr };
	VkRenderPass m_renderPass{ nullptr };

	std::vector<VkFramebuffer> m_frameBuffers;
	VkCommandPool m_commandPool;
	VkCommandBuffer m_commandBuffer;

	VkDevice m_device{ nullptr };
	VkQueues m_queues{};

	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkInstance m_instance{ nullptr };

	// 一个回调对象，用于通过 Vulkan 调试扩展（如 VK_EXT_DEBUG_UTILS）接收 Vulkan 驱动程序生成的调试信息。在应用程序结束时，你需要销毁这个对象，释放资源
	VkDebugUtilsMessengerEXT m_debugMessenger{ nullptr };
	VkSurfaceKHR m_surface{ nullptr }; // 对window的抽象
	GLFWwindow* m_window{ nullptr };
	//VkSwapchainKHR m_swapChain{ nullptr };
	VulkanSwapChain m_swapChain{};
	SwapChainSupportDetails m_swapChainSupport;

	bool m_framebufferResized = false;
	uint32_t m_width = 1600;
	uint32_t m_height = 1200;
	int m_inFlight = 2;
	//bool m_enableValidation{ false };
	Limits m_limits{};
	NeededFeatures m_neededFeatures{};
	VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	float m_maxAnisotropy;
	QueueFamilyIndices m_indices;
	std::vector<VkCommandBuffer> m_drawCmdBuffers{};

	struct
	{
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
	} m_depthStencil{};

	const std::vector<const char*> m_validationLayers = {
		"VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> m_deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	void CreateInstance();
	void InitWindow();
	void InitVulkan();
	void SetupDebugMessenger();
	void MainLoop();
	void Cleanup();
	void GetDeviceProperties();
	void SetRequiredFeatures();
	bool CheckValidationLayerSupport();
	std::vector<const char*> GetRequiredExtensions();

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void PickPhysicalDevice();
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
	bool IsDeviceSuitable(VkPhysicalDevice device);
	void CreateLogicalDevice();
	void CreateSurface();
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	void CreateSwapChain();
	void CreateSwapChainImageViews();
	void CreateGraphicsPipeline();

	void CreateRenderPass();
	void CreateFramebuffers();
	void CreateCommandPool();
	VkResult CreateBuffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer* buffer, VkDeviceSize size, void* data = nullptr);
	void CreateCommandBuffers();
	//void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void DrawFrame();
	void CreateSyncObjects();

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

	void CreateDepthResources();
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat FindDepthFormat();



	//void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	VkSampleCountFlagBits GetMaxUsableSampleCount();
	void CreateDescriptors();

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	void CreateUniformBuffer();
	//std::string GetShadersPath();
	VkPipelineShaderStageCreateInfo LoadShader(std::string fileName, VkShaderStageFlagBits stage);
	void BuildCommandBuffers();
	void PrepareFrame();
	void PreCreateSubmitInfo();
	void UpdateUniformBuffers();
	void LoadglTFFile(std::string filename);
	void EncapsulationDevice();
	void LoadAssets();

	void DrawUI(const VkCommandBuffer commandBuffer);
	void InitUI();
	void DisplayUI(UIOverlay* overlay);
	void UpdateOverlay();
	std::string GetWindowTitle()const;

	//std::string GetAssetsPath();
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void MouseCallback(GLFWwindow* window, double xpos, double ypos);
	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
};

