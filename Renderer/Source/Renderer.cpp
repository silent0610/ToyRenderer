module;
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <memory>
#include "glm/mat4x4.hpp"
#include "stb/stb_image.h"
#include "tiny_gltf.h"
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
Renderer::Renderer(bool enableValidation)
{
	m_neededFeatures.validation = enableValidation;
	m_camera.type = Camera::CameraType::lookat;
	m_camera.flipY = true;
	m_camera.setPosition(glm::vec3(0.0f, 0.0f, -3.f));
	m_camera.setRotation(glm::vec3(0.0f));
	m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);

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
	glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
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

void Renderer::EncapsulationDevice() {
	VkResult result;
	m_vulkanDevice = new VulkanDevice(m_physicalDevice);
	
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

	LoadAssets();

	// prepare
	//CreateSwapChain();
	CreateCommandPool();
	CreateCommandBuffers();
	CreateDepthResources();
	CreateRenderPass();
	CreateFramebuffers();

	
	CreateUniformBuffer();
	CreateDescriptors();
	CreateGraphicsPipeline();
	CreateVertexBuffer();
	BuildCommandBuffers();


}

// Prepare vertex and index buffers for an indexed triangle
	// Also uploads them to device local memory using staging and initializes vertex input and attribute binding to match the vertex shader
