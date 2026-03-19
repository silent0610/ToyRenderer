module;
#include "Math/Glm.hpp"
#include "spdlog/spdlog.h"
module Engine.Render.Pass.LightingPass;

import Engine.Rhi.Texture;

namespace Engine::Render::Pass
{
    void LightingPass::Init(Rhi::RhiDevice* device)
    {
        device_ = device;

        // 1. 创建 Layout: 4个 Binding 对应 A, B, C, Depth
        Rhi::DescriptorSetLayoutDesc desc;
        // Binding 0: GBufferA
        desc.Bindings.push_back({0, Rhi::DescriptorType::CombinedImageSampler,1, Rhi::ShaderStage::Fragment});
        // Binding 1: GBufferB
        desc.Bindings.push_back({1, Rhi::DescriptorType::CombinedImageSampler,1, Rhi::ShaderStage::Fragment});
        // Binding 2: GBufferC
        desc.Bindings.push_back({2, Rhi::DescriptorType::CombinedImageSampler,1, Rhi::ShaderStage::Fragment});
        // Binding 3: Depth
        desc.Bindings.push_back({3, Rhi::DescriptorType::CombinedImageSampler,1, Rhi::ShaderStage::Fragment});

        gbufferLayout_ = device_->CreateDescriptorSetLayout(desc);

        // 2. 创建全屏管线
        CreatePipeline();

        // 3. 创建线性采样器
        sampler_ = device_->CreateSampler(Rhi::SamplerDesc{});

        spdlog::info("LightingPass initialized.");
    }

    std::string LightingPass::GetName() const
    {
        return "LightingPass";
    }

    LightingPass::Output LightingPass::Setup(RenderGraph& rg, const Input& input)
    {
        Output output{};

        // 输出目标格式
        Rhi::TextureDesc colorDesc{.Width = 800,
                                   .Height = 600,
                                   .Format = Rhi::PixelFormat::B8G8R8A8UNORM,
                                   .Usage = Rhi::TextureUsage::ColorAttachment | Rhi::TextureUsage::TransferSrc};

        rg.AddPass(
            GetName(),
            [&](RGBuilder& builder) {
                // 1. 声明读取 GBuffer 资源
                builder.ReadTex(input.GBuffer.GBufferA);
                builder.ReadTex(input.GBuffer.GBufferB);
                builder.ReadTex(input.GBuffer.GBufferC);
                builder.ReadTex(input.GBuffer.Depth);

                // 2. 声明写入 SceneColor
                output.SceneColor = builder.CreateTexture("SceneColor", colorDesc);
                builder.SetColorRT(output.SceneColor, 0, Rhi::LoadOp::Clear);
            },
            [this, input](RGContext& ctx) {
                // 注意：这里按值捕获 input，保证 ResourceHandle 在 Lambda 执行时有效
                auto cmd = ctx.Cmd;

                cmd->SetViewport(0, 0, 800, 600);
                cmd->SetScissor(0, 0, 800, 600);
                cmd->SetPipelineState(pipeline_.get());

                // ==========================================================
                // 核心：使用瞬态分配 (Transient Allocation)
                // ==========================================================

                // A. 从当前帧的 Pool 中申请一个新的 Set
                auto set = device_->CreateDescriptorSet(gbufferLayout_.get(),true);

                // B. 更新 Descriptor Set (绑定物理纹理)
                set->UpdateTexture(0, ctx.GetTexture(input.GBuffer.GBufferA), sampler_.get());
                set->UpdateTexture(1, ctx.GetTexture(input.GBuffer.GBufferB), sampler_.get());
                set->UpdateTexture(2, ctx.GetTexture(input.GBuffer.GBufferC), sampler_.get());
                set->UpdateTexture(3, ctx.GetTexture(input.GBuffer.Depth), sampler_.get());

                // C. 绑定 Set 到管线 (Set 0)
                cmd->SetDescriptorSet(pipeline_.get(), 0, set.get());

                // D. 设置简单的方向光参数
                PushConstants push;
                push.CameraPos = glm::vec4(0, 5, 10, 1);
                push.LightDir = glm::vec4(-1, -1, -1, 0);
                push.LightColor = glm::vec4(1, 1, 1, 1);
                cmd->SetPushConstants(pipeline_.get(), Rhi::ShaderStage::Fragment, &push, sizeof(push));

                // E. 绘制全屏三角形 (3个顶点，无 IndexBuffer，无 VertexBuffer)
                // Vertex Shader 利用 gl_VertexIndex 生成坐标
                // 小心参数
                cmd->Draw(3, 0);

            });

        return output;
    }

    void LightingPass::CreatePipeline()
    {
        // 加载全屏 Shader
        auto vs = device_->CreateShader("Asset/Shader/Deferred/Lighting.vert.spv", Rhi::ShaderStage::Vertex);
        auto ps = device_->CreateShader("Asset/Shader/Deferred/Lighting.frag.spv", Rhi::ShaderStage::Fragment);

        Rhi::PipelineStateDesc desc;
        desc.VertexShader = vs.get();
        desc.FragmentShader = ps.get();

        // 输出只有一张 SceneColor
        desc.ColorFormats = {Rhi::PixelFormat::B8G8R8A8UNORM};

        // 全屏 Pass 不需要深度测试/写入
        desc.DepthFormat = Rhi::PixelFormat::Unknown;
        desc.DepthTestEnabled = false;
        desc.DepthWriteEnabled = false;

        // 不需要剔除
        desc.Culling = Rhi::CullMode::None;
        // 不需要 Vertex Input (完全靠 Shader 生成)
        desc.InputLayout = {};
        desc.PushConstantStages = Rhi::ShaderStage::Fragment;
        desc.PushConstantSize = sizeof(PushConstants);
        desc.ResourceLayouts = {gbufferLayout_.get()};

        pipeline_ = device_->CreatePipeline(desc);
    }
} // namespace Engine::Render::Pass