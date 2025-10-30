module;
#include "vulkan/vulkan.h"

export module IRenderPass;
import DeviceMod;

class IRenderPassBase
{
public:
    IRenderPass(OldVulkanDevice *device) : device_(device) {}
    virtual ~IRenderPass() = default;
    // 禁用拷贝和移动
    IRenderPass(const IRenderPass &) = delete;
    IRenderPass &operator=(const IRenderPass &) = delete;

    virtual void Setup() = 0;

    virtual void Initialize() = 0;

    virtual void Execute(VkCommandBuffer cmd) = 0;

private:
    OldVulkanDevice *device_{};
}