void Renderer::CreateVertexBuffer()
{
	// A note on memory management in Vulkan in general:
	//	This is a very complex topic and while it's fine for an example application to small individual memory allocations that is not
	//	what should be done a real-world application, where you should allocate large chunks of memory at once instead.

	// Setup vertices
	std::vector<Vertex> vertexBuffer{
		{ {  1.0f,  -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 0.0f,  1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
		{ {  -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
	};
	uint32_t vertexBufferSize = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(Vertex);

	// Setup indices
	std::vector<uint32_t> indexBuffer{ 0, 1, 2 };
	indices.count = static_cast<uint32_t>(indexBuffer.size());
	uint32_t indexBufferSize = indices.count * sizeof(uint32_t);

	VkMemoryAllocateInfo memAlloc{};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;

	// Static data like vertex and index buffer should be stored on the device memory for optimal (and fastest) access by the GPU
	//
	// To achieve this we use so-called "staging buffers" :
	// - Create a buffer that's visible to the host (and can be mapped)
	// - Copy the data to this buffer
	// - Create another buffer that's local on the device (VRAM) with the same size
	// - Copy the data from the host to the device using a command buffer
	// - Delete the host visible (staging) buffer
	// - Use the device local buffers for rendering
	//
	// Note: On unified memory architectures where host (CPU) and GPU share the same memory, staging is not necessary
	// To keep this sample easy to follow, there is no check for that in place

	struct StagingBuffer
	{
		VkDeviceMemory memory;
		VkBuffer buffer;
	};

	struct
	{
		StagingBuffer vertices;
		StagingBuffer indices;
	} stagingBuffers{};

	void* data;

	// Vertex buffer
	VkBufferCreateInfo vertexBufferInfoCI{};
	vertexBufferInfoCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferInfoCI.size = vertexBufferSize;
	// Buffer is used as the copy source
	vertexBufferInfoCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	// Create a host-visible buffer to copy the vertex data to (staging buffer)
	VK_CHECK_RESULT(vkCreateBuffer(m_device, &vertexBufferInfoCI, nullptr, &stagingBuffers.vertices.buffer));
	vkGetBufferMemoryRequirements(m_device, stagingBuffers.vertices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	// Request a host visible memory type that can be used to copy our data to
	// Also request it to be coherent, so that writes are visible to the GPU right after unmapping the buffer
	memAlloc.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAlloc, nullptr, &stagingBuffers.vertices.memory));
	// Map and copy
	VK_CHECK_RESULT(vkMapMemory(m_device, stagingBuffers.vertices.memory, 0, memAlloc.allocationSize, 0, &data));
	memcpy(data, vertexBuffer.data(), vertexBufferSize);
	vkUnmapMemory(m_device, stagingBuffers.vertices.memory);
	VK_CHECK_RESULT(vkBindBufferMemory(m_device, stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0));

	// Create a m_device local buffer to which the (host local) vertex data will be copied and which will be used for rendering
	vertexBufferInfoCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(m_device, &vertexBufferInfoCI, nullptr, &vertices.buffer));
	vkGetBufferMemoryRequirements(m_device, vertices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAlloc, nullptr, &vertices.memory));
	VK_CHECK_RESULT(vkBindBufferMemory(m_device, vertices.buffer, vertices.memory, 0));

	// Index buffer
	VkBufferCreateInfo indexbufferCI{};
	indexbufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	indexbufferCI.size = indexBufferSize;
	indexbufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	// Copy index data to a buffer visible to the host (staging buffer)
	VK_CHECK_RESULT(vkCreateBuffer(m_device, &indexbufferCI, nullptr, &stagingBuffers.indices.buffer));
	vkGetBufferMemoryRequirements(m_device, stagingBuffers.indices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAlloc, nullptr, &stagingBuffers.indices.memory));
	VK_CHECK_RESULT(vkMapMemory(m_device, stagingBuffers.indices.memory, 0, indexBufferSize, 0, &data));
	memcpy(data, indexBuffer.data(), indexBufferSize);
	vkUnmapMemory(m_device, stagingBuffers.indices.memory);
	VK_CHECK_RESULT(vkBindBufferMemory(m_device, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0));

	// Create destination buffer with m_device only visibility
	indexbufferCI.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(m_device, &indexbufferCI, nullptr, &indices.buffer));
	vkGetBufferMemoryRequirements(m_device, indices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAlloc, nullptr, &indices.memory));
	VK_CHECK_RESULT(vkBindBufferMemory(m_device, indices.buffer, indices.memory, 0));

	// Buffer copies have to be submitted to a queue, so we need a command buffer for them
	// Note: Some devices offer a dedicated transfer queue (with only the transfer bit set) that may be faster when doing lots of copies
	VkCommandBuffer copyCmd;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = m_commandPool;
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = 1;
	VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, &copyCmd));

	VkCommandBufferBeginInfo cmdBufInfo = Init::commandBufferBeginInfo();
	VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));
	// Put buffer region copies into command buffer
	VkBufferCopy copyRegion{};
	// Vertex buffer
	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(copyCmd, stagingBuffers.vertices.buffer, vertices.buffer, 1, &copyRegion);
	// Index buffer
	copyRegion.size = indexBufferSize;
	vkCmdCopyBuffer(copyCmd, stagingBuffers.indices.buffer, indices.buffer, 1, &copyRegion);
	VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

	// Submit the command buffer to the queue to finish the copy
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &copyCmd;

	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo fenceCI{};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCI.flags = 0;
	VkFence fence;
	VK_CHECK_RESULT(vkCreateFence(m_device, &fenceCI, nullptr, &fence));

	// Submit to the queue
	VK_CHECK_RESULT(vkQueueSubmit(m_queues.graphicsQueue, 1, &submitInfo, fence));
	// Wait for the fence to signal that command buffer has finished executing
	VK_CHECK_RESULT(vkWaitForFences(m_device, 1, &fence, VK_TRUE, 10000000));

	vkDestroyFence(m_device, fence, nullptr);
	vkFreeCommandBuffers(m_device, m_commandPool, 1, &copyCmd);

	// Destroy staging buffers
	// Note: Staging buffer must not be deleted before the copies have been submitted and executed
	vkDestroyBuffer(m_device, stagingBuffers.vertices.buffer, nullptr);
	vkFreeMemory(m_device, stagingBuffers.vertices.memory, nullptr);
	vkDestroyBuffer(m_device, stagingBuffers.indices.buffer, nullptr);
	vkFreeMemory(m_device, stagingBuffers.indices.memory, nullptr);
}

void::Renderer::CreateCommandBuffers()
{
	// Create one command buffer for each swap chain image
	m_drawCmdBuffers.resize(m_swapChain.images.size());
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = Init::commandBufferAllocateInfo(m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(m_drawCmdBuffers.size()));
	VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, m_drawCmdBuffers.data()));
}
void Renderer::BuildCommandBuffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = Init::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.5f, 0.025f, 0.025f, 1.0f } };
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

		vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
		vkCmdBindDescriptorSets(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, NULL);

		glm::mat4x4 model{ 1.0f };
		vkCmdPushConstants(m_drawCmdBuffers[i], m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4x4), &model);
		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(m_drawCmdBuffers[i], 0, 1, &vertices.buffer, offsets);
		vkCmdBindIndexBuffer(m_drawCmdBuffers[i], indices.buffer, 0, VK_INDEX_TYPE_UINT32);

		m_glTFModel.Draw(m_drawCmdBuffers[i], m_pipelineLayout);
		//drawUI(drawCmdBuffers[i]);
		vkCmdDrawIndexed(m_drawCmdBuffers[i], indices.count, 1, 0, 0, 0);
		vkCmdEndRenderPass(m_drawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]));
	}
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

