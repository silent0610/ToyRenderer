module;
#include "vulkan/vulkan.h"
export module Engine.Rhi.Vulkan.Shader;
import Engine.Rhi.Shader;

import std;
import Engine.Rhi.Vulkan.Device;
import Engine.Rhi.Definition;
export namespace Engine::Rhi
{

    class VulkanShader final : public RhiShader
    {
    public:
        VulkanShader(VulkanDevice *device, const std::string_view filePath, ShaderStage stage);
        ~VulkanShader() override;

        const char *GetEntryPoint() const override;
        VkShaderModule GetShaderModule() const;
        VkShaderStageFlagBits GetShaderStageFlagBits() const;

    private:
        VulkanDevice *device_{};
        VkShaderModule shaderModule_{};
        VkShaderStageFlagBits stageFlag_{};

        std::vector<char> ReadFile(const std::string_view filePath);
    };
} // namespace Engine::Rhi
