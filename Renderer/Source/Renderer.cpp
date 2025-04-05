module;
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "stb/stb_image.h"
#include "tiny_gltf.h"
#include "imgui.h"
module RendererMod;
import InitMod;
import ToolMod;

#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		std::cout << "Fatal : VkResult is \"" << Tool::ErrorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << "\n"; \
		assert(res == VK_SUCCESS);																		\
	}																									\
}
Renderer::Renderer(Config* config) :m_config(config)
{
	m_neededFeatures.validation = config->enableValidation;
	m_camera.type = Camera::CameraType::firstperson;
	m_camera.flipY = true;
	m_camera.setPosition(config->camera.pos);
	m_camera.setRotation(glm::vec3(0.0f));
	m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
	m_camera.setMovementSpeed(config->camera.movementSpeed);
}


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
	//std::cout << app->m_framebufferResized;
}
/// @brief 初始化窗口
void Renderer::InitWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// 设置 GLFW 创建的窗口是否可以调整大小。
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	// 使用glfw创建窗口
	m_window = glfwCreateWindow(m_width, m_height, "Renderer", nullptr, nullptr);


	// 传递对象this指针给回调函数。这样我们就可以在回调函数中访问类的成员变量, 也就是传递窗口变化信息
	glfwSetWindowUserPointer(m_window, this);
	glfwSetKeyCallback(m_window, KeyCallback);          // 键盘事件
	glfwSetCursorPosCallback(m_window, MouseCallback);  // 鼠标移动
	glfwSetScrollCallback(m_window, ScrollCallback);    // 鼠标滚轮
	glfwSetMouseButtonCallback(m_window, MouseButtonCallback); // 鼠标按键
	glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback); //窗口大小改变、最小化
}

void Renderer::PreCreateSubmitInfo()
{

	m_submitInfo = Init::submitInfo();
	m_submitInfo.pWaitDstStageMask = &m_submitPipelineStages;
	m_submitInfo.waitSemaphoreCount = 1;
	m_submitInfo.pWaitSemaphores = &m_semaphores.presentComplete;
	m_submitInfo.signalSemaphoreCount = 1;
	m_submitInfo.pSignalSemaphores = &m_semaphores.renderComplete;
}
void Renderer::SetEnabledFeatures()
{
	vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_deviceFeatures);
	if (m_deviceFeatures.geometryShader)
	{
		m_enabledFeatures.geometryShader = VK_TRUE;
	}
	if (m_deviceFeatures.samplerAnisotropy)
	{
		m_enabledFeatures.samplerAnisotropy = VK_TRUE;
	}
}
void Renderer::EncapsulationDevice()
{
	VkResult result;
	m_vulkanDevice = new VulkanDevice(m_physicalDevice);
	SetEnabledFeatures();
	result = m_vulkanDevice->CreateLogicalDevice(m_enabledFeatures, m_enabledDeviceExtensions, m_deviceCreatepNextChain);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Could not create Vulkan device: \n" + Tool::ErrorString(result));
	}
	m_device = m_vulkanDevice->logicalDevice;

	// Get a graphics queue from the device
	vkGetDeviceQueue(m_device, m_vulkanDevice->queueFamilyIndices.graphics, 0, &m_queues.graphicsQueue);
	vkGetDeviceQueue(m_device, m_vulkanDevice->queueFamilyIndices.graphics, 0, &m_queues.presentQueue);
}
/// @brief 初始化 Vulkan，包括相关设置
void Renderer::InitVulkan()
{
	CreateInstance();
	SetupDebugMessenger(); //先创建实例，相当于先指定和调试的接口，再之后再实际链接
	CreateSurface();
	PickPhysicalDevice();
	//CreateLogicalDevice();

	EncapsulationDevice();
	m_swapChain.SetContext(m_instance, m_physicalDevice, m_device);

	CreateSyncObjects();
	PreCreateSubmitInfo();
	m_swapChain.InitSurface(m_surface);
	m_swapChain.Create(m_width, m_height, false, false);

	// prepare
	//CreateSwapChain();
	CreateCommandPool();
	CreateCommandBuffers();
	CreateDepthResources();
	CreateRenderPass();
	CreateFramebuffers();
	InitUI();

	// Main
	LoadAssets();
	//CreateUniformBuffer();
	//CreateDescriptors();
	//CreateGraphicsPipeline();
	////CreateVertexBuffer();
	//BuildCommandBuffers();

	//PrepareOffscreenFramebuffer();
	//PrepareUniformBuffers();
	//SetupDescriptors();
	//PreparePipelines();
	//BuildCommandBuffers();
	//BuildDeferredCommandBuffer();

	SetupDefered();
	SetupShadow();
	InitLights();
	PrepareUniformBuffers();
	SetupDescriptorsDD();
	PreparePipelinesDD();
	BuildCommandBuffers();
	BuildDeferredCommandBuffer();
}
void Renderer::InitUI()
{
	m_UI.device = m_vulkanDevice;
	m_UI.queue = m_queues.graphicsQueue;
	m_UI.shaders = {
		LoadShader(Tool::GetShadersPath() + "Base/uioverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
		LoadShader(Tool::GetShadersPath() + "Base/uioverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
	};
	m_UI.PrepareResources();
	m_UI.PreparePipeline(nullptr, m_renderPass, m_swapChain.colorFormat, m_depthFormat);
}


void::Renderer::CreateCommandBuffers()
{
	// Create one command buffer for each swap chain image
	m_drawCmdBuffers.resize(m_swapChain.images.size());
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = Init::commandBufferAllocateInfo(m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(m_drawCmdBuffers.size()));
	VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, m_drawCmdBuffers.data()));
}
// composition
void Renderer::BuildCommandBuffers()
{

	VkCommandBufferBeginInfo cmdBufInfo = Init::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = Init::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = m_renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = m_width;
	renderPassBeginInfo.renderArea.extent.height = m_height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	VkViewport viewport = Init::viewport((float)m_width, (float)m_height, 0.0f, 1.0f);
	VkRect2D scissor = Init::rect2D(m_width, m_height, 0, 0);

	for (int32_t i = 0; i < m_drawCmdBuffers.size(); ++i)
	{
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = m_frameBuffers[i];

		VK_CHECK_RESULT(vkBeginCommandBuffer(m_drawCmdBuffers[i], &cmdBufInfo));

		vkCmdBeginRenderPass(m_drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);
		vkCmdSetScissor(m_drawCmdBuffers[i], 0, 1, &scissor);

		vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.composition);
		vkCmdBindDescriptorSets(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.composition, 0, 1, &m_descriptorSets.composition, 0, NULL);

		//glm::mat4x4 model{ 1.0f };
		//vkCmdPushConstants(m_drawCmdBuffers[i], m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4x4), &model);
		//VkDeviceSize offsets[1] = { 0 };
		vkCmdDraw(m_drawCmdBuffers[i], 3, 1, 0, 0);
		//m_glTFModel.Draw(m_drawCmdBuffers[i], vkglTF::RenderFlags::BindImages, m_pipelineLayout, 1);
		DrawUI(m_drawCmdBuffers[i]);
		//vkCmdDrawIndexed(m_drawCmdBuffers[i], indices.count, 1, 0, 0, 0);
		vkCmdEndRenderPass(m_drawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]));
	}
}

void Renderer::DrawUI(const VkCommandBuffer commandBuffer)
{
	const VkViewport viewport = Init::viewport((float)m_width, (float)m_height, 0.0f, 1.0f);
	const VkRect2D scissor = Init::rect2D(m_width, m_height, 0, 0);
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	m_UI.Draw(commandBuffer);
}

