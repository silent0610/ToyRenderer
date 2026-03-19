module;
#include "Math/Glm.hpp" // 包含你的 GLM 包装头文件
#include "spdlog/spdlog.h"
module Engine.Renderer;
import Engine.Rhi.Definition;
namespace Engine
{

    Renderer::Renderer(Rhi::RhiDevice* device) : device_(device) // 保存指针
    {
        InitResources();
        renderGraph_ = std::make_unique<RenderGraph>(device_);
    }
    Renderer::~Renderer()
    {
        // 智能指针会自动释放资源
    }

    void Renderer::InitResources()
    {
        spdlog::info("Initializing Renderer Resources...");

        CreateDefaultResources();
        
        forwardPass_ = std::make_unique<Engine::Render::Pass::ForwardPass>();
        forwardPass_->Init(device_, defaultWhiteTexture_.get(),defaultMaterialLayout_.get());
        geometryPass_ = std::make_unique<Engine::Render::Pass::GeometryPass>();
        geometryPass_->Init(device_, defaultMaterialLayout_.get());
        lightingPass_ = std::make_unique<Engine::Render::Pass::LightingPass>();
        lightingPass_->Init(device_);
    }

    

    void Renderer::RenderFrame(Rhi::CommandList* cmd, const Scene& scene, Rhi::RhiTexture* backBuffer)
    {

        renderGraph_->Reset();

        //auto backBufferHandle = renderGraph_->ImportTexture("BackBuffer", backBuffer);
 
        //auto forwardOutput = forwardPass_->Setup(*renderGraph_, {&scene});
        auto geometryOutput = geometryPass_->Setup(*renderGraph_, {&scene});
        auto lightingOutput = lightingPass_->Setup(*renderGraph_, {geometryOutput});

        renderGraph_->Compile();
        renderGraph_->Execute(cmd);

        {
            auto srcTex = renderGraph_->GetRhiTexture(lightingOutput.SceneColor);
            cmd->BlitTexture(srcTex, backBuffer);
        }
    }
    void Renderer::CreateDefaultResources()
    {
        CreateDefaultTexture();
        CreateDefaultSampler();
        CreateDefaultMaterialLayout();
    }
    void Renderer::CreateDefaultSampler()
    {
        Rhi::SamplerDesc samplerDesc{};
        defaultSampler_ = device_->CreateSampler(samplerDesc);
    }
    void Renderer::CreateDefaultMaterialLayout()
    {
        Rhi::DescriptorSetLayoutDesc layoutDesc;
        Rhi::DescriptorBinding binding;
        binding.Binding = 0;
        binding.Type = Rhi::DescriptorType::CombinedImageSampler;
        binding.Stage = Rhi::ShaderStage::Vertex | Rhi::ShaderStage::Fragment;
        layoutDesc.Bindings.push_back(binding);
        binding.Binding = 1;
        layoutDesc.Bindings.push_back(binding);
        binding.Binding = 2;
        layoutDesc.Bindings.push_back(binding);
        defaultMaterialLayout_ = device_->CreateDescriptorSetLayout(layoutDesc);
    }
    void Renderer::CreateDefaultTexture()
    {
        Rhi::TextureDesc texDesc{};
        texDesc.Name = "DefaultWhite";
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.Format = Rhi::PixelFormat::R8G8B8A8UNORM;
        texDesc.Usage = Rhi::TextureUsage::Sampled | Rhi::TextureUsage::TransferDst;
        defaultWhiteTexture_ = device_->CreateTexture(texDesc);
        uint32_t whitePixel = 0xFFFFFFFF;

        device_->UploadTextureData(defaultWhiteTexture_.get(), &whitePixel, sizeof(uint32_t));
    }
    Rhi::DescriptorSetLayout* Renderer::GetStandardMaterialLayout()const
    {
        return defaultMaterialLayout_.get();
    }
    Rhi::Sampler* Renderer::GetStandardSampler() const
    {
        return defaultSampler_.get();
    }
    Rhi::RhiTexture* Renderer::GetDefaultWhiteTexture() const
    {
        return defaultWhiteTexture_.get();
    }
} // namespace Engine