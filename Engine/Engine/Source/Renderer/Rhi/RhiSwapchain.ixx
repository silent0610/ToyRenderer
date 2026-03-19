module;
#include <cstdint>
export module Engine.Rhi.Swapchain;

import Engine.Rhi.Definition;
import Engine.Rhi.Texture;
import Engine.Rhi.Sync;
import std;
export namespace Engine::Rhi
{

    class RhiSwapchain
    {
    public:
        virtual ~RhiSwapchain() = default;

        virtual void Present(RhiSemaphore *waitSemaphore, bool vsync) = 0;

        virtual bool Resize(uint32_t width, uint32_t height) = 0;
        virtual bool AcquireNextImage(RhiSemaphore *signalSemaphore) = 0;
        virtual std::shared_ptr<RhiTexture> GetCurrentBackBuffer() = 0;
        virtual PixelFormat GetFormat() const = 0;
        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;

        virtual uint32_t GetImageCount() const = 0;
    };
}