module;
#include <functional>

module RenderTargetManager;
import Logger;
import VulkanDevice;
import VulkanRenderPass;
import VulkanTexture;
import std;

// RenderTargetDesc implementations
bool RenderTargetDesc::operator==(const RenderTargetDesc& other) const {
    return width == other.width &&
           height == other.height &&
           colorFormat == other.colorFormat &&
           depthFormat == other.depthFormat &&
           isSwapchain == other.isSwapchain;
    // Note: debugName is not included in comparison for caching purposes
}

std::size_t RenderTargetDescHash::operator()(const RenderTargetDesc& desc) const {
    std::size_t hash = 0;
    
    hash ^= desc.width + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= desc.height + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= static_cast<std::size_t>(desc.colorFormat) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= static_cast<std::size_t>(desc.depthFormat) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= desc.isSwapchain ? 1 : 0;
    
    return hash;
}

// RenderTargetManager implementations
RenderTargetManager::RenderTargetManager(RhiDevice* device)
    : device_(device), cacheHits_(0), cacheMisses_(0)
{
    Log::Info("RenderTargetManager initialized");
}

RenderTarget* RenderTargetManager::GetRenderTarget(const RenderTargetDesc& desc) {
    // Special case: swapchain render target
    if (desc.isSwapchain) {
        return GetSwapchainRenderTarget();
    }
    
    auto it = renderTargetCache_.find(desc);
    
    if (it != renderTargetCache_.end()) {
        ++cacheHits_;
        Log::Debug(std::format("RenderTarget cache hit: {}x{}", desc.width, desc.height));
        return it->second.get();
    }
    
    // Cache miss - create new render target
    ++cacheMisses_;
    Log::Debug(std::format("RenderTarget cache miss - creating {}x{}", desc.width, desc.height));
    
    auto renderTarget = CreateRenderTarget(desc);
    if (!renderTarget || !renderTarget->IsValid()) {
        Log::Error("Failed to create render target");
        return nullptr;
    }
    
    auto* renderTargetPtr = renderTarget.get();
    
    // Store in cache
    renderTargetCache_[desc] = std::move(renderTarget);
    
    Log::Debug(std::format("RenderTarget cached ({}x{}, color={}, depth={}). Total count: {}",
                         desc.width, desc.height, 
                         static_cast<int>(desc.colorFormat),
                         static_cast<int>(desc.depthFormat),
                         renderTargetCache_.size()));
    
    return renderTargetPtr;
}

RenderTarget* RenderTargetManager::GetSwapchainRenderTarget() {
    if (!swapchainRenderTarget_) {
        swapchainRenderTarget_ = CreateSwapchainRenderTarget();
        if (!swapchainRenderTarget_) {
            Log::Error("Failed to create swapchain render target");
            return nullptr;
        }
    }
    
    return swapchainRenderTarget_.get();
}

void RenderTargetManager::InvalidateSwapchainRenderTarget() {
    swapchainRenderTarget_.reset();
    Log::Info("Swapchain render target invalidated");
}

void RenderTargetManager::UpdateSwapchainFramebufferIndex() {
    if (swapchainRenderTarget_ && swapchainRenderTarget_->renderPass) {
        // Cast to VulkanDevice to get current swapchain image index
        auto* vulkanDevice = static_cast<VulkanDevice*>(device_);
        uint32_t currentImageIndex = vulkanDevice->GetCurrentSwapchainImageIndex();
        
        // Cast to VulkanRenderPass to set the framebuffer index
        auto* vulkanRenderPass = static_cast<VulkanRenderPass*>(swapchainRenderTarget_->renderPass.get());
        vulkanRenderPass->SetCurrentFramebuffer(currentImageIndex);
        
        Log::Debug(std::format("RenderTargetManager: Updated swapchain framebuffer index to {}", currentImageIndex));
    }
}

