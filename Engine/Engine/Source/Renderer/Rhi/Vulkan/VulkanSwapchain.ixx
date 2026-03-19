module;
#include <vulkan/vulkan.h>
export module Engine.Rhi.Vulkan.Swapchain;
import Engine.Rhi.Swapchain;
import Engine.Rhi.Definition;
import Engine.Rhi.Vulkan.Device;
import Engine.Rhi.Vulkan.Texture;
export namespace Engine::Rhi
{

	class VulkanSwapchain final : public RhiSwapchain
	{
	public:
		VulkanSwapchain(VulkanDevice *device, void *windowHandle, uint32_t width,
						uint32_t height);
		~VulkanSwapchain() override;

		bool Resize(uint32_t width, uint32_t height) override;
		void Present(RhiSemaphore *signalSemaphore, bool vsync) override;

		PixelFormat GetFormat() const override;
		uint32_t GetWidth() const override;
		uint32_t GetHeight() const override;
		bool AcquireNextImage(RhiSemaphore *signalSemaphore) override;
		std::shared_ptr<RhiTexture> GetCurrentBackBuffer() override;

		VkSwapchainKHR GetVulkanSwapchain() const;
		VkImageView GetCurrentImageView() const;
		VkImage GetCurrentVkImage() const;
		uint32_t GetImageCount() const override;

	private:
		void CreateSurface(void *windowHandle);
		void CreateSurfaceWithGlfw();
		void CreateSwapchain(uint32_t width, uint32_t height, bool vsync);
		void CreateImageViews();
		void Cleanup();

	private:
		VulkanDevice *vulkanDevice_;

		VkSurfaceKHR surface_{};
		VkSwapchainKHR swapchain_{};

		std::vector<VkImage> images_;
		std::vector<std::shared_ptr<VulkanTexture>> textures_;
		VkFormat vkFormat_;
		VkColorSpaceKHR vkColorSpace_;
		PixelFormat pixelFormat_ = PixelFormat::Unknown;
		uint32_t width_ = 0;
		uint32_t height_ = 0;
		uint32_t currentImageIndex_ = 0;
	};
}; // namespace Engine::Rhi
