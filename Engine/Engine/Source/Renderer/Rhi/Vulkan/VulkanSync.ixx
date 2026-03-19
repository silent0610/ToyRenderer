module;
#include <vulkan/vulkan.h>
export module Engine.Rhi.Vulkan.Sync;

import Engine.Rhi.Sync;

export namespace Engine::Rhi
{

    class VulkanSemaphore final : public RhiSemaphore
    {
    public:
        VulkanSemaphore(VkDevice device);
        ~VulkanSemaphore() override;

        VulkanSemaphore(const VulkanSemaphore &) = delete;
        VulkanSemaphore &operator=(const VulkanSemaphore &) = delete;

        void *GetNativeHandle() const override;
        VkSemaphore GetHandle() const;

    private:
        VkDevice device_{}; //
        VkSemaphore handle_{};
    };

    class VulkanFence final : public RhiFence
    {

    public:
        VulkanFence(VkDevice device, bool signaled = false);
        ~VulkanFence() override;

        VulkanFence(const VulkanFence &) = delete;
        VulkanFence &operator=(const VulkanFence &) = delete;

        void Wait() override;
        void Reset() override;

        void *GetNativeHandle() const override;
        VkFence GetHandle() const;

    private:
        VkDevice device_{};
        VkFence handle_{};
    };
}