void RenderTargetManager::ClearCache() {
    Log::Debug(std::format("RenderTargetManager::ClearCache() - destroying {} cached render targets", renderTargetCache_.size()));
    
    // Explicitly destroy each render target before clearing  
    for (auto& [desc, renderTarget] : renderTargetCache_) {
        if (renderTarget) {
            Log::Debug(std::format("Destroying cached render target ({}x{})", desc.width, desc.height));
            renderTarget.reset(); // Explicit destruction
        }
    }
    
    if (swapchainRenderTarget_) {
        Log::Debug("Destroying swapchain render target...");
        swapchainRenderTarget_.reset();
    }
    
    renderTargetCache_.clear();
    cacheHits_ = 0;
    cacheMisses_ = 0;
    
    Log::Debug("RenderTargetManager cache cleared successfully");
}

void RenderTargetManager::RemoveUnusedRenderTargets() {
    // TODO: Implement reference counting to track render target usage
    // For now, this is a placeholder
    Log::Warn("RemoveUnusedRenderTargets not yet implemented - requires reference counting");
}

float RenderTargetManager::GetCacheHitRatio() const {
    size_t total = cacheHits_ + cacheMisses_;
    if (total == 0) {
        return 0.0f;
    }
    return static_cast<float>(cacheHits_) / static_cast<float>(total);
}

std::unique_ptr<RenderTarget> RenderTargetManager::CreateRenderTarget(const RenderTargetDesc& desc) {
    auto renderTarget = std::make_unique<RenderTarget>();
    renderTarget->desc = desc;
    
    // Create color texture
    RhiTextureDesc colorTextureDesc;
    colorTextureDesc.type = RhiTextureType::Texture2D;
    colorTextureDesc.width = desc.width;
    colorTextureDesc.height = desc.height;
    colorTextureDesc.depth = 1;
    colorTextureDesc.mipLevels = 1;
    colorTextureDesc.arrayLayers = 1;
    colorTextureDesc.format = desc.colorFormat;
    colorTextureDesc.usage = RhiTextureUsage::ColorAttachment | RhiTextureUsage::Sampled;
    colorTextureDesc.debugName = desc.debugName + "_Color";
    
    renderTarget->colorTexture = device_->CreateTexture(colorTextureDesc);
    if (!renderTarget->colorTexture) {
        Log::Error("Failed to create color texture for render target");
        return nullptr;
    }
    
    // Create depth texture if needed
    if (desc.depthFormat != RhiFormat::Undefined) {
        RhiTextureDesc depthTextureDesc;
        depthTextureDesc.type = RhiTextureType::Texture2D;
        depthTextureDesc.width = desc.width;
        depthTextureDesc.height = desc.height;
        depthTextureDesc.depth = 1;
        depthTextureDesc.mipLevels = 1;
        depthTextureDesc.arrayLayers = 1;
        depthTextureDesc.format = desc.depthFormat;
        depthTextureDesc.usage = RhiTextureUsage::DepthStencilAttachment;
        depthTextureDesc.debugName = desc.debugName + "_Depth";
        
        renderTarget->depthTexture = device_->CreateTexture(depthTextureDesc);
        if (!renderTarget->depthTexture) {
            Log::Error("Failed to create depth texture for render target");
            return nullptr;
        }
    }
    
    // Create render pass
    RhiRenderPassDesc renderPassDesc;
    renderPassDesc.width = desc.width;
    renderPassDesc.height = desc.height;
    
    // Color attachment
    RhiAttachmentDesc colorAttachment;
    colorAttachment.format = desc.colorFormat;
    colorAttachment.loadOp = RhiLoadOp::Clear;
    colorAttachment.storeOp = RhiStoreOp::Store;
    colorAttachment.initialLayout = RhiImageLayout::Undefined;
    colorAttachment.finalLayout = RhiImageLayout::ShaderReadOnlyOptimal;
    renderPassDesc.colorAttachments.push_back(colorAttachment);
    
    // Depth attachment (if needed)
    if (desc.depthFormat != RhiFormat::Undefined) {
        renderPassDesc.hasDepthAttachment = true;
        renderPassDesc.depthAttachment.format = desc.depthFormat;
        renderPassDesc.depthAttachment.loadOp = RhiLoadOp::Clear;
        renderPassDesc.depthAttachment.storeOp = RhiStoreOp::DontCare;
        renderPassDesc.depthAttachment.initialLayout = RhiImageLayout::Undefined;
        renderPassDesc.depthAttachment.finalLayout = RhiImageLayout::DepthStencilAttachmentOptimal;
    } else {
        renderPassDesc.hasDepthAttachment = false;
    }
    
    renderTarget->renderPass = device_->CreateRenderPass(renderPassDesc);
    if (!renderTarget->renderPass) {
        Log::Error("Failed to create render pass for render target");
        return nullptr;
    }
    
    Log::Debug(std::format("Created render target: {}x{}, color format={}, depth format={}",
                          desc.width, desc.height, 
                          static_cast<int>(desc.colorFormat),
                          static_cast<int>(desc.depthFormat)));
    
    return renderTarget;
}

