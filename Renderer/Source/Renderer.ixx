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
//vulkanExample = new VulkanExample();															\
//vulkanExample->initVulkan();																	\
//vulkanExample->setupWindow(hInstance, WndProc);													\
//vulkanExample->prepare();																		\
//vulkanExample->renderLoop();

struct UBOMatrices
{
	glm::mat4 view;
	glm::mat4 proj;

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
	VkImage image;
	VkImageView view;

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


//struct SwapChain
//{
//	std::vector<VkImage> images{};
//	std::vector<VkImageView> views{};
//	VkFormat swapChainImageFormat{};
//	VkExtent2D swapChainExtent{};
//
//	VkSwapchainKHR swapChain{ VK_NULL_HANDLE };
//};

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

struct Vertex
{
	float pos[3];
	float color[3];
	//float uv[2];
	//float normal[3];
};

export class Renderer
{
public:
	Renderer(bool enableValidation = false);
	void Run();
	~Renderer()
	{
	}

	// Index buffer
	struct
	{
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkBuffer buffer{ VK_NULL_HANDLE };
		uint32_t count{ 0 };
	} indices;

	struct
	{
		VkDeviceMemory memory{ VK_NULL_HANDLE }; // Handle to the device memory for this buffer
		VkBuffer buffer{ VK_NULL_HANDLE };		 // Handle to the Vulkan buffer object that the memory is bound to
	} vertices;

private:


	VkPhysicalDeviceFeatures m_enabledFeatures{};
	/** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
	std::vector<const char*> m_enabledDeviceExtensions;
	std::vector<const char*> m_enabledInstanceExtensions;
	/** @brief Optional pNext structure for passing extension structures to device creation */
	void* m_deviceCreatepNextChain = nullptr;


	VulkanDevice* m_vulkanDevice;
	GLTFModel m_glTFModel;
	uint32_t m_frameCounter = 0;
	uint32_t m_lastFPS = 0;

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
	uint32_t m_width = 800;
	uint32_t m_height = 600;
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
	VkShaderModule CreateShaderModule(const std::vector<char>& code);
	static std::vector<char> ReadFile(const std::string& filename);
	void CreateRenderPass();
	void CreateFramebuffers();
	void CreateCommandPool();
	VkResult CreateBuffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer* buffer, VkDeviceSize size, void* data = nullptr);
	void CreateCommandBuffers();
	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void DrawFrame();
	void CreateSyncObjects();
	void RecreateSwapChain();
	void CleanupSwapChain();
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void CreateDescriptorSetLayout();
	void CreateUniformBuffers();
	void UpdateUniformBuffer(uint32_t currentImage);
	void CreateDescriptorPool();
	void CreateDescriptorSets();
	void CreateTextureImage();
	void CreateTextureImageView();
	void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

	VkCommandBuffer BeginSingleTimeCommands();

	void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	void CreateTextureSampler();

	void CreateDepthResources();
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat FindDepthFormat();
	bool HasStencilComponent(VkFormat format);

	void LoadModel();

	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	VkSampleCountFlagBits GetMaxUsableSampleCount();
	void CreateDescriptors();
	void CreateColorResources();
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	void CreateUniformBuffer();
	std::string GetShadersPath();
	VkPipelineShaderStageCreateInfo LoadShader(std::string fileName, VkShaderStageFlagBits stage);
	void BuildCommandBuffers();
	void PrepareFrame();
	void PreCreateSubmitInfo();
	void UpdateUniformBuffers();
	void LoadglTFFile(std::string filename);
	void EncapsulationDevice();
	void LoadAssets();
	std::string GetAssetsPath();
};