VkPipelineShaderStageCreateInfo Renderer::LoadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
	shaderStage.module = Tool::LoadShader(fileName.c_str(), m_device);
	shaderStage.pName = "main";
	assert(shaderStage.module != VK_NULL_HANDLE);
	m_shaderModules.push_back(shaderStage.module);
	return shaderStage;
}


//void Renderer::CreateGraphicsPipeline()
//{
//	std::array<VkDescriptorSetLayout, 2> setLayouts = { m_descriptorSetLayouts.Matrices ,m_descriptorSetLayouts.Textures }; //, m_descriptorSetLayouts.Textures
//
//	VkPipelineLayoutCreateInfo pipelineLayoutCI = Init::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
//
//	VkPushConstantRange pushConstRange{};
//	pushConstRange.offset = 0;
//	pushConstRange.size = sizeof(glm::mat4x4);
//	pushConstRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
//	pipelineLayoutCI.pushConstantRangeCount = 1;
//	pipelineLayoutCI.pPushConstantRanges = &pushConstRange;
//
//	if (vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &m_pipelineLayout) != VK_SUCCESS)
//	{
//		throw std::runtime_error("fail to create Pipelinelayout");
//	}
//
//	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
//	VkPipelineRasterizationStateCreateInfo rasterizationState = Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
//	VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
//	VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
//	VkPipelineDepthStencilStateCreateInfo depthStencilState = Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
//	VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1);
//	VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
//	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
//	VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
//
//	//// Vertex input state
//	//std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
//	//	Init::vertexInputBindingDescription(0, sizeof(GLTFModel::Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
//	//};
//	//std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
//	//	Init::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GLTFModel::Vertex, pos)),
//	//	Init::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GLTFModel::Vertex, normal)),
//	//	Init::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(GLTFModel::Vertex, uv)),
//	//	Init::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GLTFModel::Vertex, color)),
//	//};
//
//	//VkPipelineVertexInputStateCreateInfo vertexInputState = Init::pipelineVertexInputStateCreateInfo();
//	//vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
//	//vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
//	//vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
//	//vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();
//
//
//
//	VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_pipelineLayout, m_renderPass);
//	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
//
//	//shaderStages[0] = LoadShader(Tool::GetShadersPath() + "glTFloading/Mesh.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
//	//shaderStages[1] = LoadShader(Tool::GetShadersPath() + "glTFloading/Mesh.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
//
//	shaderStages[0] = LoadShader(Tool::GetShadersPath() + m_config->shadersPath[0], VK_SHADER_STAGE_VERTEX_BIT);
//	shaderStages[1] = LoadShader(Tool::GetShadersPath() + m_config->shadersPath[1], VK_SHADER_STAGE_FRAGMENT_BIT);
//
//	//pipelineCI.pVertexInputState = &vertexInputState;
//	pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal });
//	pipelineCI.pInputAssemblyState = &inputAssemblyState;
//	pipelineCI.pRasterizationState = &rasterizationState;
//	pipelineCI.pColorBlendState = &colorBlendState;
//	pipelineCI.pMultisampleState = &multisampleState;
//	pipelineCI.pViewportState = &viewportState;
//	pipelineCI.pDepthStencilState = &depthStencilState;
//	pipelineCI.pDynamicState = &dynamicState;
//	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
//	pipelineCI.pStages = shaderStages.data();
//
//
//	// Enable depth test and write
//	depthStencilState.depthWriteEnable = VK_TRUE;
//	depthStencilState.depthTestEnable = VK_TRUE;
//	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_pipeline));
//
//}
void Renderer::CreateDepthResources()
{
	VkImageCreateInfo imageCI{};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	m_depthFormat = FindDepthFormat();
	imageCI.format = m_depthFormat;
	imageCI.extent = { m_width, m_height, 1 };
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	vkCreateImage(m_device, &imageCI, nullptr, &m_depthStencil.image);
	VkMemoryRequirements memReqs{};
	vkGetImageMemoryRequirements(m_device, m_depthStencil.image, &memReqs);

	VkMemoryAllocateInfo memAllloc{};
	memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllloc.allocationSize = memReqs.size;
	FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAllloc, nullptr, &m_depthStencil.memory))

		if (vkBindImageMemory(m_device, m_depthStencil.image, m_depthStencil.memory, 0) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to allocate depth memory");
		};

	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.image = m_depthStencil.image;
	imageViewCI.format = m_depthFormat;
	imageViewCI.subresourceRange.baseMipLevel = 0;
	imageViewCI.subresourceRange.levelCount = 1;
	imageViewCI.subresourceRange.baseArrayLayer = 0;
	imageViewCI.subresourceRange.layerCount = 1;
	imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
	if (m_depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
	{
		imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	if (vkCreateImageView(m_device, &imageViewCI, nullptr, &m_depthStencil.view) != VK_SUCCESS)
	{
		throw std::runtime_error("fail to create depth image view");
	};
}
void Renderer::CreateFramebuffers()
{
	m_frameBuffers.resize(m_swapChain.images.size());
	for (uint32_t i = 0; i < m_frameBuffers.size(); ++i)
	{
		const VkImageView attachments[2]{ m_swapChain.imageViews[i] ,m_depthStencil.view };
		VkFramebufferCreateInfo frameBufferCI{};
		frameBufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCI.renderPass = m_renderPass;
		frameBufferCI.pAttachments = attachments;
		frameBufferCI.layers = 1;
		frameBufferCI.height = m_height;
		frameBufferCI.width = m_width;
		frameBufferCI.attachmentCount = 2;
		if (vkCreateFramebuffer(m_device, &frameBufferCI, nullptr, &m_frameBuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create framebuffer");
		};
	}
}
void Renderer::CreateCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolCI{};
	cmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCI.queueFamilyIndex = m_indices.graphicsFamily.value();
	cmdPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	if (vkCreateCommandPool(m_device, &cmdPoolCI, nullptr, &m_commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}
}
uint32_t Renderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}
	throw std::runtime_error("failed to find suitable memory type!");

}
VkResult Renderer::CreateBuffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer* buffer, VkDeviceSize size, void* data)
{
	buffer->device = m_device;

	VkBufferCreateInfo bufferCI{};
	bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCI.size = size;
	bufferCI.usage = usage;
	//bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(m_device, &bufferCI, nullptr, &buffer->buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create buffer!");
	}
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_device, buffer->buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

	// If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
	VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
	if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	{
		allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
		allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
		allocInfo.pNext = &allocFlagsInfo;
	}

	if (vkAllocateMemory(m_device, &allocInfo, nullptr, &buffer->memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	buffer->alignment = memRequirements.alignment;
	buffer->size = size;
	buffer->usageFlags = usage;
	buffer->memoryPropertyFlags = properties;

	// If a pointer to the buffer data has been passed, map the buffer and copy over the data
	if (data != nullptr)
	{
		buffer->Map();
		memcpy(buffer->mapped, data, size);
		if ((properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			buffer->Flush();

		buffer->Unmap();
	}
	buffer->SetupDescriptor();

	// vkBindBufferMemory(m_device, buffer->buffer, buffer->memory, 0);
	return buffer->Bind();
}
//void Renderer::CreateUniformBuffer()
//{
//	CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_uboBuffer, sizeof(m_uboMatrices));
//	m_uboBuffer.Map();
//	//VK_CHECK_RESULT(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_defered.uniformBuffers.offscreen, sizeof(UniformDataOffscreen)));
//	//VK_CHECK_RESULT(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_defered.uniformBuffers.composition, sizeof(UniformDataComposition)));
//
//	//// Map persistent
//	//VK_CHECK_RESULT(m_defered.uniformBuffers.offscreen.Map());
//	//VK_CHECK_RESULT(m_defered.uniformBuffers.composition.Map());
//
//	//// Update
//	//UpdateUniformBufferOffscreen();
//	//UpdateUniformBufferComposition();
//}
// Update matrices used for the offscreen rendering of the scene
void Renderer::UpdateUniformBufferOffscreen()
{
	m_uniformDataOffscreen.projection = m_camera.matrices.perspective;
	m_uniformDataOffscreen.view = m_camera.matrices.view;
	m_uniformDataOffscreen.model = glm::mat4(1.0f);
	memcpy(m_uniformBuffers.defered.mapped, &m_uniformDataOffscreen, sizeof(UniformDataOffscreen));
}

// Update lights and parameters passed to the composition shaders
void Renderer::UpdateUniformBufferComposition()
{
	//// Animate
	//m_uniformDataComposition.lights[0].position.x = -14.0f + std::abs(sin(glm::radians(m_timer * 360.0f)) * 20.0f);
	//m_uniformDataComposition.lights[0].position.z = 15.0f + cos(glm::radians(m_timer * 360.0f)) * 1.0f;

	//m_uniformDataComposition.lights[1].position.x = 14.0f - std::abs(sin(glm::radians(m_timer * 360.0f)) * 2.5f);
	//m_uniformDataComposition.lights[1].position.z = 13.0f + cos(glm::radians(m_timer * 360.0f)) * 4.0f;

	//m_uniformDataComposition.lights[2].position.x = 0.0f + sin(glm::radians(m_timer * 360.0f)) * 4.0f;
	//m_uniformDataComposition.lights[2].position.z = 4.0f + cos(glm::radians(m_timer * 360.0f)) * 2.0f;


	//m_uniformDataComposition.lights[0].position.x = 0.0f;
	//m_uniformDataComposition.lights[0].position.y = 65.0f;
	//m_uniformDataComposition.lights[0].position.z = 0.0f;

	//m_uniformDataComposition.lights[1].position.x = 0.0f;
	//m_uniformDataComposition.lights[1].position.z = -4.0f;

	//m_uniformDataComposition.lights[2].position.x = 0.0f;
	//m_uniformDataComposition.lights[2].position.z = 4.0f;

	for (uint32_t i = 0; i < LIGHT_COUNT; i++)
	{
		// mvp from light's pov (for shadows)
		glm::mat4 shadowProj = glm::perspective(glm::radians(m_shadowSettings.lightFOV), 1.0f, m_shadowSettings.zNear, m_shadowSettings.zFar);
		glm::mat4 shadowView = glm::lookAt(glm::vec3(m_uniformDataComposition.lights[i].position), glm::vec3(m_uniformDataComposition.lights[i].target), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 shadowModel = glm::mat4(1.0f);

		m_uniformDataShadows.mvp[i] = shadowProj * shadowView * shadowModel;
		m_uniformDataComposition.lights[i].viewMatrix = m_uniformDataShadows.mvp[i];
	}

	//memcpy(m_uniformDataShadows.instancePos, m_uniformDataOffscreen.instancePos, sizeof(UniformDataOffscreen::instancePos));
	memcpy(m_uniformBuffers.shadowGeometryShader.mapped, &m_uniformDataShadows, sizeof(UniformDataShadows));

	m_uniformDataComposition.viewPos = glm::vec4(m_camera.position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);;
	std::cout << m_uniformDataComposition.viewPos.x << "," << m_uniformDataComposition.viewPos.y << "," << m_uniformDataComposition.viewPos.z << std::endl;
	m_uniformDataComposition.debugDisplayTarget = m_debugDisplayTarget;

	memcpy(m_uniformBuffers.composition.mapped, &m_uniformDataComposition, sizeof(m_uniformDataComposition));
}
//void Renderer::CreateDescriptors()
//{
//	// Pool
//	std::vector<VkDescriptorPoolSize> poolSizes = {
//		Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
//		Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
//	};
//	VkDescriptorPoolCreateInfo descriptorPoolInfo = Init::descriptorPoolCreateInfo(poolSizes, 2);
//	VK_CHECK_RESULT(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool));
//
//	// create layout
//	VkDescriptorSetLayoutBinding setLayoutBinding =
//		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
//
//
//
//	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = Init::descriptorSetLayoutCreateInfo(&setLayoutBinding, 1);
//	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCI, nullptr, &m_descriptorSetLayouts.Matrices));
//	// Descriptor set layout for passing material textures
//	std::vector<VkDescriptorSetLayoutBinding>setLayoutBindings = {
//		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0)
//	};
//	descriptorSetLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), 1);
//	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCI, nullptr, &m_descriptorSetLayouts.Textures));
//
//	// allocate set
//	VkDescriptorSetAllocateInfo allocIF{};
//	allocIF.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//	allocIF.descriptorPool = m_descriptorPool;
//	allocIF.descriptorSetCount = 1;
//	allocIF.pSetLayouts = &m_descriptorSetLayouts.Matrices;
//
//	if (vkAllocateDescriptorSets(m_device, &allocIF, &m_descriptorSet) != VK_SUCCESS)
//	{
//		throw std::runtime_error("failed to create descriptor set!");
//	}
//	//allocIF.pSetLayouts = &m_descriptorSetLayouts.Textures;
//	//if (vkAllocateDescriptorSets(m_device, &allocIF, &m_texturesDescriptorSet) != VK_SUCCESS)
//	//{
//	//	throw std::runtime_error("failed to create descriptor set!");
//	//}
//
//	// write 
//	VkWriteDescriptorSet writeUBO{};
//	writeUBO.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//	writeUBO.dstSet = m_descriptorSet;
//	writeUBO.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//	writeUBO.dstBinding = 0;
//	writeUBO.pBufferInfo = &m_uboBuffer.descriptor;
//	writeUBO.descriptorCount = 1;
//
//	std::vector<VkWriteDescriptorSet> writeDescriptorSets{ writeUBO };
//	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
//
//	//// Descriptor sets for materials
//	//for (auto& image : m_glTFModel.images)
//	//{
//	//	const VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_descriptorSetLayouts.Textures, 1);
//	//	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &image.descriptorSet));
//	//	VkWriteDescriptorSet writeDescriptorSet = Init::writeDescriptorSet(image.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &image.texture.descriptor);
//	//	vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);
//	//}
//}
void Renderer::CreateRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachments = {};

	// Color attachment
	attachments[0].format = m_swapChain.colorFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Depth attachment
	attachments[1].format = FindDepthFormat();
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference{};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference{};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	// Subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies{};

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[0].dependencyFlags = 0;

	dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].dstSubpass = 0;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = 0;
	dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = 0;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}
VkFormat Renderer::FindDepthFormat()
{
	return FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

/// @brief 
/// @param candidates 
/// @param tiling 图片在设备中的存储格式
/// @param features 该格式需要支持的特性
/// @return 
VkFormat Renderer::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}
	throw std::runtime_error("failed to find supported format!");

}
VkImageView Renderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
	VkImageViewCreateInfo viewCI{};
	viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCI.image = image;
	viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCI.format = format;
	viewCI.subresourceRange.aspectMask = aspectFlags;
	viewCI.subresourceRange.baseMipLevel = 0;
	viewCI.subresourceRange.levelCount = mipLevels;
	viewCI.subresourceRange.baseArrayLayer = 0;
	viewCI.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(m_device, &viewCI, nullptr, &imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture image view!");
	}
	return imageView;
}

