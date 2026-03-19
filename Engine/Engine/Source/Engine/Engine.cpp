module;
#define GLFW_EXPOSE_NATIVE_WIN32
#include "Math/Glm.hpp"
#include "spdlog/spdlog.h"
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
module Engine;
import Engine.Rhi.Pipeline;
import Engine.Rhi.Buffer;
import Engine.Rhi.Vulkan.Buffer;
namespace Engine
{
    void MyEngine::InitRhi()
    {
        spdlog::info("Init RHI");
        device_ = ::Engine::Rhi::CreateDevice(::Engine::Rhi::GraphicsBackend::Vulkan);
        Engine::Rhi::DeviceDesc desc;
        desc.DebugLayer = true;
        desc.RayTracing = false;
        if (!device_->Init(desc))
        {
            spdlog::error("Failed to initialize RHI device");
            assert(false);
            return;
        }
        swapchain_ =
            device_->CreateSwapchain(window_->GetNativeHandle(), window_->GetWidth(), window_->GetHeight(), Engine::Rhi::PixelFormat::R8G8B8A8UNORM);

        // 准备帧资源
        if (kMaxFramesInFlight_ < swapchain_->GetImageCount())
        {
            spdlog::warn("kMaxFramesInFlight_ is less than swapchain image count!");
            frames_.resize(swapchain_->GetImageCount());
        }
        for (auto& frame : frames_)
        {
            frame.CmdList = device_->CreateCommandList(); // 创建两个 CmdList
            frame.ImageAvailable = device_->CreateSyncSemaphore();
            frame.RenderFinished = device_->CreateSyncSemaphore();
            frame.InFlightFence = device_->CreateSyncFence(true); // 初始绿灯
        }
    }
    void MyEngine::Tick()
    {
        if (window_->GetWidth() == 0 || window_->GetHeight() == 0)
        {
            return;
        }

        // =========================================================
        // 步骤 1: [CPU 等待] 等待当前帧槽位 (Frame Slot) 的 GPU 任务完成
        // =========================================================
        auto& frame = frames_[currentFrameIndex_];
        frame.InFlightFence->Wait();

        // =========================================================
        // 步骤 2: [获取图片] 询问 Swapchain 要一张图
        // =========================================================
        // 注意：传入当前帧的 ImageAvailable 信号量
        if (!swapchain_->AcquireNextImage(frame.ImageAvailable.get()))
        {
            return; // 窗口大小改变
        }

        // 只有 Acquire 成功后，才重置 Fence (变红灯)，准备录制新任务
        frame.InFlightFence->Reset();

        device_->BeginFrame(currentFrameIndex_);
        // =========================================================
        // 步骤 3: [录制指令] 使用当前帧的 CmdList
        // =========================================================
        auto cmd = frame.CmdList.get();
        cmd->Begin();
        auto backBuffer = swapchain_->GetCurrentBackBuffer().get();
        renderer_->RenderFrame(cmd, *scene_, backBuffer);
        cmd->End();
        // =========================================================
        // 步骤 4: [提交执行] 告诉 GPU 干活
        // =========================================================
        Rhi::QueueSubmitInfo submitInfo{};
        submitInfo.CmdList = cmd;

        // 依赖链：
        // Wait: 等待 Acquire 成功 (ImageAvailable) -> 才能开始往 BackBuffer 拷贝
        submitInfo.WaitSemaphore = frame.ImageAvailable.get();
        // 如果你的 Blit 在 Transfer 阶段执行，这里最好指定 WaitStage = Transfer
        submitInfo.WaitStageMask = Rhi::WaitStage::AllCommands;

        // Signal: 完成后通知 Present (RenderFinished)
        submitInfo.SignalSemaphore = frame.RenderFinished.get();

        // Fence: 完成后通知 CPU (InFlightFence)
        submitInfo.SignalFence = frame.InFlightFence.get();

        device_->Execute(submitInfo);

        // =========================================================
        // 步骤 5: [呈现]
        // =========================================================
        // 等待 RenderFinished 信号
        swapchain_->Present(frame.RenderFinished.get(), true);

        // =========================================================
        // 步骤 6: [轮转] 下一帧
        // =========================================================
        currentFrameIndex_ = (currentFrameIndex_ + 1) % kMaxFramesInFlight_;
    }

    void MyEngine::Shutdown()
    {
        if (device_)
        {
            device_->WaitIdle();
        }
    }

    void MyEngine::Init()
    {
        spdlog::info("Init Engine");
        window_ = std::make_unique<Window>(800, 600, "Engine");

        // 初始化 Input
        Input::Initialize(window_->GetNativeHandle());

        InitRhi();
        renderer_ = std::make_unique<Renderer>(device_.get());
        scene_ = std::make_unique<Scene>();
    }

} // namespace Engine