module;
#include "vulkan/vulkan.h"
#include "spdlog/spdlog.h"
export module Engine.Rhi.Vulkan.Pipeline;
import Engine.Rhi.Pipeline;
import Engine.Rhi.Definition;
import Engine.Rhi.Vulkan.Device;
import Engine.Rhi.Vulkan.Shader;
import Engine.Rhi.Vulkan.Tool;
export namespace Engine::Rhi
{
    class VulkanPipeline final : public Pipeline
    {
    public:
        VulkanPipeline(VulkanDevice* device, const PipelineStateDesc& desc);
        VulkanPipeline(VulkanDevice* device, const ComputePipelineDesc& desc);
        ~VulkanPipeline() override;
        Type GetType() const override;

        VkPipeline GetPipeline() const;
        VkPipelineLayout GetPipelineLayout() const;
        VkPipelineBindPoint GetBindPoint() const;

        private:


        VulkanDevice* device_{};
        VkPipeline pipeline_{};
        VkPipelineLayout pipelineLayout_{};
        Type pipelineType_{};
        VkPipelineBindPoint bindPoint_{};
    };
}