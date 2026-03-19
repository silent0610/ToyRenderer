module;
#include <GLFW/glfw3.h>
export module Engine.Input;

export namespace Engine
{
    enum class KeyCode : int
    {
        Space = 32,
        A = 65,
        D = 68,
        E = 69,
        S = 83,
        W = 87,
        Q = 81,
        Esc = 256,
        Right = 262,
        Left = 263,
        Down = 264,
        Up = 265,
        LeftShift = 340
    };
    enum class MouseButton : int
    {
        Left = 0,
        Right = 1,
        Middle = 2
    };

    class Input
    {
    public:
        // 初始化，传入原生窗口句柄
        static void Initialize(void *windowHandle);

        // 每帧调用，用于重置一帧内的状态 (如鼠标 Delta)
        static void Update();

        // --- 状态查询 ---
        static bool IsKeyDown(KeyCode key);
        static bool IsMouseButtonDown(MouseButton button);

        // --- 鼠标控制 ---
        static void GetMousePosition(double &x, double &y);
        static void GetMouseDelta(double &dx, double &dy);

        // 锁定/隐藏鼠标 (FPS 模式)
        static void SetCursorMode(bool locked);

    private:
        // 内部持有的窗口句柄
        static GLFWwindow *windowHandle_;

        // 鼠标状态缓存
        static double lastMouseX_;
        static double lastMouseY_;
        static double deltaX_;
        static double deltaY_;
        static bool firstMouse_;
    };

} // namespace Engine
