module;
#include <vulkan/vulkan.h>
export module Engine.Rhi.Vulkan.Descriptor;
import Engine.Rhi.Descriptor;
import Engine.Rhi.Definition;
import Engine.Rhi.Vulkan.Device;
import std;

export namespace Engine::Rhi
{
    class VulkanDescriptorSetLayout final : public DescriptorSetLayout
    {
    public:
        VulkanDescriptorSetLayout(VulkanDevice* device, const DescriptorSetLayoutDesc& desc);
        ~VulkanDescriptorSetLayout() override;

        void* GetNativeHandle() const override
        {
            return (void*)layout_;
        }
        VkDescriptorSetLayout GetHandle() const
        {
            return layout_;
        }

    private:
        VulkanDevice* device_{};
        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    };

    class VulkanDescriptorSet final : public RhiDescriptorSet
    {
    public:
        // 描述符集由 Device 分配好句柄后传进来，而不是在构造函数里自己去 Pool 分配
        VulkanDescriptorSet(VulkanDevice* device, VkDescriptorSet handle, VkDescriptorPool pool, bool isTransient = false);
        ~VulkanDescriptorSet() override;

        void* GetNativeHandle() const override
        {
            return (void*)set_;
        }
        VkDescriptorSet GetHandle() const
        {
            return set_;
        }

        void UpdateBuffer(uint32_t binding, const RhiBuffer* buffer, DescriptorType type,uint64_t offset, uint64_t range) override;
        void UpdateTexture(uint32_t binding, const RhiTexture* texture, const Sampler* sampler) override;

    private:
        VulkanDevice* device_{};
        VkDescriptorPool pool_{};
        VkDescriptorSet set_{};
        bool isTransient_ = false;
    };
} // namespace Engine::Rhi