//为获取的交换链图像创建视图，封装
void Renderer::CreateSwapChainImageViews()
{
	m_swapChain.imageViews.resize(m_swapChain.images.size());
	for (size_t i = 0; i < m_swapChain.images.size(); ++i)
	{
		m_swapChain.imageViews[i] = CreateImageView(m_swapChain.images[i], m_swapChain.colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}
}
VkExtent2D Renderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);
		VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
		std::cout << "actualExtent: " << actualExtent.width << " " << actualExtent.height << std::endl;
		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		return actualExtent;
	}
};
VkPresentModeKHR Renderer::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

/// @brief 选择合适的交换链格式
/// @param availableFormats 
/// @return 合适的交换链格式
VkSurfaceFormatKHR Renderer::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}
	return availableFormats[0];
}

//void Renderer::CreateSwapChain()
//{
//	SwapChainSupportDetails& swapChainSupport = m_swapChainSupport;
//	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
//	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
//	VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);
//
//	// 设置交换链图像数量
//	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
//	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
//	{
//		imageCount = swapChainSupport.capabilities.maxImageCount;
//	}
//
//	VkSwapchainCreateInfoKHR createInfo{};
//	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
//	createInfo.surface = m_surface;
//	createInfo.minImageCount = imageCount;
//	createInfo.imageFormat = surfaceFormat.format;
//	createInfo.imageColorSpace = surfaceFormat.colorSpace;
//	createInfo.imageExtent = extent;
//	createInfo.imageArrayLayers = 1;
//	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
//
//	QueueFamilyIndices& indices = m_indices;
//	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
//	// 如果是两个队列族，交换链就需要在两个队列族之间共享图像
//	if (indices.graphicsFamily != indices.presentFamily)
//	{
//		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
//		createInfo.queueFamilyIndexCount = 2;
//		createInfo.pQueueFamilyIndices = queueFamilyIndices;
//	}
//	else
//	{
//		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
//		createInfo.queueFamilyIndexCount = 0; // Optional
//		createInfo.pQueueFamilyIndices = nullptr; // Optional
//	}
//	// 预变换，这里是不变换
//	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
//	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
//	createInfo.presentMode = presentMode;
//	createInfo.clipped = VK_TRUE;
//	createInfo.oldSwapchain = VK_NULL_HANDLE;
//
//	if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain.swapChain) != VK_SUCCESS)
//	{
//		throw std::runtime_error("failed to create swap chain!");
//	}
//
//	//查询交换链包含的图像数量，但不会返回图像句柄。这一步的目的是确定需要分配多少内存来存储这些图像。
//	vkGetSwapchainImagesKHR(m_device, m_swapChain.swapChain, &imageCount, nullptr);
//	m_swapChain.images.resize(imageCount);
//
//	//第二次调用 vkGetSwapchainImagesKHR：
//	//传递 mSwapChainImages.data()，将交换链的图像句柄存储到向量中。
//	// mSwapChainImages 现在包含交换链的所有图像句柄，后续可通过这些句柄操作每帧的渲染目标。
//	// Vulkan 管理这些图像的内存。开发者无需手动分配或释放，只需通过 VkSwapchainImagesKHR 获取句柄即可。
//	vkGetSwapchainImagesKHR(m_device, m_swapChain.swapChain, &imageCount, m_swapChain.images.data());
//	m_swapChain.colorFormat = surfaceFormat.format;
//	//m_swapChain.swapChainExtent = extent;
//
//	CreateSwapChainImageViews();
//}
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
	appIF.apiVersion = VK_API_VERSION_1_1;
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
	//std::cout << "RequiredExtensions: " << extensions.size() << std::endl;
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
	//std::cout << "system available layerCount: " << layerCount << std::endl;

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
	//std::cout << "system available layerCount: " << layerCount << std::endl;

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
				//std::cout << "layerName: " << layerName << " Available" << std::endl;
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
	//std::cout << "support deviceCount: " << deviceCount << std::endl;
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