std::string Renderer::GetShadersPath()
{
	auto path = std::filesystem::current_path();
	auto projectPath = path.parent_path().parent_path().parent_path().parent_path();
	return projectPath.string() + "/Renderer/Shader/";
}
std::string Renderer::GetAssetsPath() {

	auto path = std::filesystem::current_path();
	auto projectPath = path.parent_path().parent_path().parent_path().parent_path();
	return projectPath.string() + "/Renderer/Asset/";
}
void Renderer::CreateGraphicsPipeline()
{
	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.pSetLayouts = &m_descriptorSetLayout;
	pipelineLayoutCI.setLayoutCount = 1;

	VkPushConstantRange pushConstRange{};
	pushConstRange.offset = 0;
	pushConstRange.size = sizeof(glm::mat4x4);
	pushConstRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pipelineLayoutCI.pushConstantRangeCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &pushConstRange;

	if (vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &m_pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("fail to create Pipelinelayout");
	}

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = Init::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationState = Init::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	VkPipelineColorBlendAttachmentState blendAttachmentState = Init::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendState = Init::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VkPipelineDepthStencilStateCreateInfo depthStencilState = Init::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportState = Init::pipelineViewportStateCreateInfo(1, 1);
	VkPipelineMultisampleStateCreateInfo multisampleState = Init::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState = Init::pipelineDynamicStateCreateInfo(dynamicStateEnables);

	// Vertex input state
	std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		Init::vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
	};
	std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		Init::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)),
		Init::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color))
		//Init::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)),
		//Init::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)),
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState = Init::pipelineVertexInputStateCreateInfo();
	vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
	vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
	vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
	vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

	VkGraphicsPipelineCreateInfo pipelineCI = Init::pipelineCreateInfo(m_pipelineLayout, m_renderPass);
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
	// TODO
	shaderStages[0] = LoadShader(GetShadersPath() + "Triangle.Vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(GetShadersPath() + "Triangle.Frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	pipelineCI.pVertexInputState = &vertexInputState;
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();


	// Enable depth test and write
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthTestEnable = VK_TRUE;

	if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("fail to create graphics pipelines");
	};

}
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
void Renderer::CreateUniformBuffer()
{
	CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_uboBuffer, sizeof(m_uboMatrices));

	m_uboBuffer.Map();

}
void Renderer::CreateDescriptors()
{
	// pool
	std::vector<VkDescriptorPoolSize> poolSizes{};
	poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
	VkDescriptorPoolCreateInfo descriptorPoolCI{};
	descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descriptorPoolCI.maxSets = 1;
	descriptorPoolCI.pPoolSizes = poolSizes.data();

	if (vkCreateDescriptorPool(m_device, &descriptorPoolCI, nullptr, &m_descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor pool!");
	};

	// create layout
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};

	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	//VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	//samplerLayoutBinding.binding = 1;
	//samplerLayoutBinding.descriptorCount = 1;
	//samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	//samplerLayoutBinding.pImmutableSamplers = nullptr;
	//samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	setLayoutBindings.push_back(std::move(uboLayoutBinding));
	//setLayoutBindings.push_back(std::move(samplerLayoutBinding));

	VkDescriptorSetLayoutCreateInfo layoutCI{};
	layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	layoutCI.pBindings = setLayoutBindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &layoutCI, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	// allocate set
	VkDescriptorSetAllocateInfo allocIF{};
	allocIF.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocIF.descriptorPool = m_descriptorPool;
	allocIF.descriptorSetCount = 1;
	allocIF.pSetLayouts = &m_descriptorSetLayout;

	if (vkAllocateDescriptorSets(m_device, &allocIF, &m_descriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set!");
	}

	// write 
	VkWriteDescriptorSet writeUBO{};
	writeUBO.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeUBO.dstSet = m_descriptorSet;
	writeUBO.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeUBO.dstBinding = 0;
	writeUBO.pBufferInfo = &m_uboBuffer.descriptor;
	writeUBO.descriptorCount = 1;

	//VkWriteDescriptorSet writeImageSampler{};
	//writeUBO.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	//writeUBO.dstSet = m_descriptorSet;
	//writeUBO.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	//writeUBO.dstBinding = 1;
	//writeUBO.pBufferInfo = bufferInfo;
	//writeUBO.descriptorCount = 1;

	std::vector<VkWriteDescriptorSet> writeDescriptorSets{ writeUBO };
	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
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

void Renderer::CreateSwapChain()
{
	SwapChainSupportDetails& swapChainSupport = m_swapChainSupport;
	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

	// 设置交换链图像数量
	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices& indices = m_indices;
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
	// 如果是两个队列族，交换链就需要在两个队列族之间共享图像
	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}
	// 预变换，这里是不变换
	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain.swapChain) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create swap chain!");
	}

	//查询交换链包含的图像数量，但不会返回图像句柄。这一步的目的是确定需要分配多少内存来存储这些图像。
	vkGetSwapchainImagesKHR(m_device, m_swapChain.swapChain, &imageCount, nullptr);
	m_swapChain.images.resize(imageCount);

	//第二次调用 vkGetSwapchainImagesKHR：
	//传递 mSwapChainImages.data()，将交换链的图像句柄存储到向量中。
	// mSwapChainImages 现在包含交换链的所有图像句柄，后续可通过这些句柄操作每帧的渲染目标。
	// Vulkan 管理这些图像的内存。开发者无需手动分配或释放，只需通过 VkSwapchainImagesKHR 获取句柄即可。
	vkGetSwapchainImagesKHR(m_device, m_swapChain.swapChain, &imageCount, m_swapChain.images.data());
	m_swapChain.colorFormat = surfaceFormat.format;
	//m_swapChain.swapChainExtent = extent;

	CreateSwapChainImageViews();
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

