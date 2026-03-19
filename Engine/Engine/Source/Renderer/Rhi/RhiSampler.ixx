module;
export module Engine.Rhi.Sampler;
import Engine.Rhi.Definition;

export namespace Engine::Rhi
{
    class Sampler
    {
    public:
        virtual ~Sampler() = default;

        // 获取底层句柄 (VkSampler)
        virtual void *GetNativeHandle() const = 0;
    };
}