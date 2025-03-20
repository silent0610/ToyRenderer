#include "Renderer.h"
#include <iostream>
void Renderer::Run()
{
	InitWindow();
	//InitVulkan();
	//MainLoop();
	//Cleanup();
}

/// @brief 初始化窗口
void Renderer::InitWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	// 使用glfw创建窗口
	m_window = glfwCreateWindow(m_width, m_height, "Renderer", nullptr, nullptr);
	// 传递对象this指针给回调函数。这样我们就可以在回调函数中访问Hello类的成员变量
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
}
///// @brief 初始化 Vulkan，包括相关设置
//void Renderer::InitVulkan()
//{
//	CreateInstance();
//	SetupDebugMessenger(); //先创建实例，相当于先指定和调试的接口，再之后再实际链接
//	CreateSurface();
//	PickPhysicalDevice();
//	CreateLogicalDevice();
//}
//void Renderer::MainLoop()
//{
//	while (!glfwWindowShouldClose(m_window))
//	{
//		glfwPollEvents();
//		DrawFrame();
//	}
//	vkDeviceWaitIdle(m_device);
//}
//void Renderer::Cleanup()
//{
//	vkDestroyDevice(m_device, nullptr);
//	if (m_enableValidation)
//	{
//		DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
//	}
//	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
//	vkDestroyInstance(m_instance, nullptr);
//
//	glfwDestroyWindow(m_window);
//	glfwTerminate();
//}

// 得到窗口大小改变的信息
void Renderer::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	// 得到从window传递的this指针
	auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	app->m_framebufferResized = true;
}