void Renderer::CreateSyncObjects()
{
	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo = Init::semaphoreCreateInfo();
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queue
	VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_semaphores.presentComplete));
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been submitted and executed
	VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_semaphores.renderComplete));

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
	m_swapChainSupport = QuerySwapChainSupport(device);
	swapChainAdequate = !m_swapChainSupport.formats.empty() && !m_swapChainSupport.presentModes.empty();
	if (!swapChainAdequate)return false;

	// 且支持各向异性采样
	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	// TODO 改造为支持neededFeature 如果不支持feature 返回不支持的feature名称
	bool support = supportedFeatures.samplerAnisotropy && supportedFeatures.sampleRateShading;
	if (support) m_indices = indices;

	return support;
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


void Renderer::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}
void Renderer::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	if (action == GLFW_PRESS)  // 按键按下
	{
		switch (key)
		{
			//case GLFW_KEY_P:
			//	app->m_camera.paused = !paused;
			//	break;
			//case GLFW_KEY_F1:
			//	uiVisible = !uiVisible;
			//	break;
		case GLFW_KEY_F2:
			app->m_camera.type = (app->m_camera.type == Camera::CameraType::lookat) ? Camera::CameraType::firstperson : Camera::CameraType::lookat;
			break;
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, true);
			break;
		}

		if (app->m_camera.type == Camera::firstperson)
		{
			switch (key)
			{
			case GLFW_KEY_W: app->m_camera.keys.up = true; break;
			case GLFW_KEY_S: app->m_camera.keys.down = true; break;
			case GLFW_KEY_A: app->m_camera.keys.left = true; break;
			case GLFW_KEY_D: app->m_camera.keys.right = true; break;
			}
		}
	}
	else if (action == GLFW_RELEASE)  // 按键释放
	{
		if (app->m_camera.type == Camera::CameraType::firstperson)
		{
			switch (key)
			{
			case GLFW_KEY_W:  app->m_camera.keys.up = false; break;
			case GLFW_KEY_S:  app->m_camera.keys.down = false; break;
			case GLFW_KEY_A:  app->m_camera.keys.left = false; break;
			case GLFW_KEY_D:  app->m_camera.keys.right = false; break;
			}
		}
	}
}
void Renderer::MouseCallback(GLFWwindow* window, double xpos, double ypos)
{


	auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	auto& mouseState = app->m_mouseState;
	auto& camera = app->m_camera;
	int32_t dx = (int32_t)mouseState.Position.x - (int32_t)xpos;
	int32_t dy = (int32_t)mouseState.Position.y - (int32_t)ypos;

	bool handled = false;

	if (mouseState.Buttons.Left)
	{
		camera.rotate(glm::vec3(dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
		//viewUpdated = true;
	}
	if (mouseState.Buttons.Right)
	{
		camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f));
		//viewUpdated = true;
	}
	if (mouseState.Buttons.Middle)
	{
		camera.translate(glm::vec3(-dx * 0.005f, -dy * 0.005f, 0.0f));
		//viewUpdated = true;
	}
	mouseState.Position = glm::vec2((float)xpos, (float)ypos);
}
void Renderer::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));

	auto& mouseState = app->m_mouseState;
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
			mouseState.Buttons.Left = true;
		else if (action == GLFW_RELEASE)
			mouseState.Buttons.Left = false;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		if (action == GLFW_PRESS)
			mouseState.Buttons.Right = true;
		else if (action == GLFW_RELEASE)
			mouseState.Buttons.Right = false;
	}
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
	{
		if (action == GLFW_PRESS)
			mouseState.Buttons.Middle = true;
		else if (action == GLFW_RELEASE)
			mouseState.Buttons.Middle = false;
	}
}
void Renderer::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	app->m_camera.translate(glm::vec3(0.0f, 0.0f, (float)yoffset * 0.1f));
	/*app->viewUpdated = true;*/
}

