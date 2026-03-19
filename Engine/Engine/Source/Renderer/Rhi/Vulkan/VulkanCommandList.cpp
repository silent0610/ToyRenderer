module;
#include "vulkan/vulkan.h"
#include "spdlog/spdlog.h"
module Engine.Rhi.Vulkan.CommandList;
import Engine.Rhi.Vulkan.Swapchain;
import Engine.Rhi.Vulkan.Pipeline;
import Engine.Rhi.Pipeline;
import Engine.Rhi.Texture;
import Engine.Rhi.Vulkan.Texture;
import Engine.Rhi.Vulkan.Buffer;
import Engine.Rhi.Buffer;
import Engine.Rhi.Vulkan.Device;
import Engine.Rhi.Vulkan.Descriptor;
namespace Engine::Rhi
{
    struct VulkanLayoutInfo
    {
        VkImageLayout Layout;
        VkAccessFlags Access;
        VkPipelineStageFlags Stage;
    };
    static VulkanLayoutInfo GetLayoutInfo(ResourceState state)
    {
        switch (state)
        {
        case ResourceState::Undefined:
            return {VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};

        case ResourceState::RenderTarget:
            return {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        case ResourceState::ShaderResource:
            return {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT}; // 假设是 PS 读取

        case ResourceState::TransferDst:
            return {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};

        case ResourceState::TransferSrc:
            return {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};

        case ResourceState::Present:
            return {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};
        case ResourceState::DepthStencilWrite:
            return {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT};
        default:
            return {VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
        }
    }

    struct VulkanBufferBarrierInfo
    {
        VkAccessFlags Access;
        VkPipelineStageFlags Stage;
    };

    static VulkanBufferBarrierInfo GetBufferBarrierInfo(ResourceState state)
    {
        switch (state)
        {
        case ResourceState::Undefined:
            return {0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};

        case ResourceState::VertexBuffer:
            return {VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT};

        case ResourceState::IndexBuffer:
            return {VK_ACCESS_INDEX_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT};

        case ResourceState::UniformBuffer:
            return {VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};

        case ResourceState::ShaderResource:
            return {VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};

        case ResourceState::UnorderedAccess:
            return {VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

        case ResourceState::TransferSrc:
            return {VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};

        case ResourceState::TransferDst:
            return {VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
        
        case ResourceState::General:
             return {VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};

        default:
            return {0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};
        }
    }

    VulkanCommandList::VulkanCommandList(VulkanDevice *device) : device_(device)
    {
        VkCommandPoolCreateInfo poolInfo{};

        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = device_->GetGraphicsQueueFamilyIndex();

        Tool::CheckResult(vkCreateCommandPool(device_->GetDevice(), &poolInfo, nullptr, &commandPool_));

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool_;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        Tool::CheckResult(vkAllocateCommandBuffers(device_->GetDevice(), &allocInfo, &commandBuffer_));
    };
    void VulkanCommandList::SetPushConstants(Pipeline *pipeline, ShaderStage stage, const void *data, uint32_t size)
    {
        auto vkPipeline = static_cast<VulkanPipeline *>(pipeline);
        VkShaderStageFlags stageFlags = Tool::ConvertShaderStage(stage);


        vkCmdPushConstants(commandBuffer_,
                           vkPipeline->GetPipelineLayout(), // 必须从 Pipeline 拿到 Layout
                           stageFlags,
                           0, // offset (简化起见从0开始)
                           size, data);
    }
    VulkanCommandList::~VulkanCommandList()
    {
        if (commandPool_)
        {
            vkDestroyCommandPool(device_->GetDevice(), commandPool_, nullptr);
        }
    }
    void VulkanCommandList::Begin()
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // 允许一次性提交
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer_, &beginInfo);
        isRecording_ = true;
    }

    void VulkanCommandList::End()
    {
        vkEndCommandBuffer(commandBuffer_);
        isRecording_ = false;
    }
    void VulkanCommandList::TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        // --- 1. 推导 Source (之前的操作) ---
        switch (oldLayout)
        {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            barrier.srcAccessMask = 0;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        default:
            // 如果遇到未处理的状态，为了安全，假设它可能在任何阶段被写过
            // 但这在生产环境是不推荐的
            barrier.srcAccessMask = 0;
            sourceStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;
        }

        // --- 2. 推导 Destination (接下来的操作) ---
        switch (newLayout)
        {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            barrier.dstAccessMask = 0; // Present 操作不需要特定的 AccessMask
            destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // 或 Vertex
            break;

        default:
            barrier.dstAccessMask = 0;
            destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
        }

        // 特殊修正：如果是从 Undefined 转换，SrcStage 必须是 TOP_OF_PIPE
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }

        vkCmdPipelineBarrier(commandBuffer_, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void VulkanCommandList::BeginRendering(const RenderPassInfo &passInfo)
    {
        // 1. 清空上一轮的追踪列表
        activeColorImages_.clear();

        // 2. 准备 Vulkan 附件信息数组
        // vector 必须在 vkCmdBeginRendering 调用前保持存活
        static std::vector<VkRenderingAttachmentInfo> vkColorAttachments;
        vkColorAttachments.clear(); // 复用 vector 减少分配

        uint32_t renderWidth = 0;
        uint32_t renderHeight = 0;

        // 3. 遍历颜色附件
        for (const auto &target : passInfo.ColorTargets)
        {
            if (!target.Texture)
            {
                continue;
            }

            // 强转为 VulkanTexture
            auto vkTex = static_cast<VulkanTexture *>(target.Texture);
            VkImage image = vkTex->GetImage();
            VkImageView view = vkTex->GetImageView();

            // 记录尺寸 (以第一个附件为准)
            if (renderWidth == 0)
            {
                renderWidth = vkTex->GetWidth();
                renderHeight = vkTex->GetHeight();
            }

            // 记录到追踪列表 (为了 EndRendering)
            activeColorImages_.push_back(image);


            // 填充 VkRenderingAttachmentInfo
            VkRenderingAttachmentInfo attachInfo{};
            attachInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            attachInfo.imageView = view;
            attachInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachInfo.loadOp = Tool::ConvertLoadOp(target.Load);
            attachInfo.storeOp = Tool::ConvertStoreOp(target.Store);
            attachInfo.clearValue.color = {target.ClearValue.r, target.ClearValue.g, target.ClearValue.b, target.ClearValue.a};

            vkColorAttachments.push_back(attachInfo);
        }
        // 4. 处理深度附件 (如果启用且存在纹理)
        VkRenderingAttachmentInfo depthAttachmentInfo{};

        // 标记是否使用了深度或模板
        bool hasDepth = passInfo.EnableDepth && passInfo.DepthStencilTarget.Texture;
        bool hasStencil = passInfo.EnableStencil && passInfo.DepthStencilTarget.Texture;

        if (hasDepth || hasStencil)
        {
            auto vkDepthTex = static_cast<VulkanTexture *>(passInfo.DepthStencilTarget.Texture);
            VkImage depthImage = vkDepthTex->GetImage();
            VkImageView depthView = vkDepthTex->GetImageView();

            if (renderWidth == 0)
            {
                renderWidth = vkDepthTex->GetWidth();
                renderHeight = vkDepthTex->GetHeight();
            }
            depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachmentInfo.imageView = depthView;
            depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            // 设置 Load/Store 操作
            // 注意：这里设置的是 loadOp/storeOp，不是 stencilLoadOp
            // 当这个结构体被赋给 pDepthAttachment 时，它控制深度
            // 当这个结构体被赋给 pStencilAttachment 时，它控制模板
            depthAttachmentInfo.loadOp = Tool::ConvertLoadOp(passInfo.DepthStencilTarget.Load);
            depthAttachmentInfo.storeOp = Tool::ConvertStoreOp(passInfo.DepthStencilTarget.Store);

            // 设置清除值
            depthAttachmentInfo.clearValue.depthStencil = {passInfo.ClearDepth, passInfo.ClearStencil};
        }

        // 5. 组装最终的渲染信息
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, {renderWidth, renderHeight}};
        renderingInfo.layerCount = 1;

        // 绑定颜色
        renderingInfo.colorAttachmentCount = static_cast<uint32_t>(vkColorAttachments.size());
        renderingInfo.pColorAttachments = vkColorAttachments.data();

        // 绑定深度
        if (hasDepth)
        {
            renderingInfo.pDepthAttachment = &depthAttachmentInfo;
        }
        else
        {
            renderingInfo.pDepthAttachment = nullptr;
        }

        // 绑定模板 (通常和深度共用一个 View，且复用配置)
        if (hasStencil)
        {
            renderingInfo.pStencilAttachment = &depthAttachmentInfo;
        }
        else
        {
            renderingInfo.pStencilAttachment = nullptr;
        }

        // 6. 调用 Vulkan 动态渲染命令
        vkCmdBeginRendering(commandBuffer_, &renderingInfo);
    }
    void VulkanCommandList::EndRendering()
    {
        vkCmdEndRendering(commandBuffer_);
        // 清空列表
        activeColorImages_.clear();
    }
    void VulkanCommandList::SetViewport(float x, float y, float width, float height)
    {
        VkViewport viewport{};
        viewport.x = x;
        viewport.y = y; // 如果想翻转 Y 轴，这里设为 height + y, height 设为负数
        viewport.width = width;
        viewport.height = height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);
    }
    void VulkanCommandList::SetScissor(int x, int y, uint32_t width, uint32_t height)
    {
        VkRect2D scissor{};
        scissor.offset = {x, y};
        scissor.extent = {width, height};

        vkCmdSetScissor(commandBuffer_, 0, 1, &scissor);
    }
    void VulkanCommandList::Draw(uint32_t vertexCount, uint32_t firstVertex)
    {
        vkCmdDraw(commandBuffer_, vertexCount, 1, firstVertex, 0);
    }
    void VulkanCommandList::DebugBegin()
    {
    }
    void VulkanCommandList::DebugEnd()
    {
    }
    void VulkanCommandList::SetPipelineState(const Pipeline *pipeline)
    {
        auto vkPipeline = static_cast<const VulkanPipeline *>(pipeline);

        vkCmdBindPipeline(commandBuffer_, vkPipeline->GetBindPoint(), vkPipeline->GetPipeline());
    }
    void VulkanCommandList::SetVertexBuffer(const RhiBuffer *buffer)
    {
        auto vkBuffer = static_cast<const VulkanBuffer *>(buffer);
        VkBuffer buf = vkBuffer->GetBuffer();
        VkDeviceSize offset = 0;
        VkDeviceSize offsets[] = {offset};
        uint32_t binding = 0;
        vkCmdBindVertexBuffers(commandBuffer_, binding, 1, &buf, offsets);
    }
    void VulkanCommandList::SetResourceBarrier(const TextureBarrier &barrier)
    {
        auto vkTex = static_cast<VulkanTexture*>(barrier.Texture);

        VulkanLayoutInfo srcInfo = GetLayoutInfo(barrier.Before);
        VulkanLayoutInfo dstInfo = GetLayoutInfo(barrier.After);

        VkImageMemoryBarrier vkBarrier{};
        vkBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        vkBarrier.oldLayout = srcInfo.Layout;
        vkBarrier.newLayout = dstInfo.Layout;
        vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.image = vkTex->GetImage();
        vkBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        if (Tool::IsDepthFormat(vkTex->GetDesc().Format))
        {
            vkBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        else if (barrier.After == Rhi::ResourceState::DepthStencilWrite)
        {
            vkBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        vkBarrier.subresourceRange.baseMipLevel = 0;
        vkBarrier.subresourceRange.levelCount = 1;
        vkBarrier.subresourceRange.baseArrayLayer = 0;
        vkBarrier.subresourceRange.layerCount = 1;

        vkBarrier.srcAccessMask = srcInfo.Access;
        vkBarrier.dstAccessMask = dstInfo.Access;

        vkCmdPipelineBarrier(commandBuffer_, srcInfo.Stage, dstInfo.Stage, 0, 0, nullptr, 0, nullptr, 1, &vkBarrier);
    }
        void VulkanCommandList::SetResourceBarrier(const BufferBarrier &barrier)
    {
        auto vkBuf = static_cast<VulkanBuffer*>(barrier.Buffer);

        VulkanBufferBarrierInfo srcInfo = GetBufferBarrierInfo(barrier.Before);
        VulkanBufferBarrierInfo dstInfo = GetBufferBarrierInfo(barrier.After);

        VkBufferMemoryBarrier vkBarrier{};
        vkBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vkBarrier.srcAccessMask = srcInfo.Access;
        vkBarrier.dstAccessMask = dstInfo.Access;
        vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.buffer = vkBuf->GetBuffer();
        vkBarrier.offset = 0;
        vkBarrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(commandBuffer_, srcInfo.Stage, dstInfo.Stage, 0, 0, nullptr, 1, &vkBarrier, 0, nullptr);
    }
    void VulkanCommandList::BlitTexture(RhiTexture *src, RhiTexture *dst, FilterMode filter)
    {
        auto vkSrc = static_cast<VulkanTexture *>(src);
        auto vkDst = static_cast<VulkanTexture *>(dst);

        // 1. 处理屏障：转换布局
        // 源图：可能是画完的状态 (COLOR_ATTACHMENT) -> 转为 TRANSFER_SRC
        TransitionImageLayout(vkSrc->GetImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        // 目标图 (Swapchain)：可能是 UNDEFINED -> 转为 TRANSFER_DST
        TransitionImageLayout(vkDst->GetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // 2. 定义拷贝区域 (全屏拷贝)
        VkImageBlit blitRegion{};
        // 源区域
        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcOffsets[0] = {0, 0, 0};
        blitRegion.srcOffsets[1] = {(int32_t)vkSrc->GetWidth(), (int32_t)vkSrc->GetHeight(), 1};

        // 目标区域
        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstOffsets[0] = {0, 0, 0};
        blitRegion.dstOffsets[1] = {(int32_t)vkDst->GetWidth(), (int32_t)vkDst->GetHeight(), 1};

        // 3. 执行 Blit
        vkCmdBlitImage(commandBuffer_, vkSrc->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkDst->GetImage(),
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion,
                       VK_FILTER_LINEAR // 或 VK_FILTER_NEAREST
        );

        // 4. (可选) 恢复状态
        // 通常 Blit 后，目标图(Swapchain)需要转为 PRESENT_SRC 准备显示
        // 这一步也可以留给 EndRendering 或者外部处理，但为了方便，我们在这里转好目标图
        TransitionImageLayout(vkDst->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
    void VulkanCommandList::CopyBuffer(RhiBuffer *src, RhiBuffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
    {
        auto vkSrc = static_cast<VulkanBuffer *>(src);
        auto vkDst = static_cast<VulkanBuffer *>(dst);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = srcOffset;
        copyRegion.dstOffset = dstOffset;
        copyRegion.size = size;

        vkCmdCopyBuffer(commandBuffer_, vkSrc->GetBuffer(), vkDst->GetBuffer(), 1, &copyRegion);
    }

    void VulkanCommandList::SetIndexBuffer(RhiBuffer* buffer, uint64_t offset)
    {
        auto vkBuffer = static_cast<VulkanBuffer*>(buffer);
        // VK_INDEX_TYPE_UINT32 表示索引是 unsigned int (32位)
        vkCmdBindIndexBuffer(commandBuffer_, vkBuffer->GetBuffer(), offset, VK_INDEX_TYPE_UINT32);
    }

    void VulkanCommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset,
                                        uint32_t firstInstance)
    {
        vkCmdDrawIndexed(commandBuffer_, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void VulkanCommandList::SetDescriptorSet(const Pipeline* pipeline, uint32_t setIndex, const RhiDescriptorSet* set)
    {
        auto vkPipeline = static_cast<const VulkanPipeline*>(pipeline);
        auto vkSet = static_cast<const VulkanDescriptorSet*>(set);

        VkDescriptorSet vkSetHandle = vkSet->GetHandle();

        // 绑定
        vkCmdBindDescriptorSets(commandBuffer_,
                                vkPipeline->GetBindPoint(), // GRAPHICS 或 COMPUTE
                                vkPipeline->GetPipelineLayout(),    // Pipeline Layout
                                setIndex,                   // First Set
                                1,                          // DescriptorSet Count
                                &vkSetHandle,               // pDescriptorSets
                                0,                          // Dynamic Offset Count
                                nullptr                     // Dynamic Offsets
        );
    }

    void VulkanCommandList::CopyBufferToTexture(const RhiBuffer* source, const RhiTexture* dest, const BufferTextureCopyRegion& region)
    {
        auto vkBuf = static_cast<const VulkanBuffer*>(source);
        auto vkTex = static_cast<const VulkanTexture*>(dest);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = region.BufferOffset;
        copyRegion.bufferRowLength = 0; // 0 表示紧密排列
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;

        copyRegion.imageOffset = {0, 0, 0};
        copyRegion.imageExtent = {region.TextureWidth, region.TextureHeight, region.TextureDepth};

        vkCmdCopyBufferToImage(commandBuffer_, vkBuf->GetBuffer(), vkTex->GetImage(),
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // 必须是这个布局才能拷贝
                               1, &copyRegion);
    }

} // namespace Engine::Rhi