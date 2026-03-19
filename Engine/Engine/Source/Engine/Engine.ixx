module;
#include "GLFW/glfw3.h"
#include "Math/Glm.hpp"
export module Engine;

import Engine.Rhi.Device;
import Engine.Rhi.Definition;
import Engine.Rhi.CommandList;
import Engine.Rhi.Swapchain;
import Engine.Window;
import Engine.Rhi.Sync;
import Engine.Scene;
import Engine.Renderer;
import Engine.Input;
import std;

export namespace Engine
{

    struct FrameResource
    {
        // 每帧都有自己的指令列表，互不干扰
        std::shared_ptr<Rhi::CommandList> CmdList;

        // 同步原语
        std::unique_ptr<Rhi::RhiSemaphore> ImageAvailable;
        std::unique_ptr<Rhi::RhiSemaphore> RenderFinished;
        std::unique_ptr<Rhi::RhiFence> InFlightFence;
    };
    class MyEngine
    {
    public:
        MyEngine() = default;
        ~MyEngine() = default;
        void Tick();
        void Init();

        void InitRhi();

        void Shutdown();

        static int GetInfligtNum()
        {
            return kMaxFramesInFlight_;
        };

        // Getters
        Scene* GetScene() const { return scene_.get(); }
        Window* GetWindow() const { return window_.get(); }
        Renderer* GetRenderer() const { return renderer_.get(); }
        Rhi::RhiDevice* GetDevice() const { return device_.get(); }

    private:
        // 1. 窗口必须最先存在
        // GLFWwindow *window_;
        std::unique_ptr<Window> window_;
        // 2. RHI 设备
        std::shared_ptr<Rhi::RhiDevice> device_;

        // 3. 交换链 (依赖 Window 和 Device)
        std::unique_ptr<Rhi::RhiSwapchain> swapchain_;

        std::unique_ptr<Scene> scene_;
        std::unique_ptr<Renderer> renderer_;

        static const int kMaxFramesInFlight_ = 3;
        std::vector<FrameResource> frames_{kMaxFramesInFlight_};
        uint32_t currentFrameIndex_ = 0;
        bool isRunning_ = false;
    };
}
