module;
#include <vulkan/vulkan.h>
module Engine.Rhi.Vulkan.Sync;
import Engine.Rhi.Vulkan.Tool;
namespace Engine::Rhi
{
    VulkanSemaphore::VulkanSemaphore(VkDevice device)
        : device_(device) // 直接存句柄
    {
        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        Tool::CheckResult(vkCreateSemaphore(device_, &info, nullptr, &handle_));
    }

    VulkanSemaphore::~VulkanSemaphore()
    {
        if (handle_)
            vkDestroySemaphore(device_, handle_, nullptr);
    }

    void *VulkanSemaphore::GetNativeHandle() const { return (void *)handle_; }
    VkSemaphore VulkanSemaphore::GetHandle() const { return handle_; }

    VulkanFence::VulkanFence(VkDevice device, bool signaled)
        : device_(device)
    {
        VkFenceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        if (signaled)
            info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        Tool::CheckResult(vkCreateFence(device_, &info, nullptr, &handle_));
    }

    VulkanFence::~VulkanFence()
    {
        if (handle_)
            vkDestroyFence(device_, handle_, nullptr);
    }

    void VulkanFence::Wait()
    {
        Tool::CheckResult(vkWaitForFences(device_, 1, &handle_, VK_TRUE, UINT64_MAX));
    }

    void VulkanFence::Reset()
    {
        Tool::CheckResult(vkResetFences(device_, 1, &handle_));
    }

    void *VulkanFence::GetNativeHandle() const { return (void *)handle_; }
    VkFence VulkanFence::GetHandle() const { return handle_; }
}