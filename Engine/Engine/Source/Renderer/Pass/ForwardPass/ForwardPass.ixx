module;
#include "Math/Glm.hpp" // 包含你的 GLM 包装头文件
export module Engine.Render.Pass.ForwardPass;

import std;
import Engine.Rhi.Device;
import Engine.Rhi.CommandList;
import Engine.Rhi.Sampler;
import Engine.Rhi.Descriptor;
import Engine.Rhi.Texture;
import Engine.Rhi.Pipeline;
import Engine.RenderGraph;
import Engine.Scene;
import Engine.Object.Model;
import Engine.Render.Pass; // 导入基类模块

export namespace Engine::Render::Pass
{

    class ForwardPass : public IPass
    {
    public:
        struct Input
        {
            const Scene* SceneData;
        };
        struct Output
        {
            RGResourceHandle SceneColor;
            RGResourceHandle SceneDepth;
        };

        struct PushConstants
        {
            glm::mat4 Model;
            glm::mat4 mvp;
            glm::vec4 color;
        };


        ForwardPass() = default;
        ~ForwardPass() override = default;

        void Init(Rhi::RhiDevice* device, Rhi::RhiTexture* defaultTexture, Rhi::DescriptorSetLayout* setLayout);
        
        std::string GetName() const override;


        Output Setup(RenderGraph& rg, const Input& input);

    private:
        void CreatePipeline(Rhi::DescriptorSetLayout* setLayout);

        void CreateSampler();
        void DrawNode(Rhi::CommandList* cmd, const Engine::Model& model, const Engine::Node& node, const glm::mat4& parentTransform,
                      const glm::mat4& viewProj);


        Input input_{};
        Rhi::RhiDevice* device_{};
        Rhi::RhiTexture* defaultTexture_{};
        std::unique_ptr<Rhi::DescriptorSetLayout> setLayout_{};
        std::unique_ptr<Rhi::Sampler> sampler_{};
        std::unique_ptr<Rhi::RhiDescriptorSet> descriptorSet_{};
        std::unique_ptr<Rhi::Pipeline> pipeline_{};
       

    };
}