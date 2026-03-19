module;
#include "Math/Glm.hpp"
#include "spdlog/spdlog.h"
module Engine.Render.Pass.GeometryPass;

import Engine.Rhi.Texture; // 导入 Texture 定义

namespace Engine::Render::Pass
{
    void GeometryPass::Init(Rhi::RhiDevice* device, Rhi::DescriptorSetLayout* materialLayout)
    {
        device_ = device;
        materialLayout_ = materialLayout;

        CreatePipeline();

        spdlog::info("GeometryPass initialized.");
    }

    std::string GeometryPass::GetName() const
    {
        return "GeometryPass";
    }

    GeometryPass::Output GeometryPass::Setup(RenderGraph& rg, const Input& input)
    {
        // 暂时写死分辨率，实际应从 Config 获取
        const uint32_t width = 800;
        const uint32_t height = 600;

        // 1. 定义 GBuffer 格式
        // RT0: Albedo (RGB) + Metallic (A)
        Rhi::TextureDesc descA{.Width = width,
                               .Height = height,
                               .Format = Rhi::PixelFormat::R8G8B8A8UNORM,
                               .Usage = Rhi::TextureUsage::ColorAttachment | Rhi::TextureUsage::Sampled};
        // RT1: Normal (RGB) + Roughness (A) - 需要高精度
        Rhi::TextureDesc descB{.Width = width,
                               .Height = height,
                               .Format = Rhi::PixelFormat::R8G8B8A8UNORM,
                               .Usage = Rhi::TextureUsage::ColorAttachment | Rhi::TextureUsage::Sampled};
        // RT2: Position (RGB) - 需要高精度
        Rhi::TextureDesc descC = descB;

        Rhi::TextureDesc descDepth{.Width = width,
                                   .Height = height,
                                   .Format = Rhi::PixelFormat::D32FLOAT,
                                   .Usage = Rhi::TextureUsage::DepthStencilAttachment | Rhi::TextureUsage::Sampled};

        Output output{};

        rg.AddPass(
            GetName(),
            [&](RGBuilder& builder) {
                // 创建资源
                output.GBufferA = builder.CreateTexture("GBufferA", descA);
                output.GBufferB = builder.CreateTexture("GBufferB", descB);
                output.GBufferC = builder.CreateTexture("GBufferC", descC);
                output.Depth = builder.CreateTexture("SceneDepth", descDepth);

                // 声明 MRT (Multiple Render Targets) 写入
                builder.SetColorRT(output.GBufferA, 0, Rhi::LoadOp::Clear);
                builder.SetColorRT(output.GBufferB, 1, Rhi::LoadOp::Clear);
                builder.SetColorRT(output.GBufferC, 2, Rhi::LoadOp::Clear);
                builder.SetDepthRT(output.Depth, Rhi::LoadOp::Clear, Rhi::StoreOp::Store);
            },
            [this, input](RGContext& ctx) {
                auto cmd = ctx.Cmd;

                // 设置视口
                cmd->SetViewport(0, 0, width, height);
                cmd->SetScissor(0, 0, width, height);

                cmd->SetPipelineState(pipeline_.get());

                // 构建 ViewProj 矩阵
                glm::mat4 viewProj = glm::mat4(1.0f);
                if (input.SceneData && input.SceneData->GetMainCamera())
                {
                    viewProj = input.SceneData->GetMainCamera()->GetViewProjectionMatrix();
                }

                if (input.SceneData)
                {
                    for (const auto& obj : input.SceneData->GetObjects())
                    {
                        if (!obj->GetModel())
                        {
                            continue;
                        }

                        const Model& model = *obj->GetModel();
                        cmd->SetVertexBuffer(model.GetVertexBuffer());
                        cmd->SetIndexBuffer(model.GetIndexBuffer());

                        glm::mat4 rootTransform = obj->GetTransform();

                        for (int nodeIdx : model.GetRootNodes())
                        {
                            DrawNode(cmd, model, model.GetNodes()[nodeIdx], rootTransform, viewProj);
                        }
                    }
                }
            });

        return output;
    }