void Renderer::DisplayUI(UIOverlay* overlay)
{
	if (overlay->Header("Settings"))
	{

		overlay->ComboBox("Display", &m_debugDisplayTarget, { "Final composition", "Shadows", "Position", "Normals", "Albedo", "Specular" });
		bool shadows = (m_uniformDataComposition.useShadows == 1);
		if (overlay->CheckBox("Shadows", &shadows))
		{
			m_uniformDataComposition.useShadows = shadows;
		}
	}
}
void Renderer::UpdateOverlay()
{
	m_UI.updateTimer -= m_frameTimer;
	if (m_UI.updateTimer >= 0.0f)
	{
		return;
	}
	// Update at max. rate of 30 fps
	m_UI.updateTimer = 1.0f / 30.0f;

	ImGuiIO& io = ImGui::GetIO();

	io.DisplaySize = ImVec2((float)m_width, (float)m_height);
	io.DeltaTime = m_frameTimer;

	io.MousePos = ImVec2(m_mouseState.Position.x, m_mouseState.Position.y);
	io.MouseDown[0] = m_mouseState.Buttons.Left && m_UI.visible;
	io.MouseDown[1] = m_mouseState.Buttons.Right && m_UI.visible;
	io.MouseDown[2] = m_mouseState.Buttons.Middle && m_UI.visible;

	ImGui::NewFrame();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::SetNextWindowPos(ImVec2(10 * m_UI.scale, 10 * m_UI.scale));
	ImGui::SetNextWindowSize(ImVec2(0, 0), 4);
	ImGui::Begin("MyToyRenderer", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
	ImGui::TextUnformatted(m_title.c_str());
	ImGui::TextUnformatted(m_vulkanDevice->properties.deviceName);
	ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / m_lastFPS), m_lastFPS);


	ImGui::PushItemWidth(110.0f * m_UI.scale);
	DisplayUI(&m_UI);
	ImGui::PopItemWidth();

	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::Render();

	if (m_UI.Update() || m_UI.updated)
	{
		BuildCommandBuffers();
		m_UI.updated = false;
	}


}
std::string Renderer::GetWindowTitle() const
{
	std::string windowTitle{ m_title + " - " + m_vulkanDevice->properties.deviceName };


	windowTitle += " - " + std::to_string(m_frameCounter) + " fps";

	return windowTitle;
}
void Renderer::MainLoop()
{

	while (!glfwWindowShouldClose(m_window))
	{
		auto tStart = std::chrono::high_resolution_clock::now();

		DrawFrame();

		m_frameCounter++;
		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		m_frameTimer = (float)tDiff / 1000.0f;
		m_camera.update(m_frameTimer);
		//std::cout << m_camera.moving();
		m_timer += m_timerSpeed * m_frameTimer;
		if (m_timer > 1.0)
		{
			m_timer -= 1.0f;
		}
		float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - m_lastTimestamp).count());
		if (fpsTimer > 1000.0f)
		{
			m_lastFPS = static_cast<uint32_t>((float)m_frameCounter * (1000.0f / fpsTimer));

			std::string windowTitle = GetWindowTitle();
			glfwSetWindowTitle(m_window, windowTitle.c_str());

			m_frameCounter = 0;
			m_lastTimestamp = tEnd;
		}
		m_tPrevEnd = tEnd;
		UpdateOverlay();
		glfwPollEvents();
	}
}

void Renderer::ResizeWindow()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);
	while (width == 0 || height == 0)
	{
		// 等待窗口事件，如窗口大小改变或其他事件
		glfwWaitEvents();
		glfwGetFramebufferSize(m_window, &width, &height);
	}
	vkDeviceWaitIdle(m_device);
	m_width = static_cast<uint32_t>(width);
	m_height = static_cast<uint32_t>(height);
	m_swapChain.Create(m_width, m_height, false, false);

	// Recreate the frame buffers
	vkDestroyImageView(m_device, m_depthStencil.view, nullptr);
	vkDestroyImage(m_device, m_depthStencil.image, nullptr);
	vkFreeMemory(m_device, m_depthStencil.memory, nullptr);
	CreateDepthResources();
	for (auto& frameBuffer : m_frameBuffers)
	{
		vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
	}
	CreateFramebuffers();
	if ((m_width > 0.0f) && (m_height > 0.0f))
	{
		m_UI.Resize(m_width, m_height);
	}

	// Command buffers need to be recreated as they may store
	// references to the recreated frame buffer
	vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_drawCmdBuffers.size()), m_drawCmdBuffers.data());
	CreateCommandBuffers();
	BuildCommandBuffers();

	// SRS - Recreate fences in case number of swapchain images has changed on resize
	//for (auto& fence : m_waitFences)
	//{
	//	vkDestroyFence(m_device, fence, nullptr);
	//}
	//CreateSyncObjects();

	vkDeviceWaitIdle(m_device);

	if ((m_width > 0.0f) && (m_height > 0.0f))
	{
		m_camera.updateAspectRatio((float)m_width / (float)m_height);
	}
}
void Renderer::PrepareFrame()
{

	VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain.swapChain, UINT64_MAX, m_semaphores.presentComplete, (VkFence)nullptr, &currentBuffer);

	if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
	{
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			ResizeWindow();
		}
		return;
	}
	else
	{
		VK_CHECK_RESULT(result);
	}
}
//void Renderer::UpdateUniformBuffers()
//{
//	m_uboMatrices.proj = m_camera.matrices.perspective;
//	m_uboMatrices.view = m_camera.matrices.view;
//	//camera.matrices.view;
//	m_uboMatrices.lightPos = glm::vec3(0.0f, 100.0f, 0.0f);
//	//m_camera.position.x = -m_camera.position.x;
//	m_uboMatrices.camPos = m_camera.GetCameraPos();
//	//std::cout << m_uboMatrices.camPos.x <<"," << m_uboMatrices.camPos.y <<"," << m_uboMatrices.camPos.z << std::endl;
//	memcpy(m_uboBuffer.mapped, &m_uboMatrices, sizeof(m_uboMatrices));
//}

void Renderer::Draw()
{
	m_submitInfo.pWaitSemaphores = &m_semaphores.presentComplete;
	m_submitInfo.pSignalSemaphores = &m_semaphores.deferedSemaphore;
	m_submitInfo.commandBufferCount = 1;
	m_submitInfo.pCommandBuffers = &m_offScreenCmdBuffer;
	VK_CHECK_RESULT(vkQueueSubmit(m_queues.graphicsQueue, 1, &m_submitInfo, VK_NULL_HANDLE));

	m_submitInfo.pWaitSemaphores = &m_semaphores.deferedSemaphore;
	m_submitInfo.pSignalSemaphores = &m_semaphores.renderComplete;
	m_submitInfo.pCommandBuffers = &m_drawCmdBuffers[currentBuffer];
	VK_CHECK_RESULT(vkQueueSubmit(m_queues.graphicsQueue, 1, &m_submitInfo, VK_NULL_HANDLE));
}
void Renderer::SubmitFrame()
{
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapChain.swapChain;
	presentInfo.pImageIndices = &currentBuffer;
	// Check if a wait semaphore has been specified to wait for before presenting the image
	if (m_semaphores.renderComplete != VK_NULL_HANDLE)
	{
		presentInfo.pWaitSemaphores = &m_semaphores.renderComplete;
		presentInfo.waitSemaphoreCount = 1;
	}
	VkResult result = vkQueuePresentKHR(m_queues.presentQueue, &presentInfo);

	// Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
	if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
	{
		ResizeWindow();
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			return;
		}
	}
	else
	{
		VK_CHECK_RESULT(result);
	}
	VK_CHECK_RESULT(vkQueueWaitIdle(m_queues.graphicsQueue));

}

