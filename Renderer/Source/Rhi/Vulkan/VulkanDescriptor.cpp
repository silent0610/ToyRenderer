module;
#include <vulkan/vulkan.h>

module VulkanDescriptor;
    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VkDescriptorSetLayout layout, VkDevice device)
        : layout_(layout), device_(device)
    {
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
    {
        if (layout_ != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(device_, layout_, nullptr);
        }
    }

    VulkanDescriptorPool::VulkanDescriptorPool(VkDescriptorPool pool, VkDevice device)
        : pool_(pool), device_(device)
    {
    }

    VulkanDescriptorPool::~VulkanDescriptorPool()
    {
        if (pool_ != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(device_, pool_, nullptr);
        }
    }

VulkanDescriptorSet::VulkanDescriptorSet(VkDescriptorSet set)
    : set_(set)
{
    // Descriptor sets are implicitly freed when their pool is destroyed
    // No explicit cleanup needed
}