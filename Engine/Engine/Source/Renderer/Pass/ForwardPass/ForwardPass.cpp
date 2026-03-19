module;
#include "spdlog/spdlog.h"
#include "Math/Glm.hpp"
module Engine.Render.Pass.ForwardPass;

import Engine.RenderGraph;
import Engine.Object.Model.Definition;

namespace Engine::Render::Pass
{

	void ForwardPass::Init(Rhi::RhiDevice* device, Rhi::RhiTexture* defaultTexture, Rhi::DescriptorSetLayout* setLayout)
	{
		device_ = device;
		defaultTexture_ = defaultTexture;

		CreatePipeline(setLayout);
        CreateSampler();


		spdlog::info("ForwardPass initialized.");
	}
	std::string ForwardPass::GetName() const
	{
		return "ForwardPass";
    }
    void ForwardPass::CreatePipeline(Rhi::DescriptorSetLayout *setLayout)
    {
        // 创建额外的描述符集布局，用于特别用途,目前没有用, 仅做展示
        {
            Rhi::DescriptorSetLayoutDesc layoutDesc;
            Rhi::DescriptorBinding binding;
            binding.Binding = 0;
            binding.Type = Rhi::DescriptorType::CombinedImageSampler;
            binding.Stage = Rhi::ShaderStage::Fragment;
            layoutDesc.Bindings.push_back(binding);
            setLayout_ = device_->CreateDescriptorSetLayout(layoutDesc);
        }
        auto vs = device_->CreateShader("Asset/Shader/triangle/triangle.vert.spv", Rhi::ShaderStage::Vertex);
        auto ps = device_->CreateShader("Asset/Shader/triangle/triangle.frag.spv", Rhi::ShaderStage::Fragment);

        Rhi::PipelineStateDesc pipelineDesc;
        pipelineDesc.Polygon = Rhi::PolygonMode::Fill;
        pipelineDesc.VertexShader = vs.get();
        pipelineDesc.FragmentShader = ps.get();
        pipelineDesc.ColorFormats = {Rhi::PixelFormat::B8G8R8A8UNORM};
        pipelineDesc.DepthFormat = Rhi::PixelFormat::Unknown;
        pipelineDesc.DepthTestEnabled = false;
        pipelineDesc.Culling = Rhi::CullMode::Front;
        pipelineDesc.PushConstantSize = sizeof(PushConstants);

        pipelineDesc.DepthFormat = Rhi::PixelFormat::D32FLOAT;
        pipelineDesc.DepthCompareOp = Rhi::CompareOp::Less;
        pipelineDesc.DepthTestEnabled = true;
        pipelineDesc.DepthWriteEnabled = true;

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
        pipelineDesc.ResourceLayouts = {setLayout,setLayout_.get()};
        pipeline_ = device_->CreatePipeline(pipelineDesc);
    }
	void ForwardPass::CreateSampler()
	{
        Rhi::SamplerDesc samplerDesc;
 
        sampler_ = device_->CreateSampler(samplerDesc);
    }
    void ForwardPass::DrawNode(Rhi::CommandList* cmd, const Engine::Model& model, const Node& node, const glm::mat4& parentTransform,
                            const glm::mat4& viewProj)
    {
        // 1. 矩阵传递：父节点变换 * 当前节点局部变换
        glm::mat4 globalTransform = parentTransform * node.LocalTransform;

        // 2. 绘制当前节点 (如果有网格引用)
        if (node.MeshIndex >= 0)
        {
            PushConstants push;
            push.Model = globalTransform;

            push.mvp = viewProj * globalTransform;

            // B. 发送 PushConstants (Per-Object 数据)
            cmd->SetPushConstants(pipeline_.get(), Rhi::ShaderStage::Vertex | Rhi::ShaderStage::Fragment, &push, sizeof(push));

            // C. 获取 Mesh 数据
            const Mesh& mesh = model.GetMeshes()[node.MeshIndex];

            // D. 遍历 SubMeshes (Primitives)
            for (const auto& subMesh : mesh.SubMeshes)
            {
                // E. 绑定材质 (Per-Material 资源)
                if (subMesh.MaterialIndex >= 0 && subMesh.MaterialIndex < model.GetMaterials().size())
                {
                    const auto& material = model.GetMaterials()[subMesh.MaterialIndex];
                    if (material.DescriptorSet)
                    {
                        cmd->SetDescriptorSet(pipeline_.get(), 0, material.DescriptorSet.get());
                    }
                }

                // F. 发出绘制命令
                cmd->DrawIndexed(subMesh.IndexCount, 1, subMesh.FirstIndex, 0, 0);
            }
        }

        // 3. 递归：继续画子节点
        for (const auto* child : node.Children)
        {
            DrawNode(cmd, model, *child, globalTransform, viewProj);
        }
    }
    ForwardPass::Output ForwardPass::Setup(RenderGraph& rg, const Input& input)
    {
        Rhi::TextureDesc colorRTDesc;
        colorRTDesc.Width = 800;
        colorRTDesc.Height = 600;
        colorRTDesc.Format = Rhi::PixelFormat::B8G8R8A8UNORM;
        colorRTDesc.Usage = Rhi::TextureUsage::ColorAttachment | Rhi::TextureUsage::TransferSrc;
        

        Rhi::TextureDesc depthDesc =
            Rhi::TextureDesc{.Width = 800, .Height = 600, .Format = Rhi::PixelFormat::D32FLOAT, .Usage = Rhi::TextureUsage::DepthStencilAttachment};

        Output output{};

        rg.AddPass(
            GetName(),
            [&](RGBuilder& builder) {
                output.SceneColor = builder.CreateTexture("OffscreenTex", colorRTDesc);
                output.SceneDepth = builder.CreateTexture("OffscreenDepth", depthDesc);
                // 这会自动在内部生成 RenderPassInfo
                builder.SetColorRT(output.SceneColor, 0, Rhi::LoadOp::Clear);
                // builder.Read(sourceTexHandle);
                builder.SetDepthRT(output.SceneDepth, Rhi::LoadOp::Clear, Rhi::StoreOp::DontCare);
            },
            [this, &input](RGContext& ctx) {
                auto cmd = ctx.Cmd;

                // 设置视口 (写死 800x600，或者从 Texture 获取)
                cmd->SetViewport(0, 0, 800, 600);
                cmd->SetScissor(0, 0, 800, 600);

                // 绑定管线
                cmd->SetPipelineState(pipeline_.get());

                float aspect = 800.0f / 600.0f;
                glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 2), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
                glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
                proj[1][1] *= -1; // 修正 GLM 的 Y 轴
                glm::mat4 viewProj = proj * view;

                for (const auto& obj : input.SceneData->GetObjects())
                {
                    if (!obj->GetModel())
                    {
                        continue; // 跳过空对象
                    }

                    const Model& model = *obj->GetModel();

                    cmd->SetVertexBuffer(model.GetVertexBuffer());
                    cmd->SetIndexBuffer(model.GetIndexBuffer());

                    glm::mat4 rootTransform = obj->GetTransform(); // 对象在世界的位置

                    for (int nodeIdx : model.GetRootNodes())
                    {
                        DrawNode(cmd, model, model.GetNodes()[nodeIdx], rootTransform, viewProj);
                    }
                }

                // 注意：不需要调用 cmd->EndRendering()，RenderGraph 会自动调用！
            });


        return output;
    }
  
}