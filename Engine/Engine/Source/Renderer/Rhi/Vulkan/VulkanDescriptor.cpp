module;
#include <vulkan/vulkan.h>
module Engine.Rhi.Vulkan.Descriptor;
import Engine.Rhi.Vulkan.Tool;
import Engine.Rhi.Vulkan.Buffer;
import Engine.Rhi.Buffer;
import Engine.Rhi.Vulkan.Sampler;
import Engine.Rhi.Sampler;
import Engine.Rhi.Vulkan.Texture;
import Engine.Rhi.Texture;
namespace Engine::Rhi
{
    static VkDescriptorType ConvertDescriptorType(DescriptorType type)
    {
        switch (type)
        {
        case DescriptorType::UniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::CombinedImageSampler:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::StorageImage:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        default:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
    }

    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDevice* device, const DescriptorSetLayoutDesc& desc) : device_(device)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(desc.Bindings.size());

        for (const auto& b : desc.Bindings)
        {
            VkDescriptorSetLayoutBinding vkBinding{};
            vkBinding.binding = b.Binding;
            vkBinding.descriptorType = ConvertDescriptorType(b.Type);
            vkBinding.descriptorCount = b.Count;
            // 使用之前的工具函数转换 ShaderStage
            vkBinding.stageFlags = Tool::ConvertShaderStage(b.Stage);
            vkBinding.pImmutableSamplers = nullptr;

            bindings.push_back(vkBinding);
        }

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = static_cast<uint32_t>(bindings.size());
        info.pBindings = bindings.data();

        Tool::CheckResult(vkCreateDescriptorSetLayout(device_->GetDevice(), &info, nullptr, &layout_));
    }
    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
    {
        if (layout_)
        {
            vkDestroyDescriptorSetLayout(device_->GetDevice(), layout_, nullptr);
        }
    }

    VulkanDescriptorSet::VulkanDescriptorSet(VulkanDevice* device, VkDescriptorSet handle, VkDescriptorPool pool, bool isTransient)
        : device_(device), set_(handle), isTransient_(isTransient), pool_(pool)
    {
    }
    VulkanDescriptorSet::~VulkanDescriptorSet()
    {
        if (set_ != VK_NULL_HANDLE)
        {
            // 如果我是持久的，我必须手动清理显存
            if (!isTransient_)
            {
                vkFreeDescriptorSets(device_->GetDevice(), pool_, 1, &set_);
            }
        }
    }
    void VulkanDescriptorSet::UpdateBuffer(uint32_t binding, const RhiBuffer* buffer, DescriptorType type, uint64_t offset, uint64_t range)
    {
        auto vkBuf = static_cast<const VulkanBuffer*>(buffer);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = vkBuf->GetBuffer();
        bufferInfo.offset = offset;
        bufferInfo.range = (range == 0) ? VK_WHOLE_SIZE : range;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set_;
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = ConvertDescriptorType(type); 
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device_->GetDevice(), 1, &write, 0, nullptr);
    }

    void VulkanDescriptorSet::UpdateTexture(uint32_t binding, const RhiTexture* texture, const Sampler* sampler)
    {
        auto vkTex = static_cast<const VulkanTexture*>(texture);
        auto vkSamp = static_cast<const VulkanSampler*>(sampler);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 采样时的布局
        imageInfo.imageView = vkTex->GetImageView();
        imageInfo.sampler = vkSamp->GetHandle();

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set_;
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device_->GetDevice(), 1, &write, 0, nullptr);
    }
}