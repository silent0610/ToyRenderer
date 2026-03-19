module;
#include <GLFW/glfw3.h>
#include "spdlog/spdlog.h"
#include <stdexcept> // 用于抛出异常

module Engine.Window;

// 不需要再次 import std，因为接口已经 import 过了 (视编译器实现而定，MSVC通常需要再次import或者接口export import)
import std;

namespace Engine
{
    static int sWindowCount{0};

    Window::Window(int width, int height, const char *title)
        : width_(width), height_(height), title_(title)
    {
        // 1. 初始化 GLFW 全局上下文
        if (sWindowCount == 0)
        {
            if (!glfwInit())
            {
                spdlog::error("Failed to initialize GLFW");
                throw std::runtime_error("Failed to initialize GLFW");
            }
        }
        sWindowCount++;

        // 2. 设置 Hints
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Vulkan 必须
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        // 3. 创建窗口
        // 注意：这里赋值给成员变量 window_，而不是创建新的局部变量！
        GLFWwindow *rawWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);

        if (!rawWindow)
        {
            spdlog::error("Failed to create GLFW window");
            glfwTerminate(); // 这一步失败了也要清理
            throw std::runtime_error("Failed to create GLFW window");
        }

        // 将 GLFWwindow* 存入 void* 成员变量
        window_ = rawWindow;

        // 4. 设置 User Pointer
        glfwSetWindowUserPointer(rawWindow, this);
        spdlog::info("Window Created: {}x{}", width, height);
    }

    Window::~Window()
    {
        spdlog::info("Destroy Window");

        // 1. 销毁窗口
        if (window_)
        {
            // 将 void* 强转回 GLFWwindow* 进行销毁
            glfwDestroyWindow(static_cast<GLFWwindow *>(window_));
        }

        sWindowCount--;

        // 2. 如果是最后一个窗口，终止 GLFW
        if (sWindowCount == 0)
        {
            glfwTerminate();
            spdlog::info("[Window] GLFW Terminated.");
        }
    }

    bool Window::ShouldClose() const
    {
        // 强转回 GLFWwindow*
        return glfwWindowShouldClose(static_cast<GLFWwindow *>(window_));
    }

    void Window::PollEvents()
    {
        glfwPollEvents();
    }
};