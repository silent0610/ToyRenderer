module;
#include <cstdint>
export module Engine.Rhi.Pipeline;

import Engine.Rhi.Definition;
import Engine.Rhi.Shader;
import Engine.Rhi.Descriptor;
import std;


export namespace Engine::Rhi
{
    struct InputElement
    {
        std::string SemanticName;
        uint32_t Binding = 0;
        uint32_t Location = 0;
        VertexFormat Format = VertexFormat::Float3;
        uint32_t Offset = 0;
    };
    struct PipelineStateDesc
    {

        RhiShader* VertexShader{};
        RhiShader* GeometryShader{};
        RhiShader* FragmentShader{};
        std::vector<InputElement> InputLayout{};

        CullMode Culling{CullMode::Back};
        
        PolygonMode Polygon = PolygonMode::Fill;
        float LineWidth = 1.0f;

        bool DepthTestEnabled = true;
        bool DepthWriteEnabled = true;
        CompareOp DepthCompareOp = CompareOp::Less;

        BlendMode Blend = BlendMode::Opaque;

        std::vector<PixelFormat> ColorFormats{};
        PixelFormat DepthFormat = PixelFormat::Unknown;

        uint32_t PushConstantSize{};
        ShaderStage PushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
        std::vector<DescriptorSetLayout*> ResourceLayouts;
    };
    struct ComputePipelineDesc
    {
        uint32_t PushConstantSize{};
        RhiShader* ComputeShader = nullptr;
    };
    class Pipeline
    {
    public:
        virtual ~Pipeline() = default;
        enum class Type
        {
            Graphics,
            Compute
        };
        virtual Type GetType() const = 0;
        // Pipeline 一旦创建就是不可变的，所以没有 Set 接口
    };

} // namespace Engine::Rhi