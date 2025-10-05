module;

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
export module RendererMod;

import BufferMod;

import VkglTFModel;
import DeviceMod;
import SwapChainMod;
import ConfigMod;
import UIMod;
import LightMod;
import FrameBufferMod;
// RHI imports for testing
// import Core;
// import RhiDevice;
// import VulkanFactory;
import TextureMod;
import SettingMod;
//import MeshOctreeMod;
import GlmMod;
import InitMod;
import ToolMod;
import std;
// import VoxelOctreeMod;
import CameraMod;
import GPUMipmapOctreeMod;
import MeshToSdf;



const int LIGHT_COUNT = 3;
struct ShadowSettings
{
	bool enableShadows = true;
	float lightFOV = 100.0f;

	float depthBiasConstant = 2.0f;
	float depthBiasSlope = 2.0f;

	float zNear = 0.1f;
	float zFar = 64.0f;
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

struct MouseState
{
	struct
	{
		bool Left = false;
		bool Right = false;
		bool Middle = false;
	} Buttons;
	glm::vec2 Position;
};

struct UBOBlurParams
{
	float blurScale = 1.0f;
	float blurStrength = 1.5f;
};
struct Params
{
	float exposure{ 4.5f };
	float gamma{ 2.2f };
};
struct FXAAParams
{
	alignas(8) glm::vec2 rcpFrame{ 1.0f, 1.0f };
	alignas(8) glm::vec2 sth{ 0.125f, 1.0f };
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
	VkSemaphore deferedSemaphore{ nullptr };
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

struct UniformDataSkybox
{
	alignas(8) glm::vec2 resolution;
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 projection;
};

struct UniformBuffers
{
	Buffer defered;
	Buffer shadowGeometryShader;
	Buffer skyBox;
	Buffer postParam;
	Buffer blurParams;
	Buffer FXAA;
};

struct Pipelines
{
	VkPipeline defered{ VK_NULL_HANDLE };
	VkPipeline composition{ VK_NULL_HANDLE };
	VkPipeline shadow{ VK_NULL_HANDLE };
	VkPipeline skyBox{ VK_NULL_HANDLE };
	VkPipeline toneMapping{ VK_NULL_HANDLE };
	VkPipeline blurVert{ VK_NULL_HANDLE };
	VkPipeline blurHorz{ VK_NULL_HANDLE };
	VkPipeline FXAA{ nullptr };
};

struct DescriptorSets
{
	VkDescriptorSet deferedModel{ VK_NULL_HANDLE };
	VkDescriptorSet composition{ VK_NULL_HANDLE };
	VkDescriptorSet shadow{ VK_NULL_HANDLE };
	VkDescriptorSet skyBox{ VK_NULL_HANDLE };
	VkDescriptorSet toneMapping{ nullptr };
	VkDescriptorSet blurVert{ nullptr };
	VkDescriptorSet blurHorz{ nullptr };
	VkDescriptorSet FXAA{ nullptr };
};

struct Textures
{
	TextureCubeMap environmentCube;
	// Generated at runtime
	Texture2D lutBrdf;
	TextureCubeMap irradianceCube;
	TextureCubeMap prefilteredCube;
};
struct SmallScene
{
	vkglTF::Model skybox;
	std::vector<vkglTF::Model> objects;
	int32_t objectIndex = 0;
	Textures textures;
	UniformDataSkybox uniformDataSkybox;
};
struct RenderPass
{
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkRenderPass renderPass;
	FramebufferManager frameBuffer;
	std::vector<Buffer> buffers;
	VkCommandBuffer commandBuffer;
};

struct PipelineLayouts
{
	VkPipelineLayout defered;
	VkPipelineLayout composition{ nullptr };
	VkPipelineLayout skyBox;
	VkPipelineLayout shadow;
	VkPipelineLayout toneMapping;
	VkPipelineLayout blur;
	VkPipelineLayout FXAA;
};
struct DescriptorSetLayouts
{
	VkDescriptorSetLayout deferedModel{ nullptr };
	VkDescriptorSetLayout deferedTextures{ nullptr };
	VkDescriptorSetLayout composition{ nullptr };
	VkDescriptorSetLayout skyBox{ nullptr };
	VkDescriptorSetLayout toneMapping{ nullptr };
	VkDescriptorSetLayout blur{ nullptr };
	VkDescriptorSetLayout FXAA{ nullptr };
};
struct Framebuffers
{
	// FramebufferManager resources for the deferred pass
	FramebufferManager* deferred{ nullptr };
	// FramebufferManager resources for the shadow pass
	FramebufferManager* shadow{ nullptr };
	FramebufferManager* lighting{ nullptr };
	FramebufferManager* bloom{ nullptr };
	FramebufferManager* bloom1{ nullptr };
	FramebufferManager* ToneMapping{ nullptr };
	FramebufferManager* SkyBox{ nullptr };
};

struct CmdBuffers
{
	VkCommandBuffer offScreenCmdBuffer;
};

export class Renderer
{
public:
	Renderer(Config* config);
	void Run();
	~Renderer() = default;

private:
	struct
	{
		UBOBlurParams blurParams;
	} m_ubos;

	FXAAParams m_FXAAParams{};
	Params m_postParams{};
	std::vector<VkFramebuffer> m_finalFramebuffers;
	VkRenderPass m_finalPass;
	SmallScene scene;
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

	MouseState m_mouseState;

	UIOverlay m_UI;
	VkPhysicalDeviceFeatures m_enabledFeatures{};
	/** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
	std::vector<const char*> m_enabledDeviceExtensions;
	std::vector<const char*> m_enabledInstanceExtensions;
	/** @brief Optional pNext structure for passing extension structures to device creation */
	void* m_deviceCreatepNextChain = nullptr;

	OldVulkanDevice* m_vulkanDevice;

	// RHI testing - new device alongside old one
	// Core::UniquePtr<RhiDevice> rhiDevice_;

	vkglTF::Model m_glTFModel;
	//std::unique_ptr<MeshOctree> m_meshOctree;

	Camera m_camera;
	uint32_t currentBuffer = 0;
	VkPipelineStageFlags m_submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo m_submitInfo{};

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
	// VkSwapchainKHR m_swapChain{ nullptr };
	VulkanSwapChain m_swapChain{};
	SwapChainSupportDetails m_swapChainSupport;

	bool m_framebufferResized = false;
	uint32_t m_width = 1280;
	uint32_t m_height = 1280;
	int m_inFlight = 2;
	// bool m_enableValidation{ false };
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
		VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_EXT_descriptor_indexing", "VK_EXT_conservative_rasterization", "VK_EXT_shader_viewport_index_layer" };

private:
	void SetupPasses();
	void CreateBuffers();
	void AllocateDescriptorSets();
	void PreparePipelines();
	void UpdateBuffers();
	void BuildDeferredCommandBuffer();
	void BuildCommandBuffers();

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

	void CreateDescriptorPool();
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
	void CreateSwapChainImageViews();

	void CreateRenderPass();
	void CreateFramebuffers();
	void CreateCommandPool();
	VkResult CreateBuffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer* buffer, VkDeviceSize size, void* data = nullptr);
	void CreateCommandBuffers();
	// void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void DrawFrame();
	void CreateSyncObjects();

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

	void CreateDepthResources();
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat FindDepthFormat();

	// void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	VkSampleCountFlagBits GetMaxUsableSampleCount();

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	// std::string GetShadersPath();
	VkPipelineShaderStageCreateInfo LoadShader(std::string fileName, VkShaderStageFlagBits stage);

	void PrepareFrame();
	void PreCreateSubmitInfo();

	void EncapsulationDevice();
	void LoadAssets();

	void DrawUI(const VkCommandBuffer commandBuffer);
	void InitUI();
	/// @brief 设置ui
	/// @param overlay
	void DisplayUI(UIOverlay* overlay);
	void UpdateOverlay();
	std::string GetWindowTitle() const;

	// std::string GetAssetsPath();
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
	ShadowSettings m_shadowSettings;
	UniformBuffers m_uniformBuffers;
	DescriptorSetLayouts m_descriptorSetLayouts;
	DescriptorSets m_descriptorSets;
	PipelineLayouts m_pipelineLayouts;
	Pipelines m_pipelines;
	VkCommandBuffer m_offScreenCmdBuffer{ VK_NULL_HANDLE };
	Semaphores m_semaphores{};

	UniformDataOffscreen m_uniformDataOffscreen;
	int32_t m_debugDisplayTarget = 0;

	void PrepareUniformBuffers();
	void SetupDescriptors();

	void UpdateUniformBufferOffscreen();

	Framebuffers m_framebuffers{ nullptr, nullptr };
	void SetupShadow();
	void SetupDeferedPass();
	// Post
	void SetupSkyBoxPass();
	void SetupFinalPass();
	void SetupBloomPass();
	void SetupToneMappingPass();



