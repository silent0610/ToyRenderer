module;
#include "spdlog/spdlog.h"
#include "vulkan/vulkan.h"
#include <fstream>
module Engine.Rhi.Vulkan.Shader;
import Engine.Rhi.Vulkan.Device;
import Engine.Rhi.Vulkan.Tool;
import std;
namespace Engine::Rhi
{
    VulkanShader::VulkanShader(VulkanDevice *device, const std::string_view filePath, ShaderStage stage) : device_{device}
    {

        stageFlag_ = static_cast<VkShaderStageFlagBits>(Tool::ConvertShaderStage(stage));
        std::vector<char> code = ReadFile(filePath);
        if (code.empty())
        {
            throw std::runtime_error("Failed to read shader file");
        }

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        Tool::CheckResult(vkCreateShaderModule(device_->GetDevice(), &createInfo, nullptr, &shaderModule_));
        spdlog::info("Shader Loaded: {}", filePath);
    }
    VulkanShader::~VulkanShader()
    {
        if (shaderModule_ != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device_->GetDevice(), shaderModule_, nullptr);
            spdlog::info("Shader Module Destroyed");
        };
    }
    const char *VulkanShader::GetEntryPoint() const
    {
        return "main";
    }
    VkShaderModule VulkanShader::GetShaderModule() const
    {
        return shaderModule_;
    }
    VkShaderStageFlagBits VulkanShader::GetShaderStageFlagBits() const
    {
        return stageFlag_;
    }
    std::vector<char> VulkanShader::ReadFile(const std::string_view filePath)
    {
        std::filesystem::path path{filePath};

        if (!std::filesystem::exists(path))
        {
            return {};
        }
        size_t fileSize = std::filesystem::file_size(path);
        std::vector<char> buffer(fileSize);
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            return {};
        }
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    };
} // namespace Engine::Rhi