// 信号量一组
// 无fence
// commande buffer 和 swapchain size皆为3
// 这里的绘制逻辑是逐帧渲染，，所以下一帧提交前所以不会有资源冲突问题，
// 因为这里不需要重新buildcommandbufer（没有东西改变），所以inflight 没有用（不需要进行tutorial中的同时多帧处理）
void Renderer::DrawFrame()
{
	UpdateUniformBufferComposition();
	UpdateUniformBufferOffscreen();
	PrepareFrame();
	Draw();
	SubmitFrame();
	//currentBuffer = (currentBuffer + 1) % m_swapChain.images.size();
}

void Renderer::Cleanup()
{
	// cmd buffer
	vkDestroyCommandPool(m_device, m_commandPool, nullptr);

	// depth 
	vkDestroyImageView(m_device, m_depthStencil.view, nullptr);
	vkDestroyImage(m_device, m_depthStencil.image, nullptr);
	vkFreeMemory(m_device, m_depthStencil.memory, nullptr);

	// semaphore
	vkDestroySemaphore(m_device, m_semaphores.presentComplete, nullptr);
	vkDestroySemaphore(m_device, m_semaphores.renderComplete, nullptr);

	//vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.Textures, nullptr);
	//vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.Matrices, nullptr);
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	vkDestroyPipeline(m_device, m_pipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_device, m_renderPass, nullptr);

	for (auto& frameBuffer : m_frameBuffers)
	{
		vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
	}
	for (auto& shaderModule : m_shaderModules)
	{
		vkDestroyShaderModule(m_device, shaderModule, nullptr);
	}
	// uniformbuffer
	m_uboBuffer.Destroy();

	/*m_glTFModel.Destroy();*/
	m_swapChain.Cleanup();

	m_UI.FreeResources();

	m_glTFModel.Destroy();
	if (m_neededFeatures.validation)
	{
		DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
	}

	glfwDestroyWindow(m_window);
	glfwTerminate();

	delete m_vulkanDevice;
	vkDestroyInstance(m_instance, nullptr);
}


void Renderer::CreateLogicalDevice()
{
	// 设置队列属性 还没创建队列
	QueueFamilyIndices& indices = m_indices;
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

	// 某个显卡支持特定的队列族，某些队列族只支持渲染，某些只支持显示，某些两者都支持
	// 之前已经判断过物理设备一定支持这两个队列族，这里是防止因为他们相同导致重复创建队列
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };
	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		// 只创建该队列族的一个队列
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}
	// 物理设备属性
	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	// 这里 没有判断，而是指定让物理设备开启这些功能
	// TODO
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.sampleRateShading = VK_TRUE;
	createInfo.pEnabledFeatures = &deviceFeatures; // 使用设备特性

	// 指定启用的扩展（交换链）获取物理设备时已判断扩展是否可用
	createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

	if (m_neededFeatures.validation)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
		createInfo.ppEnabledLayerNames = m_validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}
	if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create logical device");
	}
	// 保存队列
	vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_queues.graphicsQueue);
	vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_queues.presentQueue);
}

void Renderer::LoadAssets()
{
	const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors;
	/*LoadglTFFile(Tool::GetAssetsPath() + "Models/FlightHelmet/glTF/FlightHelmet.gltf");*/
	m_glTFModel.loadFromFile(Tool::GetAssetsPath() + m_config->modelPath, m_vulkanDevice, m_queues.graphicsQueue, glTFLoadingFlags);
	//std::cout << std::endl << "sizeof material: " << m_glTFModel.materials.size() << std::endl;
	//std::cout << std::endl << "sizeof texture: " << m_glTFModel.textures.size() << std::endl;
}


void Renderer::PrepareUniformBuffers()
{
	//CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_uboBuffer, sizeof(m_uboMatrices));
//m_uboBuffer.Map();
	VK_CHECK_RESULT(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_uniformBuffers.defered, sizeof(UniformDataOffscreen)));
	VK_CHECK_RESULT(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_uniformBuffers.composition, sizeof(UniformDataComposition)));
	VK_CHECK_RESULT(m_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_uniformBuffers.shadowGeometryShader, sizeof(UniformDataShadows)));

	// Map persistent
	VK_CHECK_RESULT(m_uniformBuffers.defered.Map());
	VK_CHECK_RESULT(m_uniformBuffers.composition.Map());
	VK_CHECK_RESULT(m_uniformBuffers.shadowGeometryShader.Map());

}
//void Renderer::SetupDescriptors()
//{
//	// Pool
//	std::vector<VkDescriptorPoolSize> poolSizes = {
//		Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 12),
//		Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 15)
//	};
//	VkDescriptorPoolCreateInfo descriptorPoolInfo = Init::descriptorPoolCreateInfo(poolSizes, 4);
//	VK_CHECK_RESULT(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool));
//
//	// Layouts 通用 
//	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
//		// Binding 0 : Vertex shader uniform buffer
//		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
//		// Binding 1 : Position texture target / Scene colormap
//		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
//		// Binding 2 : Normals texture target
//		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
//		// Binding 3 : Albedo texture target
//		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
//		// Binding 4 : Fragment shader uniform buffer
//		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
//	};
//	VkDescriptorSetLayoutCreateInfo descriptorLayout = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
//	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_defered.descriptorSetLayout));
//
//	setLayoutBindings = {
//		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0)
//	};
//	VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
//	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_defered.descriptorSetLayouts.textures));
//
//
//	// Sets
//	std::vector<VkWriteDescriptorSet> writeDescriptorSets;
//	VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_defered.descriptorSetLayout, 1);
//	// Deferred composition
//	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_defered.descriptorSets.composition));
//
//	// Image descriptors for the offscreen color attachments
//	VkDescriptorImageInfo texDescriptorPosition =
//		Init::descriptorImageInfo(
//			m_defered.colorSampler,
//			m_defered.offScreenFrameBuf.position.view,
//			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//
//	VkDescriptorImageInfo texDescriptorNormal =
//		Init::descriptorImageInfo(
//			m_defered.colorSampler,
//			m_defered.offScreenFrameBuf.normal.view,
//			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//
//	VkDescriptorImageInfo texDescriptorAlbedo =
//		Init::descriptorImageInfo(
//			m_defered.colorSampler,
//			m_defered.offScreenFrameBuf.albedo.view,
//			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//
//	writeDescriptorSets = {
//		// Binding 1 : Position texture target
//		Init::writeDescriptorSet(m_defered.descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorPosition),
//		// Binding 2 : Normals texture target
//		Init::writeDescriptorSet(m_defered.descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorNormal),
//		// Binding 3 : Albedo texture target
//		Init::writeDescriptorSet(m_defered.descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texDescriptorAlbedo),
//		// Binding 4 : Fragment shader uniform buffer
//		Init::writeDescriptorSet(m_defered.descriptorSets.composition, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4, &m_defered.uniformBuffers.composition.descriptor),
//	};
//	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
//
//	// Offscreen (scene)
//
//	// Model
//	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_defered.descriptorSets.model));
//	writeDescriptorSets = {
//		// Binding 0: Vertex shader uniform buffer
//		Init::writeDescriptorSet(m_defered.descriptorSets.model, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_defered.uniformBuffers.offscreen.descriptor),
//		// Binding 1: Color map
//		//Init::writeDescriptorSet(m_defered.descriptorSets.model, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.model.colorMap.descriptor),
//		//// Binding 2: Normal map
//		//Init::writeDescriptorSet(m_defered.descriptorSets.model, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.model.normalMap.descriptor)
//	};
//	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
//}

