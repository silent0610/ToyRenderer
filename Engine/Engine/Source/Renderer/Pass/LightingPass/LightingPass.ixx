module;
#include "Math/Glm.hpp"
export module Engine.Render.Pass.LightingPass;

import std;
import Engine.Rhi.Device;
import Engine.Rhi.CommandList;
import Engine.Rhi.Pipeline;
import Engine.Rhi.Descriptor;
import Engine.Rhi.Sampler;
import Engine.RenderGraph;
import Engine.Render.Pass;
// 导入 GeometryPass 模块以获取其 Output 结构定义
import Engine.Render.Pass.GeometryPass;

export namespace Engine::Render::Pass
{
    class LightingPass : public IPass
    {
    public:
        struct Input
        {
            // 接收 GeometryPass 的输出
            GeometryPass::Output GBuffer;
        };

        struct Output
        {
            RGResourceHandle SceneColor;
        };

        struct PushConstants
        {
            glm::vec4 CameraPos;
            glm::vec4 LightDir;
            glm::vec4 LightColor;
        };

        LightingPass() = default;
        ~LightingPass() override = default;

        void Init(Rhi::RhiDevice* device);

        std::string GetName() const override;

        Output Setup(RenderGraph& rg, const Input& input);

    private:
        void CreatePipeline();

        Rhi::RhiDevice* device_{nullptr};

        // 专门用于绑定 GBuffer 纹理的 Layout
        std::unique_ptr<Rhi::DescriptorSetLayout> gbufferLayout_{nullptr};
        std::unique_ptr<Rhi::Sampler> sampler_{nullptr};
        std::unique_ptr<Rhi::Pipeline> pipeline_{nullptr};
    };
} // namespace Engine::Render::Pass