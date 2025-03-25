module;
#include "vulkan/vulkan.h"
#include "glm/glm.hpp"
export module UIMod;
import std;
import BufferMod;
import DeviceMod;

export class UIOverlay
{
public:
	VulkanDevice* device{ nullptr };
	VkQueue queue{ VK_NULL_HANDLE };

	VkSampleCountFlagBits rasterizationSamples{ VK_SAMPLE_COUNT_1_BIT };
	uint32_t subpass{ 0 };

	Buffer vertexBuffer;
	Buffer indexBuffer;
	int32_t vertexCount{ 0 };
	int32_t indexCount{ 0 };

	std::vector<VkPipelineShaderStageCreateInfo> shaders;

	VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkPipeline pipeline{ VK_NULL_HANDLE };

	VkDeviceMemory fontMemory{ VK_NULL_HANDLE };
	VkImage fontImage{ VK_NULL_HANDLE };
	VkImageView fontView{ VK_NULL_HANDLE };
	VkSampler sampler{ VK_NULL_HANDLE };

	struct PushConstBlock
	{
		glm::vec2 scale;
		glm::vec2 translate;
	} pushConstBlock;

	bool visible{ true };
	bool updated{ false };
	float scale{ 1.0f };
	float updateTimer{ 0.0f };

	UIOverlay();
	~UIOverlay();

	void PreparePipeline(const VkPipelineCache pipelineCache, const VkRenderPass renderPass, const VkFormat colorFormat, const VkFormat depthFormat);
	void PrepareResources();

	bool Update();
	void Draw(const VkCommandBuffer commandBuffer);
	void Resize(uint32_t width, uint32_t height);

	void FreeResources();

	bool Header(const char* caption);
	bool CheckBox(const char* caption, bool* value);
	bool CheckBox(const char* caption, int32_t* value);
	bool RadioButton(const char* caption, bool value);
	bool InputFloat(const char* caption, float* value, float step, const char* format = "%.3f");
	bool SliderFloat(const char* caption, float* value, float min, float max);
	bool SliderInt(const char* caption, int32_t* value, int32_t min, int32_t max);
	bool ComboBox(const char* caption, int32_t* itemindex, std::vector<std::string> items);
	bool Button(const char* caption);
	bool ColorPicker(const char* caption, float* color);
	void Text(const char* formatstr, ...);
};