//void Renderer::PreparePipelines()
//{
//	std::vector<VkDescriptorSetLayout> setLayouts = { m_defered.descriptorSetLayout };
//	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(setLayouts.data(), 1);
//	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_defered.pipelineLayouts.composition));
//
//	setLayouts.push_back(m_defered.descriptorSetLayouts.textures);
//	pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(setLayouts.data(), 2);
//	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_defered.pipelineLayouts.model));
//
//	// Pipelines
//	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
//	VkPipelineRasterizationStateCreateInfo rasterizationState = Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
//	VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
//	VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
//	VkPipelineDepthStencilStateCreateInfo depthStencilState = Init::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
//	VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1, 0);
//	VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
//	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
//	VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
//	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
//
//	VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_defered.pipelineLayouts.composition, m_renderPass);
//	pipelineCI.pInputAssemblyState = &inputAssemblyState;
//	pipelineCI.pRasterizationState = &rasterizationState;
//	pipelineCI.pColorBlendState = &colorBlendState;
//	pipelineCI.pMultisampleState = &multisampleState;
//	pipelineCI.pViewportState = &viewportState;
//	pipelineCI.pDepthStencilState = &depthStencilState;
//	pipelineCI.pDynamicState = &dynamicState;
//	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
//	pipelineCI.pStages = shaderStages.data();
//
//	// Final fullscreen composition pass pipeline，这里与着色器顶点顺序相关
//	rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
//	shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Defered/Composition.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
//	shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Defered/Composition.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
//
//	VkPipelineVertexInputStateCreateInfo emptyInputState = Init::pipelineVertexInputStateCreateInfo();
//	pipelineCI.pVertexInputState = &emptyInputState;
//	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_defered.pipelines.composition));
//
//	// model
//	pipelineCI.layout = m_defered.pipelineLayouts.model;
//	// Vertex input state from glTF model for pipeline rendering models
//	pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal });
//	rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
//
//	// Offscreen pipeline
//	shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Defered/Model.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
//	shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Defered/Model.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
//
//	// Separate render pass
//	pipelineCI.renderPass = m_defered.offScreenFrameBuf.renderPass;
//
//	// Blend attachment states required for all color attachments
//	// This is important, as color write mask will otherwise be 0x0 and you
//	// won't see anything rendered to the attachment
//	std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates = {
//		Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
//		Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
//		Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE)
//	};
//	colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
//	colorBlendState.pAttachments = blendAttachmentStates.data();
//
//	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_defered.pipelines.offscreen));
//}

void Renderer::CreatePipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(m_device, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache));
}
// model
void Renderer::BuildDeferredCommandBuffer()
{
	if (m_offScreenCmdBuffer == VK_NULL_HANDLE)
	{
		m_offScreenCmdBuffer = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
	}

	// Create a semaphore used to synchronize offscreen rendering and usage
	VkSemaphoreCreateInfo semaphoreCreateInfo = Init::semaphoreCreateInfo();
	VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_semaphores.deferedSemaphore));

	VkCommandBufferBeginInfo cmdBufInfo = Init::commandBufferBeginInfo();

	// Clear values for all attachments written in the fragment shader
	std::array<VkClearValue, 4> clearValues = {};
	clearValues[0].depthStencil = { 1.0f, 0 };
	// First pass: Shadow map generation

	VkRenderPassBeginInfo renderPassBeginInfo = Init::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = m_framebuffers.shadow->renderPass;
	renderPassBeginInfo.framebuffer = m_framebuffers.shadow->framebuffer;
	renderPassBeginInfo.renderArea.extent.width = m_framebuffers.shadow->width;
	renderPassBeginInfo.renderArea.extent.height = m_framebuffers.shadow->height;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = clearValues.data();

	VK_CHECK_RESULT(vkBeginCommandBuffer(m_offScreenCmdBuffer, &cmdBufInfo));

	vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = Init::viewport((float)m_framebuffers.shadow->width, (float)m_framebuffers.shadow->height, 0.0f, 1.0f);
	vkCmdSetViewport(m_offScreenCmdBuffer, 0, 1, &viewport);

	VkRect2D scissor = Init::rect2D(m_framebuffers.shadow->width, m_framebuffers.shadow->height, 0, 0);
	vkCmdSetScissor(m_offScreenCmdBuffer, 0, 1, &scissor);

	// Set depth bias (aka "Polygon offset")
	vkCmdSetDepthBias(
		m_offScreenCmdBuffer,
		m_shadowSettings.depthBiasConstant,
		0.0f,
		m_shadowSettings.depthBiasSlope);

	vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.shadow);

	// We render multiple instances of a model
	vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.composition, 0, 1, &m_descriptorSets.shadow, 0, nullptr);
	m_glTFModel.Draw(m_offScreenCmdBuffer);

	vkCmdEndRenderPass(m_offScreenCmdBuffer);

	// second pass
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[3].depthStencil = { 1.0f, 0 };

	renderPassBeginInfo.renderPass = m_framebuffers.deferred->renderPass;
	renderPassBeginInfo.framebuffer = m_framebuffers.deferred->framebuffer;
	renderPassBeginInfo.renderArea.extent.width = m_framebuffers.deferred->width;
	renderPassBeginInfo.renderArea.extent.height = m_framebuffers.deferred->height;
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	viewport = Init::viewport((float)m_framebuffers.deferred->width, (float)m_framebuffers.deferred->height, 0.0f, 1.0f);
	vkCmdSetViewport(m_offScreenCmdBuffer, 0, 1, &viewport);

	scissor = Init::rect2D(m_framebuffers.deferred->width, m_framebuffers.deferred->height, 0, 0);
	vkCmdSetScissor(m_offScreenCmdBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.defered);
	vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.defered, 0, 1, &m_descriptorSets.deferedModel, 0, nullptr);
	m_glTFModel.Draw(m_offScreenCmdBuffer, vkglTF::RenderFlags::BindImages, m_pipelineLayouts.defered, 1);

	vkCmdEndRenderPass(m_offScreenCmdBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(m_offScreenCmdBuffer));
}