void Renderer::MainLoop()
{
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
		DrawFrame();
		currentBuffer = (currentBuffer + 1) % m_swapChain.images.size();
	}
	vkDeviceWaitIdle(m_device);
}

void Renderer::PrepareFrame()
{

	VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain.swapChain, UINT64_MAX, m_semaphores.presentComplete, (VkFence)nullptr, &currentBuffer);

	if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
	{
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			//windowResize();
			throw std::runtime_error("window Resized");
		}
		return;
	}
	else
	{
		VK_CHECK_RESULT(result);
	}
}
void Renderer::UpdateUniformBuffers()
{
	m_uboMatrices.proj = m_camera.matrices.perspective;
	m_uboMatrices.view = m_camera.matrices.view;
	//camera.matrices.view;
	//m_uboMatrices.camPos = m_camera.position * -1.0f;

	memcpy(m_uboBuffer.mapped, &m_uboMatrices, sizeof(m_uboMatrices));
}
void Renderer::DrawFrame()
{
	UpdateUniformBuffers();
	PrepareFrame();

	m_submitInfo.commandBufferCount = 1;
	m_submitInfo.pCommandBuffers = &m_drawCmdBuffers[currentBuffer]; //TODO
	VK_CHECK_RESULT(vkQueueSubmit(m_queues.graphicsQueue, 1, &m_submitInfo, VK_NULL_HANDLE));


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
		//windowResize();
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

void Renderer::Cleanup()
{
	// cmd buffer
	vkDestroyCommandPool(m_device, m_commandPool, nullptr);

	// indices vertices
	vkDestroyBuffer(m_device, vertices.buffer, nullptr);
	vkFreeMemory(m_device, vertices.memory, nullptr);
	vkDestroyBuffer(m_device, indices.buffer, nullptr);
	vkFreeMemory(m_device, indices.memory, nullptr);

	// depth 
	vkDestroyImageView(m_device, m_depthStencil.view, nullptr);
	vkDestroyImage(m_device, m_depthStencil.image, nullptr);
	vkFreeMemory(m_device, m_depthStencil.memory, nullptr);

	// semaphore
	vkDestroySemaphore(m_device, m_semaphores.presentComplete, nullptr);
	vkDestroySemaphore(m_device, m_semaphores.renderComplete, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	vkDestroyPipeline(m_device, m_pipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_device, m_renderPass, nullptr);

	for (auto& frameBuffer : m_frameBuffers)
		vkDestroyFramebuffer(m_device, frameBuffer, nullptr);

	for (auto& shaderModule : m_shaderModules)
	{
		vkDestroyShaderModule(m_device, shaderModule, nullptr);
	}
	// uniformbuffer
	m_uboBuffer.Destroy();

	//for (auto& view : m_swapChain.imageViews)
	//{
	//	vkDestroyImageView(m_device, view, nullptr);
	//}
	//for (auto& image : m_swapChain.views)
	//{
	//	vkDestroyImageView(m_device, image, nullptr);
	//}
	m_swapChain.Cleanup();
	delete m_vulkanDevice;
	//vkDestroySwapchainKHR(m_device, m_swapChain.swapChain, nullptr);

	//vkDestroyDevice(m_device, nullptr);
	if (m_neededFeatures.validation)
	{
		DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
	}
	//vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	//vkDestroyInstance(m_instance, nullptr);

	glfwDestroyWindow(m_window);
	glfwTerminate();
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
	LoadglTFFile(GetAssetsPath() + "Models/FlightHelmet/glTF/FlightHelmet.gltf");
}

void Renderer::LoadglTFFile(std::string fileName)
{
	tinygltf::Model glTFInput;
	tinygltf::TinyGLTF gltfContext;
	std::string error, warning;
	bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, fileName);

	// Pass some Vulkan resources required for setup and rendering to the glTF model loading class
	m_glTFModel.vulkanDevice = m_vulkanDevice;
	m_glTFModel.copyQueue = m_queues.graphicsQueue;

	std::vector<uint32_t> indexBuffer;
	std::vector<GLTFModel::Vertex> vertexBuffer;

	if (fileLoaded)
	{
		m_glTFModel.LoadImages(glTFInput);
		m_glTFModel.LoadMaterials(glTFInput);
		m_glTFModel.LoadTextures(glTFInput);
		const tinygltf::Scene& scene = glTFInput.scenes[0];
		for (size_t i = 0; i < scene.nodes.size(); i++)
		{
			const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
			m_glTFModel.LoadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
		}
	}
	else
	{
		throw std::runtime_error("Could not open the glTF file.\n\nMake sure the assets submodule has been checked out and is up-to-date.");
		return;
	}

	// Create and upload vertex and index buffer
	// We will be using one single vertex buffer and one single index buffer for the whole glTF scene
	// Primitives (of the glTF model) will then index into these using index offsets

	size_t vertexBufferSize = vertexBuffer.size() * sizeof(GLTFModel::Vertex);
	size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
	m_glTFModel.indices.count = static_cast<uint32_t>(indexBuffer.size());

	struct StagingBuffer
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertexStaging, indexStaging;

	// Create host visible staging buffers (source)
	VK_CHECK_RESULT(m_vulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		vertexBufferSize,
		&vertexStaging.buffer,
		&vertexStaging.memory,
		vertexBuffer.data()));
	// Index data
	VK_CHECK_RESULT(m_vulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		indexBufferSize,
		&indexStaging.buffer,
		&indexStaging.memory,
		indexBuffer.data()));

	// Create device local buffers (target)
	VK_CHECK_RESULT(m_vulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBufferSize,
		&m_glTFModel.vertices.buffer,
		&m_glTFModel.vertices.memory));
	VK_CHECK_RESULT(m_vulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBufferSize,
		&m_glTFModel.indices.buffer,
		&m_glTFModel.indices.memory));

	// Copy data from staging buffers (host) do device local buffer (gpu)
	VkCommandBuffer copyCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkBufferCopy copyRegion = {};

	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(
		copyCmd,
		vertexStaging.buffer,
		m_glTFModel.vertices.buffer,
		1,
		&copyRegion);

	copyRegion.size = indexBufferSize;
	vkCmdCopyBuffer(
		copyCmd,
		indexStaging.buffer,
		m_glTFModel.indices.buffer,
		1,
		&copyRegion);

	m_vulkanDevice->FlushCommandBuffer(copyCmd, m_queues.graphicsQueue, true);

	// Free staging resources
	vkDestroyBuffer(m_device, vertexStaging.buffer, nullptr);
	vkFreeMemory(m_device, vertexStaging.memory, nullptr);
	vkDestroyBuffer(m_device, indexStaging.buffer, nullptr);
	vkFreeMemory(m_device, indexStaging.memory, nullptr);

}