module;
#include <vulkan/vulkan.h>
module Engine.Rhi.Vulkan.Sampler;
import Engine.Rhi.Vulkan.Tool;
namespace Engine::Rhi
{
    static VkFilter ConvertFilter(FilterMode mode)
    {
        switch (mode)
        {
        case FilterMode::Nearest:
            return VK_FILTER_NEAREST;
        case FilterMode::Linear:
            return VK_FILTER_LINEAR;
        default:
            return VK_FILTER_LINEAR;
        }
    }
    static VkSamplerAddressMode ConvertAddressMode(SamplerAddressMode mode)
    {
        switch (mode)
        {
        case SamplerAddressMode::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerAddressMode::MirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SamplerAddressMode::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerAddressMode::ClampToBorder:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }
    VulkanSampler::VulkanSampler(VulkanDevice *device, const SamplerDesc &desc)
        : device_(device)
    {
        VkSamplerCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

        // 1. 过滤 (Min/Mag)
        info.magFilter = ConvertFilter(desc.MagFilter);
        info.minFilter = ConvertFilter(desc.MinFilter);

        // 2. Mipmap 模式 (暂时硬编码为 Linear，未来可以在 Desc 里加)
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        // 3. 寻址模式 (UVW)
        info.addressModeU = ConvertAddressMode(desc.AddressU);
        info.addressModeV = ConvertAddressMode(desc.AddressV);
        info.addressModeW = ConvertAddressMode(desc.AddressW);

        // 4. 其他默认值
        info.mipLodBias = 0.0f;

        // TODO 各向异性过滤 (Anisotropy)
        // 注意：开启这个需要检查 Device Features (samplerAnisotropy)
        // MVP 阶段先关闭，最为稳妥
        info.anisotropyEnable = VK_FALSE;
        info.maxAnisotropy = 1.0f;

        // TODO 比较操作 (用于 Shadow Map PCF)
        // 目前是普通采样，所以关闭

        info.compareEnable = VK_FALSE;
        info.compareOp = VK_COMPARE_OP_ALWAYS;

        info.minLod = 0.0f;
        info.maxLod = VK_LOD_CLAMP_NONE;
        info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        info.unnormalizedCoordinates = VK_FALSE; // 使用 0~1 UV

        Tool::CheckResult(vkCreateSampler(device_->GetDevice(), &info, nullptr, &sampler_));
    }

    VulkanSampler::~VulkanSampler()
    {
        if (sampler_)
        {
            vkDestroySampler(device_->GetDevice(), sampler_, nullptr);
        }
    }
}