void Renderer::SetupShadow()
{
	m_framebuffers.shadow = new Framebuffer(m_vulkanDevice);

	m_framebuffers.shadow->width = 2048;
	m_framebuffers.shadow->height = 2048;

	// Find a suitable depth format
	VkFormat shadowMapFormat;
	VkBool32 validShadowMapFormat = Tool::GetSupportedDepthFormat(m_physicalDevice, &shadowMapFormat);
	assert(validShadowMapFormat);

	// Create a layered depth attachment for rendering the depth maps from the lights' point of view
	// Each layer corresponds to one of the lights
	// The actual output to the separate layers is done in the geometry shader using shader instancing
	// We will pass the matrices of the lights to the GS that selects the layer by the current invocation
	AttachmentCreateInfo attachmentInfo = {};
	attachmentInfo.format = shadowMapFormat;
	attachmentInfo.width = m_framebuffers.shadow->width;
	attachmentInfo.height = m_framebuffers.shadow->height;
	attachmentInfo.layerCount = LIGHT_COUNT;
	attachmentInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	m_framebuffers.shadow->AddAttachment(attachmentInfo);

	// Create sampler to sample from to depth attachment
	// Used to sample in the fragment shader for shadowed rendering
	VK_CHECK_RESULT(m_framebuffers.shadow->CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));

	// Create default renderpass for the framebuffer
	VK_CHECK_RESULT(m_framebuffers.shadow->CreateRenderPass());
};
void Renderer::SetupDefered()
{
	m_framebuffers.deferred = new Framebuffer(m_vulkanDevice);
	m_framebuffers.deferred->width = 2048;
	m_framebuffers.deferred->height = 2048;

	AttachmentCreateInfo attachmentInfo = {};
	attachmentInfo.width = m_framebuffers.deferred->width;
	attachmentInfo.height = m_framebuffers.deferred->height;
	attachmentInfo.layerCount = 1;
	attachmentInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	// Color attachments
	// Attachment 0: (World space) Positions
	attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	m_framebuffers.deferred->AddAttachment(attachmentInfo);

	// Attachment 1: (World space) Normals
	attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	m_framebuffers.deferred->AddAttachment(attachmentInfo);

	// Attachment 2: Albedo (color)
	attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	m_framebuffers.deferred->AddAttachment(attachmentInfo);

	// Depth attachment
	// Find a suitable depth format
	VkFormat attDepthFormat;
	VkBool32 validDepthFormat = Tool::GetSupportedDepthFormat(m_physicalDevice, &attDepthFormat);
	assert(validDepthFormat);

	attachmentInfo.format = attDepthFormat;
	attachmentInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	m_framebuffers.deferred->AddAttachment(attachmentInfo);

	// Create sampler to sample from the color attachments
	VK_CHECK_RESULT(m_framebuffers.deferred->CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));

	// Create default renderpass for the framebuffer
	VK_CHECK_RESULT(m_framebuffers.deferred->CreateRenderPass());

};
void Renderer::InitLights()
{
	m_uniformDataComposition.lights[0] = InitLight(glm::vec3(10.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	m_uniformDataComposition.lights[1] = InitLight(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	m_uniformDataComposition.lights[2] = InitLight(glm::vec3(-10.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
};

void Renderer::SetupDescriptorsDD()
{
	// Pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16),
		Init::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 20)
	};
	VkDescriptorPoolCreateInfo descriptorPoolInfo = Init::descriptorPoolCreateInfo(poolSizes, 5);
	VK_CHECK_RESULT(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool));

	// Layout
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: Vertex shader uniform buffer
		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0),
		// Binding 1: Position texture
		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		// Binding 2: Normals texture
		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
		// Binding 3: Albedo texture
		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
		// Binding 4: Fragment shader uniform buffer
		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
		// Binding 5: Shadow map
		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
	};
	VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_descriptorSetLayouts.composition));
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_descriptorSetLayouts.deferedModel));
	setLayoutBindings = {
		Init::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0)
	};
	descriptorLayoutCI = Init::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayoutCI, nullptr, &m_descriptorSetLayouts.deferedTextures));

	// Sets
	std::vector<VkWriteDescriptorSet> writeDescriptorSets;
	VkDescriptorSetAllocateInfo allocInfo = Init::descriptorSetAllocateInfo(m_descriptorPool, &m_descriptorSetLayouts.composition, 1);
	// Deferred composition
	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.composition));

	// Image descriptors for the offscreen color attachments
	VkDescriptorImageInfo texDescriptorPosition =
		Init::descriptorImageInfo(
			m_framebuffers.deferred->sampler,
			m_framebuffers.deferred->attachments[0].view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkDescriptorImageInfo texDescriptorNormal =
		Init::descriptorImageInfo(
			m_framebuffers.deferred->sampler,
			m_framebuffers.deferred->attachments[1].view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkDescriptorImageInfo texDescriptorAlbedo =
		Init::descriptorImageInfo(
			m_framebuffers.deferred->sampler,
			m_framebuffers.deferred->attachments[2].view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkDescriptorImageInfo texDescriptorShadowMap =
		Init::descriptorImageInfo(
			m_framebuffers.shadow->sampler,
			m_framebuffers.shadow->attachments[0].view,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
	writeDescriptorSets = {
		// Binding 1: World space position texture
		Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorPosition),
		// Binding 2: World space normals texture
		Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorNormal),
		// Binding 3: Albedo texture
		Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texDescriptorAlbedo),
		// Binding 4: Fragment shader uniform buffer
		Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4, &m_uniformBuffers.composition.descriptor),
		// Binding 5: Shadow map
		Init::writeDescriptorSet(m_descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, &texDescriptorShadowMap),
	};
	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);


	// offscreen
	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.deferedModel));

	writeDescriptorSets = {
		// Binding 0: Vertex shader uniform buffer
		Init::writeDescriptorSet(m_descriptorSets.deferedModel, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_uniformBuffers.defered.descriptor)
	};
	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

	// Shadow mapping
	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.shadow));
	writeDescriptorSets = {
		// Binding 0: Vertex shader uniform buffer
		Init::writeDescriptorSet(m_descriptorSets.shadow, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_uniformBuffers.shadowGeometryShader.descriptor),
	};
	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
void Renderer::PreparePipelinesDD()
{
	// Layout
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(&m_descriptorSetLayouts.composition, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.composition));

	std::array<VkDescriptorSetLayout, 2> setLayouts = { m_descriptorSetLayouts.deferedModel ,m_descriptorSetLayouts.deferedTextures };
	pipelineLayoutCreateInfo = Init::pipelineLayoutCreateInfo(setLayouts.data(), 2);
	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.defered));

	// Pipelines

	// composition
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationState = Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VkPipelineDepthStencilStateCreateInfo depthStencilState = Init::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_pipelineLayouts.composition, m_renderPass);
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();

	// Final fullscreen composition pass pipeline
	rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
	shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Deferedshadows/Composition.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Deferedshadows/Composition.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	// Empty vertex input state, vertices are generated by the vertex shader
	VkPipelineVertexInputStateCreateInfo emptyInputState = Init::pipelineVertexInputStateCreateInfo();
	pipelineCI.pVertexInputState = &emptyInputState;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_pipelines.composition));

	// Vertex input state from glTF model for pipeline rendering models
	pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal });
	rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

	// Offscreen pipeline
	// Separate render pass
	pipelineCI.layout = m_pipelineLayouts.defered;
	pipelineCI.renderPass = m_framebuffers.deferred->renderPass;

	// Blend attachment states required for all color attachments
	// This is important, as color write mask will otherwise be 0x0 and you
	// won't see anything rendered to the attachment
	std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates =
	{
		Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
		Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
		Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE)
	};
	colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
	colorBlendState.pAttachments = blendAttachmentStates.data();

	shaderStages[0] = LoadShader(Tool::GetShadersPath() + "Deferedshadows/Model.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(Tool::GetShadersPath() + "Deferedshadows/Model.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_pipelines.defered));

	// Shadow mapping pipeline
	// The shadow mapping pipeline uses geometry shader instancing (invocations layout modifier) to output
	// shadow maps for multiple lights sources into the different shadow map layers in one single render pass

	pipelineCI.layout = m_pipelineLayouts.composition;

	std::array<VkPipelineShaderStageCreateInfo, 2> shadowStages;
	shadowStages[0] = LoadShader(Tool::GetShadersPath() + "Deferedshadows/Shadow.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shadowStages[1] = LoadShader(Tool::GetShadersPath() + "Deferedshadows/Shadow.Geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

	pipelineCI.pStages = shadowStages.data();
	pipelineCI.stageCount = static_cast<uint32_t>(shadowStages.size());

	// Shadow pass doesn't use any color attachments
	colorBlendState.attachmentCount = 0;
	colorBlendState.pAttachments = nullptr;
	// Cull front faces
	rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	// Enable depth bias
	rasterizationState.depthBiasEnable = VK_TRUE;
	// Add depth bias to dynamic state, so we can change it at runtime
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
	dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);
	// Reset blend attachment state
	pipelineCI.renderPass = m_framebuffers.shadow->renderPass;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCI, nullptr, &m_pipelines.shadow));

}

