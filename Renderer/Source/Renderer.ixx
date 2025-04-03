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
import VkglTFModel;
import DeviceMod;
import SwapChainMod;
import ConfigMod;
import UIMod;
import ConfigMod;
import LightMod;
import FrameBufferMod;

const int LIGHT_COUNT = 3;
struct ShadowSettings
{
	bool enableShadows = true;
	float lightFOV = 100.0f;

	float depthBiasConstant = 1.25f;
	float depthBiasSlope = 1.75f;
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




struct VkQueues
{
	VkQueue graphicsQueue{ nullptr };
	VkQueue presentQueue{ nullptr };
};


// 应该是键值对 使用map
struct NeededFeatures
{
	bool validation{ false };
	VkBool32 sampleRateShading{ false };
	VkBool32 samplerAnisotropy{ false };

};


struct Semaphores
{
	VkSemaphore presentComplete;
	VkSemaphore renderComplete;
};

struct UniformDataOffscreen
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
	alignas(16) int layer{ 0 };
};

// This UBO stores the shadow matrices for all of the light sources
// The matrices are indexed using geometry shader instancing
// The instancePos is used to place the models using instanced draws
struct UniformDataShadows
{
	glm::mat4 mvp[LIGHT_COUNT];
	glm::vec4 instancePos[3];
};

struct UniformDataComposition
{
	glm::vec4 viewPos;
	Light lights[LIGHT_COUNT];
	uint32_t useShadows = 1;
	int32_t debugDisplayTarget = 0;
};
struct UniformBuffers
{
	Buffer offscreen;
	Buffer composition;
	Buffer shadowGeometryShader;
};

struct Pipelines
{
	VkPipeline offscreen{ VK_NULL_HANDLE };
	VkPipeline composition{ VK_NULL_HANDLE };
	VkPipeline shadowpass{ VK_NULL_HANDLE };
};

struct DescriptorSets
{
	VkDescriptorSet model{ VK_NULL_HANDLE };
	VkDescriptorSet composition{ VK_NULL_HANDLE };
	VkDescriptorSet shadow{ VK_NULL_HANDLE };
};

struct RenderPass
{
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkRenderPass renderPass;
	Framebuffer frameBuffer;
	std::vector<Buffer> buffers;
	VkCommandBuffer commandBuffer;
};

struct PipelineLayouts
{
	VkPipelineLayout model;
	VkPipelineLayout composition;
};
struct DescriptorSetLayouts
{
	VkDescriptorSetLayout model{ nullptr };
	VkDescriptorSetLayout composition{ nullptr };
	VkDescriptorSetLayout textures{ nullptr };
};
struct Framebuffers
{
	// Framebuffer resources for the deferred pass
	Framebuffer* deferred{ nullptr };
	// Framebuffer resources for the shadow pass
	Framebuffer* shadow{ nullptr };
};

export class Renderer
{
public:
	Renderer(Config* config);
	void Run();
	~Renderer() = default;

private:

	Config* m_config;
	std::vector<VkFence> m_waitFences;
	float m_timer;
	float m_timerSpeed = 0.25f;
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
	vkglTF::Model m_glTFModel;


	Camera m_camera;
	uint32_t currentBuffer = 0;
	VkPipelineStageFlags m_submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo m_submitInfo{};
	Semaphores m_semaphores{};
	std::vector<VkShaderModule> m_shaderModules{};
	VkFormat m_depthFormat{};
	Buffer m_uboBuffer{};
	VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
	VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorPool m_descriptorPool{ nullptr };
	VkDescriptorSet m_descriptorSet{ nullptr };
	VkDescriptorSet m_texturesDescriptorSet{ nullptr };
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

	void PrepareFrame();
	void PreCreateSubmitInfo();
	void UpdateUniformBuffers();
	void EncapsulationDevice();
	void LoadAssets();

	void DrawUI(const VkCommandBuffer commandBuffer);
	void InitUI();
	/// @brief 设置ui
	/// @param overlay 
	void DisplayUI(UIOverlay* overlay);
	void UpdateOverlay();
	std::string GetWindowTitle()const;

	//std::string GetAssetsPath();
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void MouseCallback(GLFWwindow* window, double xpos, double ypos);
	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
	void ResizeWindow();
	void SetEnabledFeatures();
	VkPipelineCache m_pipelineCache{ VK_NULL_HANDLE };
	void CreatePipelineCache();
	VkPhysicalDeviceFeatures m_deviceFeatures{};

	void Draw();
	void SubmitFrame();
private:
	struct Defered
	{
		VkSampler colorSampler{ VK_NULL_HANDLE };
		VkSemaphore offscreenSemaphore{ VK_NULL_HANDLE };
		VkCommandBuffer offScreenCmdBuffer{ VK_NULL_HANDLE };
		PipelineLayouts pipelineLayouts;
		Pipelines pipelines;
		UniformBuffers uniformBuffers;
		UniformDataComposition uniformDataComposition;
		UniformDataOffscreen uniformDataOffscreen;
		DescriptorSets descriptorSets;
		DescriptorSetLayouts descriptorSetLayouts;
		VkDescriptorSetLayout descriptorSetLayout;
		int32_t debugDisplayTarget = 0;
	};
	UniformBuffers m_uniformBuffers;
	DescriptorSetLayouts m_descriptorSetLayouts;
	DescriptorSets m_descriptorSets;
	Defered m_defered;
	void PrepareOffscreenFramebuffer();
	void PrepareUniformBuffers();
	void SetupDescriptors();
	void PreparePipelines();
	void BuildCommandBuffers();
	void BuildDeferredCommandBuffer();

	void UpdateUniformBufferOffscreen();
	void UpdateUniformBufferComposition();

	;
	Framebuffers m_framebuffers{ nullptr,nullptr };
	void SetupShadow();
	void SetupDefered();
	void InitLights();

	void SetupDescriptorsDD();

};

