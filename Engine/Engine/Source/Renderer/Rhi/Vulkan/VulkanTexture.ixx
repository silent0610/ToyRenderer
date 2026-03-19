module;
#include "vulkan/vulkan.h"
export module Engine.Rhi.Vulkan.Texture;
import Engine.Rhi.Texture;
import Engine.Rhi.Definition;
import Engine.Rhi.Vulkan.Device;
export namespace Engine::Rhi
{
    class VulkanTexture final : public RhiTexture
    {
    public:
        VulkanTexture(VulkanDevice* device, const TextureDesc& desc);
        ~VulkanTexture() override;

        VulkanTexture(VulkanDevice* device, VkImage existingImage, VkImageView existingView, const TextureDesc& desc);

        const Engine::Rhi::TextureDesc& GetDesc() const override;
        void* GetNativeHandle() const override;

        // Vulkan 特有访问器
        VkImage GetImage() const;
        VkImageView GetImageView() const;

    private:
        void CreateImage(VulkanDevice* device, const TextureDesc& desc);
        void AllocateMemory(VkImage image);
        void CreateView(VkImage image, const TextureDesc& desc) ;

    private:
        VulkanDevice* device_{};
        TextureDesc desc_{};

        VkImage image_{};
        VkDeviceMemory memory_{};
        VkImageView imageView_{};

        bool ownMemory_{true}; // 是否拥有资源的所有权
    };  
} // namespace Engine::Rhi