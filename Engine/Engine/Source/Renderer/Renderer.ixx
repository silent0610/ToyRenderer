module;
#include "math/Glm.hpp"
export module Engine.Renderer;
import Engine.Rhi.Device;
import Engine.Rhi.CommandList;
import Engine.Rhi.Texture;
import Engine.Rhi.Pipeline;
import Engine.Rhi.Buffer;
import Engine.Scene;
import Engine.RenderGraph;
import Engine.Rhi.Definition;
import Engine.Rhi.Sampler;
import Engine.Rhi.Descriptor;
import Engine.Object.Model;
import Engine.Render.Pass.ForwardPass;
import Engine.Render.Pass.GeometryPass;
import Engine.Render.Pass.LightingPass;
import std;
export namespace Engine
{
    class Renderer
    {
    public:
        Renderer(Rhi::RhiDevice *device);
        ~Renderer();
        void RenderFrame(Rhi::CommandList *cmd, const Scene &scene, Rhi::RhiTexture *backBuffer);
        Rhi::DescriptorSetLayout* GetStandardMaterialLayout()const;
        Rhi::Sampler* GetStandardSampler()const;
        Rhi::RhiTexture* GetDefaultWhiteTexture() const;
    private:
        // 内部初始化函数
        void InitResources();

    private:

        void CreateDefaultTexture();
        void CreateDefaultSampler();
        void CreateDefaultMaterialLayout();
        void CreateDefaultResources();
        Rhi::RhiDevice *device_; 

        std::unique_ptr<RenderGraph> renderGraph_;
        std::unique_ptr<Rhi::DescriptorSetLayout> defaultMaterialLayout_{};
        std::unique_ptr<Rhi::Sampler> defaultSampler_{};
        std::shared_ptr<Rhi::RhiTexture> defaultWhiteTexture_;

        std::unique_ptr<Engine::Render::Pass::ForwardPass> forwardPass_{};
        std::unique_ptr<Engine::Render::Pass::GeometryPass> geometryPass_{};
        std::unique_ptr<Engine::Render::Pass::LightingPass> lightingPass_{};
    };

}