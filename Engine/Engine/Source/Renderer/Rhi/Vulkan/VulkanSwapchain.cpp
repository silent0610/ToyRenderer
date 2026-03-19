module;
#include <vulkan/vulkan.h>
#include "GLFW/glfw3.h"
#include "spdlog/spdlog.h"
module Engine.Rhi.Vulkan.Swapchain;
import Engine.Rhi.Vulkan.Device;
import Engine.Rhi.Vulkan.Tool;
import Engine.Rhi.Vulkan.Sync;
import Engine.Rhi.Definition;
namespace Engine::Rhi
{
    VulkanSwapchain::VulkanSwapchain(VulkanDevice *device, void *windowHandle, uint32_t width, uint32_t height)
        : vulkanDevice_{device}, width_(width), height_{height}
    {
        CreateSurface(windowHandle);
        CreateSwapchain(width, height, false);
    }
    VulkanSwapchain::~VulkanSwapchain()
    {
        Cleanup();
        if (surface_)
        {
            vkDestroySurfaceKHR(vulkanDevice_->GetInstance(), surface_, nullptr);
        }
    }
    uint32_t VulkanSwapchain::GetImageCount() const
    {
        return static_cast<uint32_t>(images_.size());
    }
    void VulkanSwapchain::CreateSurface(void *windowHandle)
    {
        GLFWwindow *window = static_cast<GLFWwindow *>(windowHandle);

        Engine::Rhi::Tool::CheckResult(glfwCreateWindowSurface(
            vulkanDevice_->GetInstance(),
            window,
            nullptr,
            &surface_));
    }
    void VulkanSwapchain::CreateSurfaceWithGlfw()
    {
        ;
    }
    void VulkanSwapchain::CreateSwapchain(uint32_t width, uint32_t height, bool vsync)
    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanDevice_->GetPhysicalDevice(), surface_, &capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanDevice_->GetPhysicalDevice(), surface_, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanDevice_->GetPhysicalDevice(), surface_, &formatCount, formats.data());

        VkSurfaceFormatKHR selectedFormat = formats[0];
        for (const auto &fmt : formats)
        {
            if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                selectedFormat = fmt;
                break;
            }
        }
        vkFormat_ = selectedFormat.format;
        vkColorSpace_ = selectedFormat.colorSpace;

        if (vkFormat_ == VK_FORMAT_B8G8R8A8_UNORM)
            pixelFormat_ = PixelFormat::B8G8R8A8UNORM;

        // 获取呈现模式 (Present Mode)
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(vulkanDevice_->GetPhysicalDevice(), surface_, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(vulkanDevice_->GetPhysicalDevice(), surface_, &presentModeCount, presentModes.data());

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (!vsync)
        {
            for (const auto &mode : presentModes)
            {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    presentMode = mode;
                    break;
                }
            }
        }

        VkExtent2D extent;
        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            extent = capabilities.currentExtent;
        }
        else
        {
            extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }
        width_ = extent.width;
        height_ = extent.height;

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        {
            imageCount = capabilities.maxImageCount;
        }

        // C. 创建 Swapchain
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface_;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = vkFormat_;
        createInfo.imageColorSpace = vkColorSpace_;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // 队列所有权设置 (如果 Graphics 和 Present 是同一个队列，则独占模式)
        // 目前简单起见，假设它们相同
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // 不透明窗口
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;             // 允许被其他窗口遮挡时剔除像素
        createInfo.oldSwapchain = VK_NULL_HANDLE; // Resize 时需要传旧的

        Tool::CheckResult(vkCreateSwapchainKHR(vulkanDevice_->GetDevice(), &createInfo, nullptr, &swapchain_));

        vkGetSwapchainImagesKHR(vulkanDevice_->GetDevice(), swapchain_, &imageCount, nullptr);
        images_.resize(imageCount);
        vkGetSwapchainImagesKHR(vulkanDevice_->GetDevice(), swapchain_, &imageCount, images_.data());

        CreateImageViews();

        spdlog::info("[Vulkan] Swapchain Created.Size : {}x{}, Image: {}", width_, height_, imageCount);
    }
    VkImageView VulkanSwapchain::GetCurrentImageView() const
    {
        if (currentImageIndex_ < textures_.size())
        {
            return textures_[currentImageIndex_]->GetImageView();
        }
        return VK_NULL_HANDLE;
    }
    void VulkanSwapchain::CreateImageViews()
    {
        textures_.resize(images_.size());
        TextureDesc desc;
        desc.Width = width_;
        desc.Height = height_;
        desc.Format = pixelFormat_; // 确保你在 CreateSwapchain 里正确设置了 pixelFormat_
        // 关键：标记为 ColorAttachment，这样 CommandList 才知道怎么转换屏障
        desc.Usage = TextureUsage::ColorAttachment;
        desc.Name = "SwapchainBackBuffer";

        for (size_t i = 0; i < images_.size(); i++)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = images_[i];

            // 确保 viewType 和 format 正确
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = vkFormat_; // 使用之前保存的 VkFormat

            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            VkImageView imageView;
            VkResult res = vkCreateImageView(vulkanDevice_->GetDevice(), &createInfo, nullptr, &imageView);
            if (res != VK_SUCCESS)
            {
                // Log Error
                throw std::runtime_error("Failed to create swapchain image view");
            }

            // 🔥 关键步骤：包装成 RhiTexture
            // 使用构造函数 B: (Device, ExistingImage, ExistingView, Desc)
            // 这样 VulkanTexture 析构时会自动销毁这个 imageView，但不销毁 image (因为那是 Swapchain 的)
            textures_[i] = std::make_shared<VulkanTexture>(vulkanDevice_, images_[i], imageView, desc);
        }
    }
    void VulkanSwapchain::Cleanup()
    {
        textures_.clear();

        if (swapchain_)
        {
            vkDestroySwapchainKHR(vulkanDevice_->GetDevice(), swapchain_, nullptr);
            swapchain_ = VK_NULL_HANDLE;
        }
    }

    bool VulkanSwapchain::Resize(uint32_t width, uint32_t height)
    {
        vulkanDevice_->WaitIdle(); // 必须等 GPU 停下来才能销毁旧的
        Cleanup();
        CreateSwapchain(width, height, false);
        return true;
    }

    void VulkanSwapchain::Present(RhiSemaphore *waitSemaphore, bool vsync)
    {

        // 1. 将抽象基类转换为 Vulkan 具体实现
        // 这个信号量是由 FrameResource 传入的 "RenderFinished"
        auto vkSema = static_cast<VulkanSemaphore *>(waitSemaphore);
        VkSemaphore semaphoreHandle = vkSema->GetHandle();

        // 2. 准备 Present Info
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        // 只有当 GPU 执行完 CommandBuffer 里的绘制指令，signalSemaphore 变绿后，
        // 这里的 Present 才会真正把图片送去显示器。
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &semaphoreHandle;

        VkSwapchainKHR swapchains[] = {swapchain_};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &currentImageIndex_;

        VkResult result = vkQueuePresentKHR(vulkanDevice_->GetGraphicsQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            Resize(width_, height_);
        }
    };
    VkImage VulkanSwapchain::GetCurrentVkImage() const
    {
        // currentImageIndex_ 是在 AcquireNextImage 时获取的
        // images_ 是 CreateSwapchain 时保存的 vector<VkImage>
        if (images_.empty())
            return VK_NULL_HANDLE;
        return images_[currentImageIndex_];
    }
    std::shared_ptr<RhiTexture> VulkanSwapchain::GetCurrentBackBuffer()
    {
        // TODO: 等以后实现了 VulkanTexture 和 Image 包装后，这里返回当前帧的 Texture
        if (currentImageIndex_ < textures_.size())
        {
            return textures_[currentImageIndex_];
        }
        return nullptr;
    }
    PixelFormat VulkanSwapchain::GetFormat() const
    {
        // 假设你在 .ixx 定义了 pixelFormat_ 成员变量
        // 如果你的成员变量叫 vkFormat_，你需要先转换一下或者直接返回 pixelFormat_
        return pixelFormat_;
    }

    uint32_t VulkanSwapchain::GetWidth() const
    {
        return width_;
    }

    uint32_t VulkanSwapchain::GetHeight() const
    {
        return height_;
    }
    bool VulkanSwapchain::AcquireNextImage(RhiSemaphore *signalSemaphore)
    {
        auto vkSema = static_cast<VulkanSemaphore *>(signalSemaphore);
        VkSemaphore semaphoreHandle = vkSema->GetHandle();

        // 2. 获取下一张图
        // 告诉 Vulkan：一旦找到空闲图片，请把 semaphoreHandle 变绿 (Signal)
        VkResult result = vkAcquireNextImageKHR(
            vulkanDevice_->GetDevice(),
            swapchain_,
            UINT64_MAX,
            semaphoreHandle, //
            VK_NULL_HANDLE,
            &currentImageIndex_);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            spdlog::warn("Resize Swapchain");
            Resize(width_, height_);
            return false;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            spdlog::error("Failed to acquire swapchain image!");
            return false;
        }

        return true;
    }
};