    void GeometryPass::CreatePipeline()
    {
        auto vs = device_->CreateShader("Asset/Shader/Deferred/Geometry.Vert.spv", Rhi::ShaderStage::Vertex);
        auto ps = device_->CreateShader("Asset/Shader/Deferred/Geometry.Frag.spv", Rhi::ShaderStage::Fragment);

        Rhi::PipelineStateDesc pipelineDesc;
        pipelineDesc.VertexShader = vs.get();
        pipelineDesc.FragmentShader = ps.get();

        // [关键] MRT 格式必须匹配
        pipelineDesc.ColorFormats = {
            Rhi::PixelFormat::R8G8B8A8UNORM,     // GBuffer A
            Rhi::PixelFormat::R8G8B8A8UNORM,     // GBuffer B
            Rhi::PixelFormat::R8G8B8A8UNORM  // GBuffer C
        };
        pipelineDesc.DepthFormat = Rhi::PixelFormat::D32FLOAT;
        pipelineDesc.DepthTestEnabled = true;
        pipelineDesc.DepthWriteEnabled = true;
        pipelineDesc.DepthCompareOp = Rhi::CompareOp::Less;
        pipelineDesc.Culling = Rhi::CullMode::Back;
        pipelineDesc.PushConstantSize = sizeof(PushConstants);

        // 定义输入布局 (Pos + Color)
        Rhi::InputElement posElement;
        posElement.Binding = 0;
        posElement.Location = 0;
        posElement.Format = Rhi::VertexFormat::Float3;
        posElement.Offset = offsetof(Vertex, Pos);

        Rhi::InputElement colorElement;
        colorElement.Binding = 0;
        colorElement.Location = 1;
        colorElement.Format = Rhi::VertexFormat::Float3;
        colorElement.Offset = offsetof(Vertex, Normal);

        Rhi::InputElement uvElement;
        uvElement.Binding = 0;
        uvElement.Location = 2;                       // 对应 Shader 里的 location(2)
        uvElement.Format = Rhi::VertexFormat::Float2; // vec2
        uvElement.Offset = offsetof(Vertex, UV);      // offsetof(Vertex, uv) = 24

        Rhi::InputElement tangentElement;
        tangentElement.Binding = 0;
        tangentElement.Location = 3;                       // 对应 Shader 里的 location(2)
        tangentElement.Format = Rhi::VertexFormat::Float4; // vec2
        tangentElement.Offset = offsetof(Vertex, Tangent); // offsetof(Vertex, uv) = 24

        pipelineDesc.InputLayout = {posElement, colorElement, uvElement, tangentElement};

        // 绑定材质 Layout
        pipelineDesc.ResourceLayouts = {materialLayout_};

        pipeline_ = device_->CreatePipeline(pipelineDesc);
    }

    void GeometryPass::DrawNode(Rhi::CommandList* cmd, const Engine::Model& model, const Engine::Node& node, const glm::mat4& parentTransform,
                                const glm::mat4& viewProj)
    {
        glm::mat4 globalTransform = parentTransform * node.LocalTransform;

        if (node.MeshIndex >= 0)
        {
            // 1. Push Constant (MVP)
            PushConstants push;
            push.Model = globalTransform;
            push.MVP = viewProj * globalTransform;
            cmd->SetPushConstants(pipeline_.get(), Rhi::ShaderStage::Vertex | Rhi::ShaderStage::Fragment, &push, sizeof(push));

            const Mesh& mesh = model.GetMeshes()[node.MeshIndex];

            // 2. 绘制 SubMeshes
            for (const auto& subMesh : mesh.SubMeshes)
            {
                // 3. 绑定材质 Set
                if (subMesh.MaterialIndex >= 0 && subMesh.MaterialIndex < model.GetMaterials().size())
                {
                    const auto& mat = model.GetMaterials()[subMesh.MaterialIndex];
                    if (mat.DescriptorSet)
                    {
                        cmd->SetDescriptorSet(pipeline_.get(), 0, mat.DescriptorSet.get());
                    }
                }
                cmd->DrawIndexed(subMesh.IndexCount, 1, subMesh.FirstIndex, 0, 0);
            }
        }

        for (const auto* child : node.Children)
        {
            DrawNode(cmd, model, *child, globalTransform, viewProj);
        }
    }
} // namespace Engine::Render::Pass