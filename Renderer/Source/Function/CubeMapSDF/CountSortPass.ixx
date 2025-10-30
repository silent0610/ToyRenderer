module;
#include "vulkan/vulkan.h"
export module CountSortPass;

class CountSortPass : public IRenderPassBase
{
public:
    CountSortPass(OldVulkanDevice *device) : IRenderPass(device) { ; }
    virtual ~ShadowPass() = default;
    void Setup() override;
    void Initialize() override;
    void Execute(VkCommandBuffer cmd) override;
}