module;
#include "vulkan/vulkan.h"
#include "spdlog/spdlog.h"
module Engine.Rhi.Vulkan.Pipeline;
import Engine.Rhi.Vulkan.Descriptor;
namespace Engine::Rhi
{
    VulkanPipeline::VulkanPipeline(VulkanDevice* device, const PipelineStateDesc& desc)
        : device_{device}, pipelineType_{Type::Graphics}, bindPoint_{VK_PIPELINE_BIND_POINT_GRAPHICS}
	{
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        //  Shader
        if (desc.VertexShader)
        {
            auto vkS = static_cast<VulkanShader*>(desc.VertexShader);
            VkPipelineShaderStageCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            info.stage = VK_SHADER_STAGE_VERTEX_BIT;
            info.module = vkS->GetShaderModule();
            info.pName = vkS->GetEntryPoint();
            shaderStages.push_back(info);
        }
        if (desc.FragmentShader)
        {
            auto vkS = static_cast<VulkanShader*>(desc.FragmentShader);
            VkPipelineShaderStageCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            info.module = vkS->GetShaderModule();
            info.pName = vkS->GetEntryPoint();
            shaderStages.push_back(info);
        }
        if (desc.GeometryShader)
        {
            spdlog::warn("Using Geometry Shader, make sure device supports geometryShader feature.");
            auto vkS = static_cast<VulkanShader*>(desc.GeometryShader);
            VkPipelineShaderStageCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            info.module = vkS->GetShaderModule();
            info.pName = vkS->GetEntryPoint();
            shaderStages.push_back(info);
        }

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        std::vector<VkVertexInputBindingDescription> bindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        // 如果 InputLayout 为空，说明可能是硬编码三角形 (Vertex ID)，不需要 Buffer
        if (!desc.InputLayout.empty())
        {
            // 这是一个简单的自动计算 Stride 的逻辑
            // 假设所有数据都在 Binding 0 上 (MVP 阶段够用了)
            uint32_t stride = 0;
            for (const auto& element : desc.InputLayout)
            {
                VkVertexInputAttributeDescription attr{};
                attr.binding = element.Binding;
                attr.location = element.Location;
                attr.format = Tool::ConvertVertexFormat(element.Format);
                attr.offset = element.Offset;
                attributeDescriptions.push_back(attr);

                // 简单的 stride 估算 (假设最后一个元素在最后)
                // 实际生产环境应该由用户传入 Stride
                uint32_t elementSize = Tool::GetVertexFormatSize(element.Format);
                if (element.Offset + elementSize > stride)
                {
                    stride = element.Offset + elementSize;
                }
            }

            // 如果用户没算 stride，我们很难猜对，这里先留个占位符逻辑
            // 建议在 PipelineStateDesc 里显式增加 vertexStride 字段
            // 现在为了防止 crash，如果 input 不为空，我们至少加一个 binding
            VkVertexInputBindingDescription bindingDesc{};
            bindingDesc.binding = 0;
            bindingDesc.stride = stride; // TODO: Calculate proper stride
            bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            bindingDescriptions.push_back(bindingDesc);

            vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
            vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
            vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        }

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // 默认三角形列表
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // 视口状态指针需要存在，虽然因为是 Dynamic 所以里面的值会被忽略
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = Tool::ConvertPolygonMode(desc.Polygon);
        rasterizer.lineWidth = desc.LineWidth;
        rasterizer.cullMode = Tool::ConvertCullMode(desc.Culling);
             // 顺时针为正面 (Vulkan标准)
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // 7. Depth Stencil
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        bool hasDepth = (desc.DepthFormat != PixelFormat::Unknown);
        if (hasDepth)
        {
            depthStencil.depthTestEnable = desc.DepthTestEnabled ? VK_TRUE : VK_FALSE;
            depthStencil.depthWriteEnable = desc.DepthWriteEnabled ? VK_TRUE : VK_FALSE;
            depthStencil.depthCompareOp = Tool::ConvertCompareOp(desc.DepthCompareOp);
        }
        else
        {
            depthStencil.depthTestEnable = VK_FALSE;
            depthStencil.depthWriteEnable = VK_FALSE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        }
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        // 8. Color Blending
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
        for (size_t i = 0; i < desc.ColorFormats.size(); i++)
        {
            VkPipelineColorBlendAttachmentState attachment{};
            attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            // 这里可以针对每个 RT 做不同的 Blend 设置
            // 为了 MVP，我们假设所有 RT 使用相同的 Blend 配置
            if (desc.Blend == BlendMode::Opaque)
            {
                attachment.blendEnable = VK_FALSE;
            }
            else if (desc.Blend == BlendMode::AlphaBlend)
            {
                attachment.blendEnable = VK_TRUE;
                attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attachment.colorBlendOp = VK_BLEND_OP_ADD;
                attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                attachment.alphaBlendOp = VK_BLEND_OP_ADD;
            }
            else
            {
                attachment.blendEnable = VK_FALSE;
            }
            blendAttachments.push_back(attachment);
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
        colorBlending.pAttachments = blendAttachments.data();
        

        // PipelineLayout

        std::vector<VkDescriptorSetLayout> setLayouts;
        for (auto* rhiLayout : desc.ResourceLayouts)
        {
            if (rhiLayout)
            {
                auto vkLayout = static_cast<VulkanDescriptorSetLayout*>(rhiLayout);
                setLayouts.push_back(vkLayout->GetHandle());
            }
        }

        std::vector<VkPushConstantRange> pushConstantRanges;
        if (desc.PushConstantSize > 0)
        {
            VkPushConstantRange range{};
            range.stageFlags = Tool::ConvertShaderStage(desc.PushConstantStages);
            range.offset = 0;
            range.size = desc.PushConstantSize; // 比如 sizeof(Matrix4x4)

            pushConstantRanges.push_back(range);
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
        pipelineLayoutInfo.pSetLayouts = setLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
        pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();



        Tool::CheckResult(vkCreatePipelineLayout(device_->GetDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout_));

        VkPipelineRenderingCreateInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

        // 转换所有颜色格式
        std::vector<VkFormat> vkColorFormats;
        for (auto fmt : desc.ColorFormats)
        {
            vkColorFormats.push_back(Tool::ConvertPixelFormat(fmt));
        }

        renderingInfo.colorAttachmentCount = static_cast<uint32_t>(vkColorFormats.size());
        renderingInfo.pColorAttachmentFormats = vkColorFormats.data();
        if (hasDepth)
        {
            renderingInfo.depthAttachmentFormat = Tool::ConvertPixelFormat(desc.DepthFormat);
        }
        else
        {
            renderingInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &renderingInfo;

        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();

        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;

        pipelineInfo.layout = pipelineLayout_;
        pipelineInfo.renderPass = VK_NULL_HANDLE; // 必须为 NULL (因为使用了 Dynamic Rendering)
        pipelineInfo.subpass = 0;

        Tool::CheckResult(vkCreateGraphicsPipelines(device_->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_));

    }
    VulkanPipeline::VulkanPipeline(VulkanDevice* device, const ComputePipelineDesc& desc)
        : device_(device), pipelineType_(Type::Compute), bindPoint_(VK_PIPELINE_BIND_POINT_COMPUTE)
    {
        if (!desc.ComputeShader)
        {
            throw std::runtime_error("Compute Pipeline requires a valid Compute Shader!");
        }

        auto vkCS = static_cast<VulkanShader*>(desc.ComputeShader);

        // 1. Shader Stage
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = vkCS->GetShaderModule();
        stageInfo.pName = vkCS->GetEntryPoint();

        // 2. Layout (Compute 也需要 Layout 来绑定 Buffer/Image)

        std::vector<VkPushConstantRange> pushConstantRanges;
        if (desc.PushConstantSize > 0)
        {
            VkPushConstantRange range{};
            // 简单起见，我们让它对 Vertex 和 Fragment 都可见
            // 如果是 Compute Pipeline，这里改成 VK_SHADER_STAGE_COMPUTE_BIT
            range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            range.offset = 0;
            range.size = desc.PushConstantSize; // 比如 sizeof(Matrix4x4)

            pushConstantRanges.push_back(range);
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        // TODO 目前 MVP 没有 SetLayout，未来在这里添加
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
        pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

        if (vkCreatePipelineLayout(device_->GetDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create compute pipeline layout!");
        }

        // 3. Create Compute Pipeline
        // Compute Pipeline 非常简单，不需要 RenderPass，不需要 Viewport
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = pipelineLayout_;
        pipelineInfo.stage = stageInfo;

        Tool::CheckResult(vkCreateComputePipelines(device->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_));

    }
    VkPipelineBindPoint VulkanPipeline::GetBindPoint() const
    {
        return bindPoint_;
    }
    VulkanPipeline::~VulkanPipeline()
    {
        if (pipeline_)
            vkDestroyPipeline(device_->GetDevice(), pipeline_, nullptr);
        if (pipelineLayout_)
            vkDestroyPipelineLayout(device_->GetDevice(), pipelineLayout_, nullptr);
    }

    VkPipeline VulkanPipeline::GetPipeline() const
    {
        return pipeline_;
    }
    VkPipelineLayout VulkanPipeline::GetPipelineLayout() const
    {
        return pipelineLayout_;
    }
    Engine::Rhi::Pipeline::Type VulkanPipeline::GetType() const
    {
        return pipelineType_;
    }
}