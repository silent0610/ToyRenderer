module;
export module Engine.Rhi.Sync;
import std;
export namespace Engine::Rhi
{
    // 信号量：用于 GPU 内部队列同步 (CPU 不可见)
    class RhiSemaphore
    {
    public:
        virtual ~RhiSemaphore() = default;
        virtual void *GetNativeHandle() const = 0;
    };

    // 围栏：用于 CPU 等待 GPU
    class RhiFence
    {
    public:
        virtual ~RhiFence() = default;

        // CPU 阻塞等待 Fence 变绿
        virtual void Wait() = 0;

        // 重置 Fence 为红灯
        virtual void Reset() = 0;

        virtual void *GetNativeHandle() const = 0;
    };
}