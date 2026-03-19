module;
#include <GLFW/glfw3.h>
module Engine.Input;

// 这里引入 GLFW，不要污染 ixx

namespace Engine
{
    // 静态变量定义
    GLFWwindow *Input::windowHandle_ = nullptr;
    double Input::lastMouseX_ = 0.0;
    double Input::lastMouseY_ = 0.0;
    double Input::deltaX_ = 0.0;
    double Input::deltaY_ = 0.0;
    bool Input::firstMouse_ = true;

    void Input::Initialize(void *windowHandle)
    {
        windowHandle_ = static_cast<GLFWwindow *>(windowHandle);

        // 初始化鼠标位置
        if (windowHandle_)
        {
            glfwGetCursorPos(windowHandle_, &lastMouseX_, &lastMouseY_);
        }
    }

    void Input::Update()
    {
        if (!windowHandle_)
            return;

        double currentX, currentY;
        glfwGetCursorPos(windowHandle_, &currentX, &currentY);

        if (firstMouse_)
        {
            lastMouseX_ = currentX;
            lastMouseY_ = currentY;
            firstMouse_ = false;
        }

        deltaX_ = currentX - lastMouseX_;
        deltaY_ = currentY - lastMouseY_;

        lastMouseX_ = currentX;
        lastMouseY_ = currentY;
    }

    bool Input::IsKeyDown(KeyCode key)
    {
        if (!windowHandle_)
            return false;
        int state = glfwGetKey(windowHandle_, static_cast<int>(key));
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    }

    bool Input::IsMouseButtonDown(MouseButton button)
    {
        if (!windowHandle_)
            return false;
        int state = glfwGetMouseButton(windowHandle_, static_cast<int>(button));
        return state == GLFW_PRESS;
    }

    void Input::GetMousePosition(double &x, double &y)
    {
        if (!windowHandle_)
            return;
        glfwGetCursorPos(windowHandle_, &x, &y);
    }

    void Input::GetMouseDelta(double &dx, double &dy)
    {
        dx = deltaX_;
        dy = deltaY_;
    }

    void Input::SetCursorMode(bool locked)
    {
        if (!windowHandle_)
            return;
        glfwSetInputMode(windowHandle_, GLFW_CURSOR,
                         locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
}