	// PBR
	void GenerateBRDFLUT();
	void GenerateIrradianceCube();
	void GeneratePrefilteredCube();
	void UpdateUniformBufferPost();
	void UpdateUniformBuffersBlur();
	void UpdateUniformBufferFXAA();
	struct PostSettings
	{
		bool bloom{ true };
		bool FXAA{ true };
	} m_postSettings;

	void SetupBlurDescriptorSets();

	struct QueueIndex
	{
		uint32_t graphics;
		uint32_t compute;
	} m_index;
	// Compute
private:
	struct SpotLightShadowTex
	{
		Texture Tex;
		VkFramebuffer Framebuffer;
	};
	struct SpotLightShadowPass
	{
		VkPipelineLayout PipelineLayout;
		VkPipeline Pipeline;
		VkRenderPass RenderPass;
		VkDescriptorSet Set;
		VkDescriptorSetLayout SetLayout;
		std::vector<SpotLightShadowTex> Shadows;
		VkSampler Sampler;
		Buffer CBuffer;
		struct CBufferDesc
		{
			glm::mat4 mvp;
		};
		std::vector<CBufferDesc> CBufferData;
		FramebufferAttachment Depth;
	} m_spotLightPass;
	uint32_t m_shadowSpotLightCount{ 0 };
	void CreateBuffersSpotLight();
	void CreateBuffersSpotLightShadow();
	void AllocateDescriptorSetSpotLightShadow();
	void UpdateCBufferSpotLight();
	void PreparePilineSpotLightShadow();

private:
	struct CubeMap
	{
		Texture Tex;
		std::array<VkImageView, 6> FaceViews = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };
		VkImageView framebufferView = VK_NULL_HANDLE; // 2D数组视图，用于framebuffer附件
	};
	struct ShadowOmniPass
	{
		// vector 每6个
		static const int WIDTH{ 1280 };
		static const int HEIGHT{ 1280 };
		std::vector<std::array<VkFramebuffer, 6>> FrameBuffers;
		std::vector<CubeMap> CubeMaps;

		VkPipelineLayout PipelineLayout{ nullptr };
		VkPipeline Pipeline{ nullptr };
		VkDescriptorSetLayout SetLayout{ VK_NULL_HANDLE };
		VkDescriptorSet Set{ VK_NULL_HANDLE };
		// depth 共用
		FramebufferAttachment Depth;
		VkRenderPass RenderPass;
		VkSampler Sampler;
		Buffer CBuffer;
		Buffer LightMatricesBuffer;
		struct PushBlockDesc
		{
			glm::mat4x4 view;
			uint32_t index;
			uint32_t pad1;
			uint32_t pad2;
			uint32_t pad3;
		};
		struct CBufferDesc
		{
			glm::mat4 Proj;
			glm::mat4 View;
			glm::mat4 Model;
			glm::vec4 Pos;
		};
		std::vector<CBufferDesc> CBufferData;
	} m_shadowOmniPass;
	// CubeMap
	void CreateBuffersPointLights();
	void SetupPassShadowOmni();
	void AllocateDescriptorSetShadowOmni();
	void CreateBuffersShadowOmni();
	void PreparePipelineShadowOmni();
	void UpdateCubeFace(uint32_t lightIndex, uint32_t faceIndex, VkCommandBuffer commandBuffer, VkDescriptorSet set);

private:
	struct LightCullingPass
	{
		static const uint32_t tileSize{ 16 };
		static const uint32_t MAX_LIGHTS_PER_TILE{ 10 };
		uint32_t queueFamilyIndex;				   // Used to check if compute and graphics queue families differ and require additional barriers
		VkQueue queue;							   // Separate queue for compute commands (queue family may differ from the one used for graphics)
		VkCommandPool commandPool;				   // Use a separate command pool (queue family may differ from the one used for graphics)
		VkCommandBuffer commandBuffer;			   // Command buffer storing the dispatch commands and barriers
		VkSemaphore semaphore;					   // Execution dependency between compute & graphic submission
		VkDescriptorSetLayout descriptorSetLayout; // Compute shader binding layout
		VkDescriptorSet descriptorSet;			   // Compute shader bindings
		VkPipelineLayout pipelineLayout;		   // Layout of the compute pipeline
		VkPipeline pipeline;					   // Compute pipeline for updating particle positions
		struct UniformComputeCullingData
		{
			glm::mat4x4 view;
			glm::mat4x4 proj;
			glm::vec2 screenSize;
			int tileSize;
			int numTilesX;
			int numTilesY;
			int numLights;
		};
		struct Tile
		{
			uint32_t lightCount{ 0 };
			uint32_t lightIndices[MAX_LIGHTS_PER_TILE];
		};
		struct LightCullingBuffers
		{
			Buffer lights;
			Buffer tiles;
			Buffer uniformBuffer;
			// std::vector<PointLight> pointLightsData;
			std::vector<Tile> tileData;
			UniformComputeCullingData uniformBufferData;
		};
		LightCullingBuffers buffers;
	};

	LightCullingPass m_compute;
	/// @brief 创建TilebasedLighting 相关
	void SetupTileBasedLightingPass();
	void PrepareLightCullingBuffers();
	void UpdateLightCullingUBO();
	void BuildTileBasedLightingCommandBuffer();

private:
	void OffscreenWork();
	void SetupPassDepthCubeMap();
	void RenderToCube(const vkglTF::Model& model, const glm::vec3& pos, const std::string& savePath);
	void SaveToImage(const Texture& tex, const std::string& savePath);
	void ExportSDFDataForVisualization(); // 导出SDF数据用于Python可视化验证
    void ExportSDFDataForVisualization(Texture* texture, VkImageLayout oldLayout,const std::string outputPath);
	struct DepthCubeMapPass
	{
		static const int WIDTH{ 512 };
		static const int HEIGHT{ 512 };

		CubeMap cubeMap;
		std::array<VkFramebuffer, 6> frameBuffer;

		VkPipelineLayout PipelineLayout{ nullptr };
		VkPipeline Pipeline{ nullptr };
		VkDescriptorSetLayout SetLayout{ VK_NULL_HANDLE };
		VkDescriptorSet Set{ VK_NULL_HANDLE };

		FramebufferAttachment Depth;
		VkRenderPass RenderPass;
		VkSampler Sampler;
		Buffer CBuffer;
		struct PushBlockDesc
		{
			glm::mat4x4 view;
			glm::mat4x4 model;
			glm::vec3 pos;
		};
		struct CBufferDesc
		{
			glm::mat4 Proj;
		};
		CBufferDesc CBufferData;
	} m_DepthCubePass;

private:
	struct SkyBoxPass
	{
		struct CBufferDesc
		{
		} CBufferData;
	};
	void PreparePipelineSkyBox();
	void UpdateCbufferSkyBox();
	void CreateBuffersSkyBox();
	void AllocateDescriptorSetSkyBox();

private:
	struct LightingPass
	{
		VkPipeline Pipeline;
		VkPipelineLayout PipelineLayout;
		struct CBufferDesc
		{
			alignas(16) glm::vec4 SplitDepth{ glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) };
			alignas(4) uint32_t DirLightCount;
			alignas(4) uint32_t ShadowDirCount;
			alignas(4) uint32_t PointLightCount;
			alignas(4) uint32_t ShadowPointCount;
			alignas(4) uint32_t SpotLightCount;
			alignas(4) uint32_t ShadowSpotCount;
			alignas(4) float MetallicFactor{ 1.0f };
			alignas(4) float RoughnessFactor{ 1.0f };
			alignas(4) uint32_t useShadows = 1;
			alignas(4) int32_t debugDisplayTarget = 0;
		} CBufferData;
		struct BuffersDesc
		{
			Buffer ConstBuffer;
		} Buffers;

	} m_lightingPass;
	void SetupLightingPass();
	void CreateBuffersLighting();
	void AllocateDescriptorSetLighting();
	void PreparePipelineLighting();
	void UpdateCBufferLighting();
	void UpdateDescritporSetLighting();

private:
	struct ToneMappingPass
	{
		struct CBufferDesc
		{
		} CBufferData;
	} m_toneMappingPass;

private:
	struct BloomPass
	{
		struct CBufferDesc
		{
			alignas(4) float Exposure{ 4.5f };
			alignas(4) float Gamma{ 2.2f };
		} CBufferData;
	} m_BloomPass;

	// Basic SSAO
