module;

export module Engine.Window;

import std;

export namespace Engine
{
    class Window
    {
    public:
        Window(int width, int height, const char *title);
        ~Window();

        // 禁止拷贝
        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

        bool ShouldClose() const;
        void PollEvents();

        int GetWidth() const { return width_; }
        int GetHeight() const { return height_; }
        float GetAspectRatio() const { return static_cast<float>(width_) / height_; }

        // 获取底层句柄 (返回 void* 以隐藏 GLFW 类型)
        void *GetNativeHandle() const { return window_; }

    private:
        // 真正使用 void* 存储，彻底解耦 GLFW 头文件
        void *window_{nullptr};

        int width_{};
        int height_{};
        std::string title_{};
    };
};