std::unique_ptr<RenderTarget> RenderTargetManager::CreateSwapchainRenderTarget() {
    auto renderTarget = std::make_unique<RenderTarget>();
    
    // Get swapchain properties
    uint32_t width, height;
    device_->GetSwapchainExtent(width, height);
    RhiFormat swapchainFormat = device_->GetSwapchainFormat();
    
    // Setup descriptor
    renderTarget->desc.width = width;
    renderTarget->desc.height = height;
    renderTarget->desc.colorFormat = swapchainFormat;
    renderTarget->desc.depthFormat = RhiFormat::D32_SFLOAT; // More universally supported depth format
    renderTarget->desc.isSwapchain = true;
    renderTarget->desc.debugName = "Swapchain";
    
    // Note: For swapchain, colorTexture is managed by the swapchain itself
    // We only need to create the depth texture and render pass
    
    // Create depth texture
    RhiTextureDesc depthTextureDesc;
    depthTextureDesc.type = RhiTextureType::Texture2D;
    depthTextureDesc.width = width;
    depthTextureDesc.height = height;
    depthTextureDesc.depth = 1;
    depthTextureDesc.mipLevels = 1;
    depthTextureDesc.arrayLayers = 1;
    depthTextureDesc.format = RhiFormat::D32_SFLOAT;
    depthTextureDesc.usage = RhiTextureUsage::DepthStencilAttachment;
    depthTextureDesc.debugName = "SwapchainDepth";
    
    Log::Debug("Creating swapchain depth texture...");
    renderTarget->depthTexture = device_->CreateTexture(depthTextureDesc);
    if (!renderTarget->depthTexture) {
        Log::Error("Failed to create depth texture for swapchain render target");
        return nullptr;
    }
    
    // Create render pass manually with our existing depth texture
    // Cast to VulkanDevice to access Vulkan-specific functionality
    auto* vulkanDevice = static_cast<VulkanDevice*>(device_);
    
    // Get swapchain image views
    std::vector<VkImageView> swapchainImageViews;
    vulkanDevice->GetSwapchainImageViews(swapchainImageViews);
    
    // Get depth image view from our created depth texture
    auto* vulkanDepthTexture = static_cast<VulkanTexture*>(renderTarget->depthTexture.get());
    VkImageView depthImageView = vulkanDepthTexture->GetVkImageView();
    
    // Create VulkanRenderPass directly
    renderTarget->renderPass = Core::MakeUnique<VulkanRenderPass>(
        vulkanDevice->GetVkDevice(), width, height, swapchainFormat, RhiFormat::D32_SFLOAT);
    
    // Initialize with our depth image view
    auto* vulkanRenderPass = static_cast<VulkanRenderPass*>(renderTarget->renderPass.get());
    if (!vulkanRenderPass->Initialize(swapchainImageViews, depthImageView)) {
        Log::Error("Failed to initialize swapchain render pass");
        return nullptr;
    }
    
    Log::Debug(std::format("Created swapchain render target: {}x{}, format={}",
                         width, height, static_cast<int>(swapchainFormat)));
    
    return renderTarget;
}