private:
	Texture2D m_blueNoise;
	struct SSAOPass
	{
		FramebufferManager* frameBuffer{ nullptr };
		VkDescriptorSetLayout setLayout{ nullptr };
		VkDescriptorSet set{ nullptr };
		VkPipelineLayout pipelineLayout{ nullptr };
		VkPipeline pipeline{ nullptr };
		Texture2D noiseTexture;
		struct CBufferDesc
		{
			alignas(4) uint32_t sampleNum;
			alignas(4) float radius;
			alignas(4) float scale;
		};
		struct Buffers
		{
			Buffer ubo;
			Buffer kernels;
			CBufferDesc uboData;
		} buffers;
	} m_SSAOPass;
	void SetupSSAOPass();
	void CreateBuffersSSAO();
	void AllocateDescriptorSetSSAO();
	void PreparePipelineSSAO();
	void UpdateCBufferSSAO();
	void GenerateNoiseTextureSSAO();
	void GenerateSampleKernel();

private:
	class SdfAOPass
	{
	public:
		FramebufferManager* frameBuffer{ nullptr };
		VkDescriptorSetLayout setLayout{ nullptr };
		VkDescriptorSet set{ nullptr };
		VkPipelineLayout pipelineLayout{ nullptr };
		VkPipeline pipeline{ nullptr };
		Texture2D noiseTexture;
		struct CBufferDesc
		{
			alignas(4) uint32_t sampleCount;
			alignas(4) float sampleRadius;
			alignas(4) float aoStrength;
			alignas(4) float biasDistance;
			alignas(4) float maxDistance;
			alignas(4) float falloffPower;
			alignas(4) float voxelSize;
			alignas(4) uint32_t sdfTextureSize;
			alignas(16) glm::vec4 minBounds;
			alignas(16) glm::vec4 maxBounds;
			alignas(8) glm::vec2 noiseScale;
		};
		struct Buffers
		{
			Buffer cBuffer;
			CBufferDesc cBufferData;
		} buffers;

	} sdfAOPass_;

	void SetupSdfAOPass();
	void CreateBuffersSdfAO();
	void AllocateDescriptorSetSdfAO();
	void PreparePipelineSdfAO();
	void UpdateCBufferSdfAO();

private:
	struct HBAOPass
	{
		FramebufferManager* FrameBuffer{ nullptr };
		Texture2D NoiseTex;
		VkDescriptorSetLayout SetLayout;
		VkDescriptorSet Set;
		VkPipeline Pipeline;
		VkPipelineLayout PipelineLayout;
		struct ConstBufferDesc
		{
			alignas(4) float Radius;
			alignas(4) float Radius2;
			alignas(4) float NegInvR2;
			alignas(4) float Scale;
			alignas(4) int NumDirections;
			alignas(4) int NumSteps;
			alignas(4) float MaxRadiusPixels;
			alignas(4) float TanBias;
			alignas(4) float Strength;
			alignas(8) glm::vec2 UVToViewA;
			alignas(8) glm::vec2 UVToViewB;
			alignas(8) glm::vec2 LinMAD;
		} ConstBufferData;
		struct BuffersDesc
		{
			Buffer ConstBuffer;
		} Buffers;

	} m_HBAOPass;
	void SetupHBAOPass();
	void CreateBuffersHBAO();
	void AllocateDescriptorSetHBAO();
	void PreparePipelineHBAO();
	void UpdateCBufferHBAO();
	void GenerateNoiseTextureHBAO();

private:
	struct GTAOPass
	{
		FramebufferManager* FrameBuffer{ nullptr };
		Texture2D NoiseTex;
		VkDescriptorSetLayout SetLayout;
		VkDescriptorSet Set;
		VkPipeline Pipeline;
		VkPipelineLayout PipelineLayout;
		struct ConstBufferDesc
		{
			alignas(4) float Radius;
			alignas(4) float Scale;
			alignas(4) int NumDirections;
			alignas(4) int NumSteps;
			alignas(4) float MaxRadiusPixels;
			alignas(4) float Strength;
		} ConstBufferData;
		struct BuffersDesc
		{
			Buffer ConstBuffer;
		} Buffers;

	} m_GTAOPass;
	void SetupGTAOPass();
	void CreateBuffersGTAO();
	void AllocateDescriptorSetGTAO();
	void PreparePipelineGTAO();
	void UpdateCBufferGTAO();
	void GenerateNoiseTextureGTAO();

private:
	// 交叉双边滤波
	struct CrossBilateralFilterPass
	{
		FramebufferManager* FrameBufferX{ nullptr };
		FramebufferManager* FrameBufferY{ nullptr };
		VkDescriptorSetLayout SetLayout;
		VkDescriptorSet SetX;
		VkDescriptorSet SetY;
		VkPipeline PipelineX;
		VkPipeline PipelineY;
		VkPipelineLayout PipelineLayout;
		struct ConstBufferDesc
		{
			alignas(8) glm::vec2 Res;
			alignas(8) glm::vec2 InvRes;
			alignas(4) float KernelRadius;
			alignas(4) float ZFactor;
		} ConstBufferData;
		struct BuffersDesc
		{
			Buffer ConstBuffer;
		} Buffers;
	} m_CBFPass;
	void SetupCBFPass();
	void CreateBuffersCBF();
	void AllocateDescriptorSetCBF();
	void UpdateDescriptorSetCBF();
	void PreparePipelineCBF();
	void UpdateCBufferCBF();

private:
	struct CameraInfos
	{
		alignas(16) glm::mat4x4 view;
		alignas(16) glm::mat4x4 proj;
		alignas(16) glm::mat4x4 projView;
		alignas(16) glm::mat4x4 invView;
		alignas(16) glm::mat4x4 invProj;
		alignas(16) glm::mat4x4 invProjView;
		alignas(16) glm::vec4 cameraWorldPos;
		alignas(4) float zNear;
		alignas(4) float zFar;
		alignas(8) glm::vec2 screenSize;
		alignas(8) glm::vec2 invScreenSize; // = 1.0 / screenSize
	} m_cameraInfosData;
	struct SharedBuffers
	{
		Buffer ConstBufferCamera;
	} m_sharedBuffers;
	void UpdateCameraInfos();
	void CreateBufferCameraInfos();

	// Debug
private:
	PFN_vkCmdBeginDebugUtilsLabelEXT m_vkCmdBeginDebugUtilsLabelEXT = nullptr;
	PFN_vkCmdEndDebugUtilsLabelEXT m_vkCmdEndDebugUtilsLabelEXT = nullptr;
	void LoadDebugUtilsFunctions();
	void BeginDebugLabel(VkCommandBuffer cmd, const char* name, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);
	void EndDebugLabel(VkCommandBuffer cmd);

private:
	void CreateStorageBuffer(VkDeviceSize bufferSize);

	// CSM
private:
	static const uint32_t SHADOW_MAP_CASCADE_COUNT{ 4 };
	static const uint32_t SHADOWMAP_DIM{ 4096 };
	struct Cascade
	{
		VkFramebuffer frameBuffer;
		VkImageView view;
		float splitDepth;
		glm::mat4 viewProjMatrix;
		void destroy(VkDevice device)
		{
			vkDestroyImageView(device, view, nullptr);
			vkDestroyFramebuffer(device, frameBuffer, nullptr);
		}
	};

	struct CSMPass
	{
		VkRenderPass renderPass{ nullptr };
		VkPipelineLayout pipelineLayout{ nullptr };
		VkPipeline pipeline{ nullptr };
		FramebufferAttachment depth;
		std::array<Cascade, SHADOW_MAP_CASCADE_COUNT> cascades;
		std::vector<FramebufferAttachment> Depths;
		std::vector<std::array<Cascade, SHADOW_MAP_CASCADE_COUNT>> Cascades;
		struct CascadeDataDesc
		{
			alignas(16) glm::mat4 cascadeViewProjMatrices[SHADOW_MAP_CASCADE_COUNT];
		};
		std::vector<CascadeDataDesc> CascadeData;
		struct Buffers
		{
			Buffer cascadeViewProjMatricesBuffer;
			Buffer uboFS;
		} buffers;
		VkDescriptorSet set{ nullptr };
		VkDescriptorSetLayout setLayout{ nullptr };
		struct PushBlock
		{
			int lightIndex;
			uint32_t cascadeIndex;
		} pushBlock;
	} m_CSMPass;
	struct UBOFS
	{
		float cascadeSplits[4];
		glm::mat4 inverseViewMat;
		glm::vec3 lightDir;
		float _pad;
		int32_t colorCascades;
	} uboFS;
	void SetupCSMPass();
	void UpdateCascades();
	// void CreateLights();
	void AllocateDescriptorSetCSM();
	void UpdateUBOCSM();
	void PreparePipelineCSM();
	void CreateBuffersCSM();

private:
	Lights m_lights;
	struct DirLightMat
	{
		alignas(16) glm::mat4x4 Matrices[SHADOW_MAP_CASCADE_COUNT];
	};
	std::vector<DirLightMat> m_dirLightsMatrices;
	Buffer PointLightBuffer;
	Buffer SpotLightBuffer;
	Buffer DirLightBuffer;
	Buffer DirLightMatricesBufer;
	int m_shadowDirLightCount{ 0 };
	int m_shadowPointLightCount{ 0 };
	void InitLights();
	void CreateBuffersDirLights();
	void CreateLightsBuffers();

