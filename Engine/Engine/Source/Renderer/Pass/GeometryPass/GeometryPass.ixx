module;
#include "Math/Glm.hpp" // GLM 头文件
export module Engine.Render.Pass.GeometryPass;

import std;
import Engine.Rhi.Device;
import Engine.Rhi.CommandList;
import Engine.Rhi.Pipeline;
import Engine.Rhi.Descriptor;
import Engine.RenderGraph;
import Engine.Scene;
import Engine.Object.Model;
import Engine.Render.Pass; // 导入基类 IPass

export namespace Engine::Render::Pass
{
    class GeometryPass : public IPass
    {
    public:
        struct Input
        {
            const Scene* SceneData;
        };

        struct Output
        {
            RGResourceHandle GBufferA; // Albedo + Metallic
            RGResourceHandle GBufferB; // Normal + Roughness
            RGResourceHandle GBufferC; // Position
            RGResourceHandle Depth;    // Depth
        };

        struct PushConstants
        {
            glm::mat4 Model;
            glm::mat4 MVP;
        };

        GeometryPass() = default;
        ~GeometryPass() override = default;

        // Init 需要传入全局的标准材质布局 (Standard Material Layout)
        void Init(Rhi::RhiDevice* device, Rhi::DescriptorSetLayout* materialLayout);

        std::string GetName() const override;

        Output Setup(RenderGraph& rg, const Input& input);

    private:
        void CreatePipeline();

        void DrawNode(Rhi::CommandList* cmd, const Engine::Model& model, const Engine::Node& node, const glm::mat4& parentTransform,
                      const glm::mat4& viewProj);

        Rhi::RhiDevice* device_{nullptr};
        Rhi::DescriptorSetLayout* materialLayout_{nullptr}; // 引用，不持有
        std::unique_ptr<Rhi::Pipeline> pipeline_{nullptr};
    };
} // namespace Engine::Render::Pass