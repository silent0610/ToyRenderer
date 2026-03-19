module;
#include <vulkan/vulkan.h>
export module Engine.Rhi.Vulkan.Sampler;
import Engine.Rhi.Sampler;
import Engine.Rhi.Definition;
import Engine.Rhi.Vulkan.Device;
export namespace Engine::Rhi
{

    class VulkanSampler final : public Sampler
    {
    public:
        VulkanSampler(VulkanDevice *device, const SamplerDesc &desc);
        ~VulkanSampler() override;

        void *GetNativeHandle() const override { return (void *)sampler_; }
        VkSampler GetHandle() const { return sampler_; }

    private:
        VulkanDevice *device_{};
        VkSampler sampler_{};
    };
}
