module;
#include <vulkan/vulkan.h>

export module VulkanDescriptor;
import RhiDescriptor;

export {
    class VulkanDescriptorSetLayout : public RhiDescriptorSetLayout
    {
    public:
        VulkanDescriptorSetLayout(VkDescriptorSetLayout layout, VkDevice device);
        ~VulkanDescriptorSetLayout() override;

        void* GetNativeHandle() const override { return reinterpret_cast<void*>(layout_); }
        VkDescriptorSetLayout GetVkDescriptorSetLayout() const { return layout_; }

    private:
        VkDescriptorSetLayout layout_;
        VkDevice device_;
    };

    class VulkanDescriptorPool : public RhiDescriptorPool
    {
    public:
        VulkanDescriptorPool(VkDescriptorPool pool, VkDevice device);
        ~VulkanDescriptorPool() override;

        void* GetNativeHandle() const override { return reinterpret_cast<void*>(pool_); }
        VkDescriptorPool GetVkDescriptorPool() const { return pool_; }

    private:
        VkDescriptorPool pool_;
        VkDevice device_;
    };

    class VulkanDescriptorSet : public RhiDescriptorSet
    {
    public:
        VulkanDescriptorSet(VkDescriptorSet set);

        void* GetNativeHandle() const override { return reinterpret_cast<void*>(set_); }
        VkDescriptorSet GetVkDescriptorSet() const { return set_; }

    private:
        VkDescriptorSet set_; // Managed by pool, no explicit cleanup needed
    };
}