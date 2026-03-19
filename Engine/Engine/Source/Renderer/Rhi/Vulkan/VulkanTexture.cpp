module;
#include "vulkan/vulkan.h"
module Engine.Rhi.Vulkan.Texture;
import Engine.Rhi.Vulkan.Tool;
namespace Engine::Rhi
{
    const TextureDesc& VulkanTexture::GetDesc() const 
    {
        return desc_;
    }
    void* VulkanTexture::GetNativeHandle() const 
    {
        return (void*)image_;
    }

    // Vulkan 特有访问器
    VkImage VulkanTexture::GetImage() const
    {
        return image_;
    }
    VkImageView VulkanTexture::GetImageView() const
    {
        return imageView_;
    }
    VulkanTexture::VulkanTexture(VulkanDevice* device, const TextureDesc& desc) : device_(device), desc_(desc), ownMemory_(true)
    {
        CreateImage(device_,desc_);
        AllocateMemory(image_);
        CreateView(image_,desc_);
    }
    VulkanTexture::VulkanTexture(VulkanDevice* device, VkImage existingImage, VkImageView existingView, const TextureDesc& desc)
        : device_(device), desc_(desc), image_(existingImage), imageView_(existingView), ownMemory_(false)
    {
        // 这种模式下不需要分配内存，也不需要创建 View (通常由 Swapchain 传进来)
        // 如果 Swapchain 只给了 Image 没给 View，这里还是需要调用 CreateView()
        if (imageView_ == nullptr)
        {
            CreateView(image_,desc_);
        }
    }
    VulkanTexture::~VulkanTexture()
    {
        if (imageView_)
        {
            vkDestroyImageView(device_->GetDevice(), imageView_, nullptr);
        }

        // 只有自己创建的才销毁
        if (ownMemory_)
        {
            if (image_)
                vkDestroyImage(device_->GetDevice(), image_, nullptr);
            if (memory_)
                vkFreeMemory(device_->GetDevice(), memory_, nullptr);
        }
    }
    void VulkanTexture::CreateImage(VulkanDevice* device, const TextureDesc& desc)
    {        
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D; // 暂时只支持 2D
        imageInfo.extent.width = desc.Width;
        imageInfo.extent.height = desc.Height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = desc.MipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = Tool::ConvertPixelFormat(desc.Format);

        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL; // GPU 专用优化布局
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.usage = Tool::ConvertImageUsage(desc.Usage);

        Tool::CheckResult(vkCreateImage(device->GetDevice(), &imageInfo, nullptr, &image_));
    }

    void VulkanTexture::AllocateMemory(VkImage image)
    {
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device_->GetDevice(), image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;

        allocInfo.memoryTypeIndex = device_->FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        Tool::CheckResult(vkAllocateMemory(device_->GetDevice(), &allocInfo, nullptr, &memory_));

        vkBindImageMemory(device_->GetDevice(), image_, memory_, 0);
    }
    void VulkanTexture::CreateView(VkImage image, const TextureDesc& desc)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        viewInfo.format = Tool::ConvertPixelFormat(desc.Format);

        if ((desc_.Usage & TextureUsage::DepthStencilAttachment)!=TextureUsage::None)
        {
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        else
        {
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = desc_.MipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        Tool::CheckResult(vkCreateImageView(device_->GetDevice(), &viewInfo, nullptr, &imageView_));
    }
}