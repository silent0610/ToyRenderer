module;
#include "vulkan/vulkan.h"

export module IRenderPass;
import DeviceMod;

export class IRenderPassBase
{
public:
    IRenderPassBase(OldVulkanDevice *device) : device_(device) {}
    virtual ~IRenderPassBase() = default;
    // 禁用拷贝和移动
    IRenderPassBase(const IRenderPassBase &) = delete;
    IRenderPassBase &operator=(const IRenderPassBase &) = delete;

    virtual void Setup() = 0;

    virtual void Initialize() = 0;

    virtual void Execute(VkCommandBuffer cmd) = 0;

private:
    OldVulkanDevice *device_{};
};