public:
	SettingInfo Settings;

private:
	struct DefalutTextures
	{
		Texture2D White;
		Texture2D Black;
		/*Texture2DArray WhiteArray;*/
	} m_defaultTextures;
	void CreateDefaultTextures();

private:
	// Voxelization Pass - Scan Line Fill Method
	struct VoxelizationPass
	{
		// GPU Resources
		Texture2D voxelCounterTexture;	  // R32_SINT format for atomic operations
		Texture3D finalVoxelStateTexture; // R32_UINT format for final result
		Buffer voxelUniformBuffer;		  // Uniform buffer for constants

		// Descriptor sets and layouts
		VkDescriptorSetLayout markPassDescriptorSetLayout{ VK_NULL_HANDLE };
		VkDescriptorSetLayout fillPassDescriptorSetLayout{ VK_NULL_HANDLE };
		VkDescriptorSet markPassDescriptorSet{ VK_NULL_HANDLE };
		VkDescriptorSet fillPassDescriptorSet{ VK_NULL_HANDLE };

		// Pipeline layouts and pipelines
		VkPipelineLayout markPassPipelineLayout{ VK_NULL_HANDLE };
		VkPipelineLayout fillPassPipelineLayout{ VK_NULL_HANDLE };
		VkPipeline markPassPipeline{ VK_NULL_HANDLE };
		VkPipeline fillPassPipeline{ VK_NULL_HANDLE };

		// Render pass for voxelization (no color attachments, just for pipeline creation)
		VkRenderPass renderPass{ VK_NULL_HANDLE };
		VkFramebuffer framebuffer{ VK_NULL_HANDLE };

		// Depth attachment for framebuffer
		VkImage depthImage{ VK_NULL_HANDLE };
		VkImageView depthImageView{ VK_NULL_HANDLE };
		VkDeviceMemory depthImageMemory{ VK_NULL_HANDLE };

		// Constants matching shader struct
		struct VoxelConstants
		{
			glm::mat4 model;
			glm::mat4 view;
			glm::mat4 projection;
			glm::uvec3 voxelGridSize;
		} constants;
	};
	VoxelizationPass m_voxelizationPass;
	std::vector<uint8_t> voxelData_;

	// GPU稀疏体素八叉树
	// GPU Mipmap隐式八叉树
	std::unique_ptr<GPUMipmapOctree> m_gpuMipmapOctree;
	bool m_enableVoxelization = true;

	// 统一GPU管线资源管理
	struct UnifiedGPUPipeline
	{
		// Command buffer (pre-recorded)
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkCommandPool commandPool = VK_NULL_HANDLE;

		// Synchronization objects
		VkFence executionFence = VK_NULL_HANDLE;
		VkSemaphore completionSemaphore = VK_NULL_HANDLE;

		// State flags
		bool resourcesInitialized = false;
		bool commandsRecorded = false;

		// Cleanup method
		void cleanup(VkDevice device)
		{
			if (completionSemaphore != VK_NULL_HANDLE)
			{
				vkDestroySemaphore(device, completionSemaphore, nullptr);
				completionSemaphore = VK_NULL_HANDLE;
			}
			if (executionFence != VK_NULL_HANDLE)
			{
				vkDestroyFence(device, executionFence, nullptr);
				executionFence = VK_NULL_HANDLE;
			}
			if (commandBuffer != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE)
			{
				vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
				commandBuffer = VK_NULL_HANDLE;
			}
			if (commandPool != VK_NULL_HANDLE)
			{
				vkDestroyCommandPool(device, commandPool, nullptr);
				commandPool = VK_NULL_HANDLE;
			}
			resourcesInitialized = false;
			commandsRecorded = false;
		}
	} m_unifiedGPUPipeline;

	// Analytical 的节点选择
	struct SolidNodeSelection
	{
		// Solid node data structure
		struct SolidNode
		{
			glm::vec3 center;	 // 世界坐标中心
			float size;			 // 立方体边长
			uint32_t level;		 // Mipmap层级
			uint32_t padding[3]; // 对齐到16字节
		};

        struct SolidNodeSelectionPushConstant
        {
            uint32_t BaseSize{128};
            uint32_t SampledLevel{0};
            glm::uvec2 Padding;
            alignas(16)glm::vec3 modelCenter;    // 模型中心
            float halfSizeWithMargin; // 包含边距的半尺寸
        };

		// GPU资源
		Buffer solidNodeBuffer; // 存储筛选出的solid nodes
		Buffer counterBuffer;	// 原子计数器，记录solid node数量
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

		// 参数
		static constexpr uint32_t MAX_SOLID_NODES = 1024; // 最大solid node数量
		uint32_t actualNodeCount = 0;					  // 实际筛选出的节点数量

		void cleanup(VkDevice device)
		{
			if (pipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, pipeline, nullptr);
				pipeline = VK_NULL_HANDLE;
			}
			if (pipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
				pipelineLayout = VK_NULL_HANDLE;
			}
			if (descriptorSetLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
				descriptorSetLayout = VK_NULL_HANDLE;
			}
			solidNodeBuffer.Destroy();
			counterBuffer.Destroy();
		}
	} m_solidNodeSelection;

	// 阶段三 (版本B)：自适应相机位置筛选 (Multi-Pass Adaptive Selection)
	struct SolidNodeSelectionB
	{
		// 复用版本A的节点结构体
		using SolidNode = SolidNodeSelection::SolidNode;

		// 临时保持原有字段以便向后兼容
		Buffer selectedNodesBuffer; // 最终选中的节点 (最多10个)
		Buffer selectedCountBuffer; // 最终节点计数
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

		// 新添加的多pass字段
		Buffer candidateNodesBuffer; // 候选节点缓冲区 (最多1000个)
		Buffer candidateCountBuffer; // 候选节点计数
		VkPipeline collectionPipeline = VK_NULL_HANDLE;
		VkPipelineLayout collectionPipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout collectionDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet collectionDescriptorSet = VK_NULL_HANDLE;

		// 最终选择管线字段
		VkPipeline finalSelectionPipeline = VK_NULL_HANDLE;
		VkPipelineLayout finalSelectionPipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout finalSelectionDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet finalSelectionDescriptorSet = VK_NULL_HANDLE;

		// 参数
		static constexpr uint32_t MAX_SELECTED_NODES = 10;	  // 最终节点限制
		static constexpr uint32_t MAX_CANDIDATE_NODES = 1000; // 候选节点容量
		uint32_t actualNodeCount = 0;						  // 实际选中的节点数量

		void cleanup(VkDevice device)
		{
			// 清理原有管线
			if (pipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, pipeline, nullptr);
				pipeline = VK_NULL_HANDLE;
			}
			if (pipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
				pipelineLayout = VK_NULL_HANDLE;
			}
			if (descriptorSetLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
				descriptorSetLayout = VK_NULL_HANDLE;
			}

			// 清理收集管线
			if (collectionPipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, collectionPipeline, nullptr);
				collectionPipeline = VK_NULL_HANDLE;
			}
			if (collectionPipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, collectionPipelineLayout, nullptr);
				collectionPipelineLayout = VK_NULL_HANDLE;
			}
			if (collectionDescriptorSetLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, collectionDescriptorSetLayout, nullptr);
				collectionDescriptorSetLayout = VK_NULL_HANDLE;
			}

			// 清理最终选择管线
			if (finalSelectionPipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, finalSelectionPipeline, nullptr);
				finalSelectionPipeline = VK_NULL_HANDLE;
			}
			if (finalSelectionPipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, finalSelectionPipelineLayout, nullptr);
				finalSelectionPipelineLayout = VK_NULL_HANDLE;
			}
			if (finalSelectionDescriptorSetLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, finalSelectionDescriptorSetLayout, nullptr);
				finalSelectionDescriptorSetLayout = VK_NULL_HANDLE;
			}

			// 清理缓冲区
			candidateNodesBuffer.Destroy();
			candidateCountBuffer.Destroy();
			selectedNodesBuffer.Destroy();
			selectedCountBuffer.Destroy();
		}
	} m_solidNodeSelectionB;

	// 版本控制：选择使用阶段三的哪个版本
	bool m_useSolidNodeSelectionB = false; // false: 使用版本A, true: 使用版本B

	// 阶段四：解析式SDF生成 (Analytical SDF Generation)
	struct AnalyticalSDFGeneration
	{
		// GPU资源
        Texture sdfTexture{}; // 64x64x64 SDF输出纹理
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

		// 参数
		static constexpr uint32_t MAX_CUBES = 20;	   // 限制处理20个立方体

		void cleanup(VkDevice device)
		{
			if (pipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, pipeline, nullptr);
				pipeline = VK_NULL_HANDLE;
			}
			if (pipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
				pipelineLayout = VK_NULL_HANDLE;
			}
			if (descriptorSetLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
				descriptorSetLayout = VK_NULL_HANDLE;
			}
			sdfTexture.Destroy();
		}
	} m_analyticalSDFGeneration;

	Texture* GetAnalyticalSdfTexture()
	{
        m_analyticalSDFGeneration.sdfTexture.dimZ = m_config->Sdf.Resolution;
		return &m_analyticalSDFGeneration.sdfTexture;
    }
    Texture* GetMultiViewDepthSdfTexture()
    {
        // return m_multiViewDepthSDF4C.sdfFusionPass.finalSDFTexture
        Texture* sdfTex{new Texture};
        sdfTex->image = m_multiViewDepthSDF4C.sdfFusionPass.finalSDFTexture;
        sdfTex->dimZ = m_multiViewDepthSDF4C.sdfFusionPass.SDF_RESOLUTION;
        return sdfTex;
    }
	// 阶段四 (版本B): 多视角深度渲染与融合 (Multi-View Depth SDF)
	struct MultiViewDepthSDF
	{
		// 深度渲染Pass - Graphics渲染
		struct DepthRenderingPass
		{
			std::vector<CubeMap> depthCubeMaps; // 每个相机位置的深度立方体贴图
			Buffer indirectDrawBuffer;			// vkCmdDrawIndexedIndirect命令缓冲区
			Buffer cameraMatricesBuffer;		// 相机视图矩阵缓冲区
			VkPipeline pipeline = VK_NULL_HANDLE;
			VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
			VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
			VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
			VkRenderPass renderPass = VK_NULL_HANDLE;
			VkFramebuffer frameBuffer = VK_NULL_HANDLE; // 统一的多层framebuffer (Pure Vertex Shader Solution)
			FramebufferAttachment depthAttachment;		// 深度缓冲区

			static constexpr int CUBEMAP_SIZE = 64; // 立方体贴图分辨率 (降低到64)
		} depthPass;

		// SDF融合Pass - Compute计算
		struct SDFFusionPass
		{
			Texture sdfTexture; // 256³ 输出SDF纹理 (VK_FORMAT_R16_SFLOAT)
			VkPipeline pipeline = VK_NULL_HANDLE;
			VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
			VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
			VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
			VkSampler depthCubeSampler = VK_NULL_HANDLE; // 深度立方体贴图采样器

			static constexpr uint32_t SDF_RESOLUTION = 256; // 256³ SDF分辨率
		} fusionPass;

		// 控制参数
		uint32_t activeCameraCount = 0;				// 当前活跃的相机数量
		static constexpr uint32_t MAX_CAMERAS = 10; // 最大相机数量(来自Stage3B)

		void cleanup(VkDevice device)
		{
			// 清理深度渲染Pass
			if (depthPass.pipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, depthPass.pipeline, nullptr);
				depthPass.pipeline = VK_NULL_HANDLE;
			}
			if (depthPass.pipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, depthPass.pipelineLayout, nullptr);
				depthPass.pipelineLayout = VK_NULL_HANDLE;
			}
			if (depthPass.descriptorSetLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, depthPass.descriptorSetLayout, nullptr);
				depthPass.descriptorSetLayout = VK_NULL_HANDLE;
			}
			if (depthPass.renderPass != VK_NULL_HANDLE)
			{
				vkDestroyRenderPass(device, depthPass.renderPass, nullptr);
				depthPass.renderPass = VK_NULL_HANDLE;
			}

			// 清理framebuffer (Pure Vertex Shader Solution)
			if (depthPass.frameBuffer != VK_NULL_HANDLE)
			{
				vkDestroyFramebuffer(device, depthPass.frameBuffer, nullptr);
				depthPass.frameBuffer = VK_NULL_HANDLE;
			}

			// 清理cubemaps
			for (auto& cubeMap : depthPass.depthCubeMaps)
			{
				cubeMap.Tex.Destroy();
				for (auto& view : cubeMap.FaceViews)
				{
					if (view != VK_NULL_HANDLE)
					{
						vkDestroyImageView(device, view, nullptr);
						view = VK_NULL_HANDLE;
					}
				}
				// 清理framebuffer视图
				if (cubeMap.framebufferView != VK_NULL_HANDLE)
				{
					vkDestroyImageView(device, cubeMap.framebufferView, nullptr);
					cubeMap.framebufferView = VK_NULL_HANDLE;
				}
			}

			depthPass.indirectDrawBuffer.Destroy();
			depthPass.cameraMatricesBuffer.Destroy();
			depthPass.depthAttachment.Destroy();

			// 清理SDF融合Pass
			if (fusionPass.pipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, fusionPass.pipeline, nullptr);
				fusionPass.pipeline = VK_NULL_HANDLE;
			}
			if (fusionPass.pipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, fusionPass.pipelineLayout, nullptr);
				fusionPass.pipelineLayout = VK_NULL_HANDLE;
			}
			if (fusionPass.descriptorSetLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, fusionPass.descriptorSetLayout, nullptr);
				fusionPass.descriptorSetLayout = VK_NULL_HANDLE;
			}
			if (fusionPass.depthCubeSampler != VK_NULL_HANDLE)
			{
				vkDestroySampler(device, fusionPass.depthCubeSampler, nullptr);
				fusionPass.depthCubeSampler = VK_NULL_HANDLE;
			}
			fusionPass.sdfTexture.Destroy();
		}
	} m_multiViewDepthSDF;

	void CreateFinalSDFSampler();

	// GPU驱动的数据准备架构
	struct GPUDataPreparation
	{
		// 相机矩阵准备
		VkPipeline cameraMatrixPipeline = VK_NULL_HANDLE;
		VkPipelineLayout cameraMatrixPipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout cameraMatrixDescriptorLayout = VK_NULL_HANDLE;
		VkDescriptorSet cameraMatrixDescriptorSet = VK_NULL_HANDLE;

		// 间接命令生成
		VkPipeline indirectCommandPipeline = VK_NULL_HANDLE;
		VkPipelineLayout indirectCommandPipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout indirectCommandDescriptorLayout = VK_NULL_HANDLE;
		VkDescriptorSet indirectCommandDescriptorSet = VK_NULL_HANDLE;

		// GPU-only buffers (无CPU可见性)
		Buffer cameraMatricesBuffer_GPU;	// 相机矩阵数据 (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		Buffer activeCameraCountBuffer_GPU; // 活跃相机数量 (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		Buffer indirectDrawBuffer_GPU;		// 间接绘制命令 (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)

		void cleanup(VkDevice device)
		{
			// 清理相机矩阵准备管线
			if (cameraMatrixPipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, cameraMatrixPipeline, nullptr);
				cameraMatrixPipeline = VK_NULL_HANDLE;
			}
			if (cameraMatrixPipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, cameraMatrixPipelineLayout, nullptr);
				cameraMatrixPipelineLayout = VK_NULL_HANDLE;
			}
			if (cameraMatrixDescriptorLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, cameraMatrixDescriptorLayout, nullptr);
				cameraMatrixDescriptorLayout = VK_NULL_HANDLE;
			}

			// 清理间接命令生成管线
			if (indirectCommandPipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, indirectCommandPipeline, nullptr);
				indirectCommandPipeline = VK_NULL_HANDLE;
			}
			if (indirectCommandPipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, indirectCommandPipelineLayout, nullptr);
				indirectCommandPipelineLayout = VK_NULL_HANDLE;
			}
			if (indirectCommandDescriptorLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, indirectCommandDescriptorLayout, nullptr);
				indirectCommandDescriptorLayout = VK_NULL_HANDLE;
			}

			// 清理缓冲区
			cameraMatricesBuffer_GPU.Destroy();
			activeCameraCountBuffer_GPU.Destroy();
			indirectDrawBuffer_GPU.Destroy();
		}
	} m_gpuDataPreparation;

	// ==================== 新的多视角深度SDF方案 (版本4C) ====================
	// 基于用户提供的详细设计文档，支持多子部件模型和完整的GPU驱动架构
	struct MultiViewDepthSDF4C
	{

		// 模型子部件信息结构 (与着色器匹配)
		struct ModelPartInfo
		{
			uint32_t indexCount;	// 该子部件的索引数量
			uint32_t firstIndex;	// 该子部件在全局索引缓冲中的起始位置
			uint32_t vertexOffset;	// 该子部件的顶点偏移
			uint32_t materialIndex; // 材质索引 (暂时保留，可能用于未来扩展)
		};

		// 模型静态数据缓冲区 (在程序启动时加载，整个阶段四共用)
		struct ModelStaticData
		{
			Buffer modelVertexBuffer;			  // 包含模型所有子部件的全部顶点数据
			Buffer modelIndexBuffer;			  // 包含模型所有子部件的全部索引数据
			Buffer modelPartsBuffer;			  // 描述每个子部件的几何信息 (indexCount, firstIndex等)
			Buffer modelMatricesBuffer;			  // 存储每个子部件的模型矩阵
			uint32_t totalPartCount = 0;		  // 模型的总子部件数量
			std::vector<ModelPartInfo> partInfos; // CPU端的子部件信息副本 (用于构建缓冲区)
		} staticData;

		// GPU数据准备阶段 - 两个计算着色器
		struct GPUDataPreparation4C
		{
			// 相机矩阵准备计算着色器相关资源
			VkPipeline cameraMatrixPipeline = VK_NULL_HANDLE;
			VkPipelineLayout cameraMatrixPipelineLayout = VK_NULL_HANDLE;
			VkDescriptorSetLayout cameraMatrixDescriptorLayout = VK_NULL_HANDLE;
			VkDescriptorSet cameraMatrixDescriptorSet = VK_NULL_HANDLE;

			// 间接命令生成计算着色器相关资源
			VkPipeline indirectCommandPipeline = VK_NULL_HANDLE;
			VkPipelineLayout indirectCommandPipelineLayout = VK_NULL_HANDLE;
			VkDescriptorSetLayout indirectCommandDescriptorLayout = VK_NULL_HANDLE;
			VkDescriptorSet indirectCommandDescriptorSet = VK_NULL_HANDLE;

			// GPU端数据缓冲区
			Buffer cameraMatricesBuffer;	// 相机位置数据 struct CameraMatrix{float4 cameraPosition;}
			Buffer indirectCommandsBuffer;	// VkDrawIndexedIndirectCommand命令缓冲
			Buffer activeCameraCountBuffer; // 活跃相机数量缓冲区 (单个uint32_t)

			// 控制参数
			uint32_t activeCameraCount = 0; // 实际使用的相机数量
		} gpuPreparation;

		// 深度渲染阶段 - 使用Multiview的VS+FS管线
		struct DepthRenderingPass4C
		{
			// Multiview渲染相关对象
			VkRenderPass renderPass = VK_NULL_HANDLE;	// 启用multiview功能的渲染通道
			VkFramebuffer framebuffer = VK_NULL_HANDLE; // 单一framebuffer，绑定N*6层的深度数组

			// 图形管线对象
			VkPipeline pipeline = VK_NULL_HANDLE;
			VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
			VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
			VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

			// 深度立方体贴图数组 (N个相机 × 6个面)
			VkImage depthCubemapArray = VK_NULL_HANDLE;			   // 3D图像资源
			VkImageView depthCubemapArrayView = VK_NULL_HANDLE;	   // 用于framebuffer渲染
			VkImageView depthCubemapSamplingView = VK_NULL_HANDLE; // 用于SDF融合阶段采样
			VkDeviceMemory depthCubemapMemory = VK_NULL_HANDLE;

			// 渲染参数结构体
			struct RenderParams
			{
                glm::mat4 ModelMatrix;
				glm::mat4 projectionMatrix;
				uint32_t totalPartCount;
				uint32_t activeCameraCount;
				uint32_t totalDrawCommands; // 总绘制命令数 (partCount * cameraCount * 6)
				uint32_t baseInstanceID;	// 基础实例ID用于解码
				glm::uvec2 _padding;		// 16字节对齐
			} renderParams;

			// 深度附件 (用于深度测试)
			VkImage depthAttachment = VK_NULL_HANDLE;
			VkImageView depthAttachmentView = VK_NULL_HANDLE;
			VkDeviceMemory depthAttachmentMemory = VK_NULL_HANDLE;

			static constexpr uint32_t CUBEMAP_SIZE = 64;				   // 立方体贴图分辨率 (降低到64)
			static constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_R32_SFLOAT; // 32位浮点深度格式
		} depthRendering;

		// SDF融合阶段 - 计算着色器重建3D SDF
		struct SDFFusionPass4C
		{
			// 计算管线对象
			VkPipeline computePipeline = VK_NULL_HANDLE;
			VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
			VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
			VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

			// 最终输出SDF纹理 (3D volume texture)
			VkImage finalSDFTexture = VK_NULL_HANDLE;  // VK_ItexturMAGE_TYPE_3D
			VkImageView finalSDFView = VK_NULL_HANDLE; // VK_IMAGE_VIEW_TYPE_3D
			VkSampler finalSDFSampler = VK_NULL_HANDLE;
			VkDescriptorImageInfo finalSDFDescriptor{};

			VkDeviceMemory finalSDFMemory = VK_NULL_HANDLE;

			// 深度图采样器 (用于读取DepthCubemapArray)
			VkSampler depthCubemapSampler = VK_NULL_HANDLE;
			VkImageView depthCubemapArrayView = VK_NULL_HANDLE; // 2D array view for compute shader

			// Push constants structure
			struct PushConstants
			{
				uint32_t activeCameraCount; // Number of active cameras
				float maxDistance;			// Maximum SDF distance
				glm::vec2 _padding;			// 16-byte alignment
			} pushConstants;

			static constexpr uint32_t SDF_RESOLUTION = 64; // 64³ SDF分辨率 (降低到64)
		} sdfFusionPass;

		// 全局控制参数
		static constexpr uint32_t MAX_CAMERAS = 10; // 最大相机数量

		// 清理函数
		void cleanup(VkDevice device)
		{
			// 清理模型静态数据
			staticData.modelVertexBuffer.Destroy();
			staticData.modelIndexBuffer.Destroy();
			staticData.modelPartsBuffer.Destroy();
			staticData.modelMatricesBuffer.Destroy();

			// 清理GPU数据准备阶段
			if (gpuPreparation.cameraMatrixPipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, gpuPreparation.cameraMatrixPipeline, nullptr);
				gpuPreparation.cameraMatrixPipeline = VK_NULL_HANDLE;
			}
			if (gpuPreparation.cameraMatrixPipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, gpuPreparation.cameraMatrixPipelineLayout, nullptr);
				gpuPreparation.cameraMatrixPipelineLayout = VK_NULL_HANDLE;
			}
			if (gpuPreparation.cameraMatrixDescriptorLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, gpuPreparation.cameraMatrixDescriptorLayout, nullptr);
				gpuPreparation.cameraMatrixDescriptorLayout = VK_NULL_HANDLE;
			}
			if (gpuPreparation.indirectCommandPipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, gpuPreparation.indirectCommandPipeline, nullptr);
				gpuPreparation.indirectCommandPipeline = VK_NULL_HANDLE;
			}
			if (gpuPreparation.indirectCommandPipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, gpuPreparation.indirectCommandPipelineLayout, nullptr);
				gpuPreparation.indirectCommandPipelineLayout = VK_NULL_HANDLE;
			}
			if (gpuPreparation.indirectCommandDescriptorLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, gpuPreparation.indirectCommandDescriptorLayout, nullptr);
				gpuPreparation.indirectCommandDescriptorLayout = VK_NULL_HANDLE;
			}
			gpuPreparation.cameraMatricesBuffer.Destroy();
			gpuPreparation.indirectCommandsBuffer.Destroy();
			gpuPreparation.activeCameraCountBuffer.Destroy();

			// 清理深度渲染阶段
			if (depthRendering.pipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, depthRendering.pipeline, nullptr);
				depthRendering.pipeline = VK_NULL_HANDLE;
			}
			if (depthRendering.pipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, depthRendering.pipelineLayout, nullptr);
				depthRendering.pipelineLayout = VK_NULL_HANDLE;
			}
			if (depthRendering.descriptorSetLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, depthRendering.descriptorSetLayout, nullptr);
				depthRendering.descriptorSetLayout = VK_NULL_HANDLE;
			}
			if (depthRendering.renderPass != VK_NULL_HANDLE)
			{
				vkDestroyRenderPass(device, depthRendering.renderPass, nullptr);
				depthRendering.renderPass = VK_NULL_HANDLE;
			}
			if (depthRendering.framebuffer != VK_NULL_HANDLE)
			{
				vkDestroyFramebuffer(device, depthRendering.framebuffer, nullptr);
				depthRendering.framebuffer = VK_NULL_HANDLE;
			}
			if (depthRendering.depthCubemapArrayView != VK_NULL_HANDLE)
			{
				vkDestroyImageView(device, depthRendering.depthCubemapArrayView, nullptr);
				depthRendering.depthCubemapArrayView = VK_NULL_HANDLE;
			}
			if (depthRendering.depthCubemapSamplingView != VK_NULL_HANDLE)
			{
				vkDestroyImageView(device, depthRendering.depthCubemapSamplingView, nullptr);
				depthRendering.depthCubemapSamplingView = VK_NULL_HANDLE;
			}
			if (depthRendering.depthCubemapArray != VK_NULL_HANDLE)
			{
				vkDestroyImage(device, depthRendering.depthCubemapArray, nullptr);
				depthRendering.depthCubemapArray = VK_NULL_HANDLE;
			}
			if (depthRendering.depthCubemapMemory != VK_NULL_HANDLE)
			{
				vkFreeMemory(device, depthRendering.depthCubemapMemory, nullptr);
				depthRendering.depthCubemapMemory = VK_NULL_HANDLE;
			}
			if (depthRendering.depthAttachmentView != VK_NULL_HANDLE)
			{
				vkDestroyImageView(device, depthRendering.depthAttachmentView, nullptr);
				depthRendering.depthAttachmentView = VK_NULL_HANDLE;
			}
			if (depthRendering.depthAttachment != VK_NULL_HANDLE)
			{
				vkDestroyImage(device, depthRendering.depthAttachment, nullptr);
				depthRendering.depthAttachment = VK_NULL_HANDLE;
			}
			if (depthRendering.depthAttachmentMemory != VK_NULL_HANDLE)
			{
				vkFreeMemory(device, depthRendering.depthAttachmentMemory, nullptr);
				depthRendering.depthAttachmentMemory = VK_NULL_HANDLE;
			}

			// 清理SDF融合阶段
			if (sdfFusionPass.computePipeline != VK_NULL_HANDLE)
			{
				vkDestroyPipeline(device, sdfFusionPass.computePipeline, nullptr);
				sdfFusionPass.computePipeline = VK_NULL_HANDLE;
			}
			if (sdfFusionPass.pipelineLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(device, sdfFusionPass.pipelineLayout, nullptr);
				sdfFusionPass.pipelineLayout = VK_NULL_HANDLE;
			}
			if (sdfFusionPass.descriptorSetLayout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device, sdfFusionPass.descriptorSetLayout, nullptr);
				sdfFusionPass.descriptorSetLayout = VK_NULL_HANDLE;
			}
			if (sdfFusionPass.depthCubemapSampler != VK_NULL_HANDLE)
			{
				vkDestroySampler(device, sdfFusionPass.depthCubemapSampler, nullptr);
				sdfFusionPass.depthCubemapSampler = VK_NULL_HANDLE;
			}
			if (sdfFusionPass.depthCubemapArrayView != VK_NULL_HANDLE)
			{
				vkDestroyImageView(device, sdfFusionPass.depthCubemapArrayView, nullptr);
				sdfFusionPass.depthCubemapArrayView = VK_NULL_HANDLE;
			}
			if (sdfFusionPass.finalSDFView != VK_NULL_HANDLE)
			{
				vkDestroyImageView(device, sdfFusionPass.finalSDFView, nullptr);
				sdfFusionPass.finalSDFView = VK_NULL_HANDLE;
			}
			if (sdfFusionPass.finalSDFTexture != VK_NULL_HANDLE)
			{
				vkDestroyImage(device, sdfFusionPass.finalSDFTexture, nullptr);
				sdfFusionPass.finalSDFTexture = VK_NULL_HANDLE;
			}
			if (sdfFusionPass.finalSDFMemory != VK_NULL_HANDLE)
			{
				vkFreeMemory(device, sdfFusionPass.finalSDFMemory, nullptr);
				sdfFusionPass.finalSDFMemory = VK_NULL_HANDLE;
			}
		}
	} m_multiViewDepthSDF4C;

	// Vulkan GPU到CPU读取finalVoxelStateTexture的完整实现示例
	bool ReadFinalVoxelStateTextureToCPU_Staging();
	void TestVoxelOctreeFromVoxelData();
	// 统一GPU管线方法 - 分离资源管理
	void InitializeUnifiedGPUPipelineResources(); // 初始化资源
	void RecordUnifiedGPUPipelineCommands();	  // 预录制命令
	void SubmitUnifiedGPUPipeline();			  // 在渲染循环中提交
	void ExecuteUnifiedGPUPipeline();			  // 旧方法，待重构

	void ExecuteVoxelizationMarkPass(VkCommandBuffer cmd);
	void ExecuteVoxelizationFillPass(VkCommandBuffer cmd);

	// 阶段三和阶段四的具体执行方法
	void InitializeSolidNodeSelectionResources();			  // 初始化节点筛选资源
	void InitializeAnalyticalSDFGenerationResources();		  // 初始化SDF生成资源
	void UpdateSolidNodeSelectionDescriptorSet();			  // 更新节点筛选描述符集
	void ExecuteSolidNodeSelection(VkCommandBuffer cmd);	  // 执行节点筛选
	void ExecuteAnalyticalSDFGeneration(VkCommandBuffer cmd); // 执行SDF生成
	void ValidateSolidNodeSelectionResults();				  // 验证节点筛选结果（调试用）

	// 阶段三版本B的相关函数
	void InitializeSolidNodeSelectionBResources();		  // 初始化版本B资源
	void UpdateSolidNodeSelectionBDescriptorSet();		  // 更新版本B描述符集
	void UpdateAnalyticalSDFGenerationDescriptorSet();	  // 动态更新阶段四使用的节点选择版本
	void ExecuteSolidNodeSelectionB(VkCommandBuffer cmd); // 执行版本B节点筛选
	void ValidateSolidNodeSelectionBResults();			  // 验证版本B结果（调试用）
	void SetSolidNodeSelectionVersion(bool useVersionB);  // 切换版本A/B

	// 阶段四版本B的相关函数 (Multi-View Depth SDF)
	void InitializeMultiViewDepthSDFResources(); // 初始化多视角深度SDF资源

	// ==================== 阶段四版本C的相关函数 (MultiViewDepthSDF4C) ====================
	// 模型静态数据管理
	void LoadModelStaticData4C();  // 从GLTF模型提取并上传静态数据
	void CreateModelPartInfos4C(); // 创建模型子部件信息缓冲区
	void CreateModelMatrices4C();  // 创建模型矩阵缓冲区

	// 初始化和管理函数
	void InitializeMultiViewDepthSDF4CResources();			  // 初始化所有MultiViewDepthSDF4C资源
	void InitializeGPUDataPreparation4C();					  // 初始化GPU数据准备阶段
	void CreateCameraMatrixPreparationPipeline();			  // 创建相机矩阵准备计算管线
	void CreateIndirectCommandGenerationPipeline();			  // 创建间接命令生成计算管线
	void InitializeDepthRendering4C();						  // 初始化深度渲染阶段
	void CreateDepthCubemapArray();							  // 创建深度立方体贴图数组
	void CreateMultiViewDepthRenderPass();					  // 创建Multiview渲染通道
	void CreateMultiViewDepthPipeline();					  // 创建多视角深度渲染管线
	void ExecuteMultiViewDepthRendering(VkCommandBuffer cmd); // 执行多视角深度渲染
	void InitializeSDFFusion4C();							  // 初始化SDF融合阶段
	void UpdateMultiViewDepthSDFDescriptorSets();			  // 更新描述符集
	void ExecuteSDFFusion(VkCommandBuffer cmd);				  // 执行SDF融合
	void ValidateMultiViewDepthSDFResults();				  // 验证结果（调试用）

	// GPU驱动数据准备相关函数
	void InitializeGPUDataPreparation();				 // 初始化GPU数据准备资源
	void ExecuteGPUDataPreparation(VkCommandBuffer cmd); // 执行GPU数据准备

	// GPU数据准备辅助函数
	void BindGPUDataPreparationDescriptors(); // 绑定GPU数据准备描述符集

	// 辅助函数
	void InitializeMultiViewDepthRenderingPass(); // 初始化深度渲染Pass
	void InitializeSDFFusionPass();				  // 初始化SDF融合Pass
	void CreateSDFFusionPipeline();				  // 创建SDF融合管线
	void CreateFinalSDFTexture();				  // 创建最终SDF 3D纹理
	void CreateDepthCubemapSampler();			  // 创建深度立方体贴图采样器

	// 相机矩阵结构体 (与着色器匹配)
	struct CameraMatrix
	{
		glm::mat4 viewMatrix;
		glm::vec3 cameraPosition;
		float padding;
	};

	// Stage 4B 辅助函数
	void PrepareCameraMatricesFromStage3B(); // 从阶段3B准备相机矩阵
	void PrepareIndirectDrawCommands();		 // 准备间接绘制命令
	VkBuffer GetModelVertexBuffer();		 // 获取模型顶点缓冲区
	VkBuffer GetModelIndexBuffer();			 // 获取模型索引缓冲区
	uint32_t GetModelIndexCount();			 // 获取模型索引数量
	uint32_t GetModelVertexCount();			 // 获取模型顶点数量

	// Voxel Visualization System
	struct VoxelVisualization
	{
		// Visualization modes
		enum class Mode
		{
			SLICE_XY,	 // XY平面切片
			SLICE_XZ,	 // XZ平面切片
			SLICE_YZ,	 // YZ平面切片
			POINT_CLOUD, // 点云渲染
			CUBE_RENDER	 // 立方体渲染
		} currentMode{ Mode::SLICE_XY };

		// Slice visualization
		struct SliceView
		{
			uint32_t sliceIndex{ 128 };				// 当前切片索引 (0-255)
			float sliceDepth{ 0.5f };					// 切片深度 (0.0-1.0)
			glm::vec3 sliceColor{ 1.0f, 0.0f, 0.0f }; // 切片高亮颜色
		} sliceView;

		// Point cloud visualization
		struct PointCloudView
		{
			float pointSize{ 2.0f };					// 点的大小
			float density{ 0.1f };					// 点的密度 (0.0-1.0)
			glm::vec3 pointColor{ 0.0f, 1.0f, 0.0f }; // 点的颜色
		} pointCloudView;

		// Cube rendering
		struct CubeView
		{
			float cubeSize{ 1.0f };				   // 立方体大小
			float opacity{ 0.8f };				   // 透明度
			glm::vec3 cubeColor{ 0.0f, 0.0f, 1.0f }; // 立方体颜色
		} cubeView;

		// Camera controls for 3D view
		struct Camera
		{
			glm::vec3 position{ 0.0f, 0.0f, 5.0f };
			glm::vec3 target{ 0.0f, 0.0f, 0.0f };
			glm::vec3 up{ 0.0f, 1.0f, 0.0f };
			float fov{ 45.0f };
			float nearPlane{ 0.1f };
			float farPlane{ 100.0f };
		} camera;

		// Rendering resources
		VkPipeline slicePipeline{ VK_NULL_HANDLE };
		VkPipeline pointCloudPipeline{ VK_NULL_HANDLE };
		VkPipeline cubePipeline{ VK_NULL_HANDLE };
		VkPipelineLayout slicePipelineLayout{ VK_NULL_HANDLE };
		VkPipelineLayout pointCloudPipelineLayout{ VK_NULL_HANDLE };
		VkPipelineLayout cubePipelineLayout{ VK_NULL_HANDLE };
		VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
		VkRenderPass renderPass{ VK_NULL_HANDLE };
		VkFramebuffer framebuffer{ VK_NULL_HANDLE };

		// Uniform buffer for visualization
		Buffer uniformBuffer;
		struct UniformData
		{
			glm::mat4 viewProjection;
			glm::mat4 model;
			glm::vec3 cameraPos;
			glm::vec3 sliceColor;
			glm::vec3 pointColor;
			glm::vec3 cubeColor;
			glm::uvec3 gridSize;
			uint32_t sliceIndex;
			float sliceDepth;
			float pointSize;
			float density;
			float cubeSize;
			float opacity;
		} uniformData;

		// Vertex buffer for full-screen quad (slice rendering)
		Buffer quadVertexBuffer;
		Buffer quadIndexBuffer;

		// Vertex buffer for point cloud
		Buffer pointCloudBuffer;
		uint32_t pointCount{ 0 };

		// Vertex buffer for cubes
		Buffer cubeVertexBuffer;
		Buffer cubeIndexBuffer;
		uint32_t cubeCount{ 0 };

		bool initialized{ false };
	};
	VoxelVisualization m_voxelVisualization;

	// Voxel Point Cloud Rendering
	struct VoxelPointCloud
	{
		// Compute pipeline for extracting voxel points
		VkPipeline computePipeline{ VK_NULL_HANDLE };
		VkPipelineLayout computePipelineLayout{ VK_NULL_HANDLE };
		VkDescriptorSetLayout computeDescriptorSetLayout{ VK_NULL_HANDLE };
		VkDescriptorSet computeDescriptorSet{ VK_NULL_HANDLE };

		// Graphics pipeline for rendering points
		VkPipeline graphicsPipeline{ VK_NULL_HANDLE };
		VkPipelineLayout graphicsPipelineLayout{ VK_NULL_HANDLE };
		VkDescriptorSetLayout graphicsDescriptorSetLayout{ VK_NULL_HANDLE };
		VkDescriptorSet graphicsDescriptorSet{ VK_NULL_HANDLE };

		// Buffers
		Buffer voxelPointsBuffer; // Storage buffer for extracted voxel points
		Buffer uniformBuffer;	  // Uniform buffer for rendering constants

		// Uniform data
		struct UniformData
		{
			glm::mat4 viewProjection;
			glm::mat4 model;
			float pointSize;
			float densityScale;
			float threshold;
		} uniformData;

		// Compute constants
		struct ComputeConstants
		{
			glm::uvec3 gridSize;
			float threshold;
			uint32_t maxPoints;
		} computeConstants;

		uint32_t pointCount{ 0 };
		bool initialized{ false };
	};
	VoxelPointCloud m_voxelPointCloud;

	struct GpuOctreePass
	{
		static const uint32_t OCTREE_RESOLUTION = 128;
		uint32_t queueFamilyIndex{ 0 };
		VkQueue queue{ nullptr };
		VkCommandPool commandPool{ nullptr };
		VkCommandBuffer commandBuffer{ nullptr };
		VkSemaphore semaphore{ nullptr };

		Buffer primitiveMortonBuffer;
		Buffer octreeNodeBuffer;

		struct
		{
			VkPipeline mortonGen{ VK_NULL_HANDLE };
			VkPipeline radixSort{ VK_NULL_HANDLE };
			VkPipeline buildHierarchy{ VK_NULL_HANDLE };
		} pipelines;

		struct
		{
			VkPipelineLayout mortonGen{ VK_NULL_HANDLE };
			VkPipelineLayout radixSort{ VK_NULL_HANDLE };
			VkPipelineLayout buildHierarchy{ VK_NULL_HANDLE };
		} pipelineLayouts;

		struct
		{
			VkDescriptorSet mortonGen{ VK_NULL_HANDLE };
			VkDescriptorSet radixSort{ VK_NULL_HANDLE };
			VkDescriptorSet buildHierarchy{ VK_NULL_HANDLE };
		} descriptorSets;

		struct
		{
			VkDescriptorSetLayout mortonGen{ VK_NULL_HANDLE };
			VkDescriptorSetLayout radixSort{ VK_NULL_HANDLE };
			VkDescriptorSetLayout buildHierarchy{ VK_NULL_HANDLE };
		} descriptorSetLayouts;

	} m_gpuOctreePass;

	void SetupGpuOctreePass();
	void BuildGpuOctreeCommandBuffer();
	void DestroyGpuOctreePass();

	// Voxelization Pass functions
	void SetupVoxelizationPass();
	void InitVoxelizationTextures();
	void InitVoxelizationDescriptors();
	void InitVoxelizationPipelines();
	void VoxelizationPass(VkCommandBuffer cmd);
	void VoxelizationMarkPass(VkCommandBuffer cmd);
	void VoxelizationFillPass(VkCommandBuffer cmd);
	void ExecuteVoxelizationWithSync();
	void UpdateVoxelizationConstants();
	void BuildVoxelizationCommandBuffer();

	// Voxel Point Cloud Rendering functions
	void SetupVoxelPointCloud();
	void InitVoxelPointCloudResources();
	void InitVoxelPointCloudPipelines();
	void RenderVoxelPointCloud(VkCommandBuffer cmd);
	void UpdateVoxelPointCloudUniforms();
	void ExtractVoxelPoints();

	// Voxel Visualization functions
	void SetupVoxelVisualization();
	void InitVoxelVisualizationResources();
	void InitVoxelVisualizationPipelines();
	void RenderVoxelVisualization(VkCommandBuffer cmd);
	void UpdateVoxelVisualizationUniforms();
	void GeneratePointCloudFromVoxels();
	void GenerateCubesFromVoxels();
	void CleanupVoxelVisualization();
	void SaveVoxelSlices();

	// Voxel Visualization UI controls
	void VoxelVisualizationUI();
	void HandleVoxelVisualizationInput();

	// Test function for voxelization
	void TestVoxelization();

	// Voxel texture save function
	void SaveVoxelTextureWithValidation(const std::string& filename);

	// Voxelization debug flag
	bool m_voxelizationDebugEnabled{ false };
    glm::mat4 CalculateModelToStandardTransform(const vkglTF::Model& model);
 private:
	void InitializeMeshToSdfOperator();
	MeshToSdf* meshToSdfOperator_{};
    VkCommandBuffer meshToSdfCommandBuffer_{};

public:
    MeshToSdf* GetMeshToSdfOperator();

public: 
	void TestBruteSdfAndSave();
};