module;
#include <vector>
#include <string>
#include <cstdint>

export module RhiPipelineDesc;

import RhiTypes;

export {
    enum class RhiShaderStage
    {
        Vertex,
        Fragment,
        Compute,
        Geometry,
        TessellationControl,
        TessellationEvaluation
    };


    enum class RhiPrimitiveTopology
    {
        TriangleList,
        TriangleStrip,
        LineList,
        LineStrip,
        PointList
    };

    enum class RhiCullMode
    {
        None,
        Front,
        Back,
        FrontAndBack
    };

    enum class RhiFrontFace
    {
        CounterClockwise,
        Clockwise
    };

    enum class RhiPolygonMode
    {
        Fill,
        Line,
        Point
    };

    enum class RhiCompareOp
    {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    struct RhiShaderDesc
    {
        RhiShaderStage stage = RhiShaderStage::Vertex;
        std::string filePath;
        std::string entryPoint = "main";
    };


    struct RhiVertexInputStateDesc
    {
        std::vector<RhiVertexBindingDesc> bindings;
        std::vector<RhiVertexAttributeDesc> attributes;
    };

    struct RhiInputAssemblyStateDesc
    {
        RhiPrimitiveTopology topology = RhiPrimitiveTopology::TriangleList;
        bool primitiveRestartEnable = false;
    };

    struct RhiRasterizationStateDesc
    {
        bool depthClampEnable = false;
        bool rasterizerDiscardEnable = false;
        RhiPolygonMode polygonMode = RhiPolygonMode::Fill;
        RhiCullMode cullMode = RhiCullMode::Back;
        RhiFrontFace frontFace = RhiFrontFace::CounterClockwise;
        bool depthBiasEnable = false;
        float depthBiasConstantFactor = 0.0f;
        float depthBiasClamp = 0.0f;
        float depthBiasSlopeFactor = 0.0f;
        float lineWidth = 1.0f;
    };

    struct RhiDepthStencilStateDesc
    {
        bool depthTestEnable = true;
        bool depthWriteEnable = true;
        RhiCompareOp depthCompareOp = RhiCompareOp::Less;
        bool stencilTestEnable = false;
        // Stencil operations can be added later if needed
    };

    struct RhiColorBlendAttachmentDesc
    {
        bool blendEnable = false;
        // Blend factors and operations can be added later if needed
    };

    struct RhiColorBlendStateDesc
    {
        std::vector<RhiColorBlendAttachmentDesc> attachments;
        bool logicOpEnable = false;
    };

    // Helper comparison functions
    inline bool CompareShaderDesc(const RhiShaderDesc& a, const RhiShaderDesc& b) {
        return a.stage == b.stage && 
               a.filePath == b.filePath && 
               a.entryPoint == b.entryPoint;
    }
    
    inline bool CompareVertexInputState(const RhiVertexInputStateDesc& a, const RhiVertexInputStateDesc& b) {
        if (a.bindings.size() != b.bindings.size() || a.attributes.size() != b.attributes.size()) {
            return false;
        }
        
        for (size_t i = 0; i < a.bindings.size(); ++i) {
            const auto& bindingA = a.bindings[i];
            const auto& bindingB = b.bindings[i];
            if (bindingA.binding != bindingB.binding ||
                bindingA.stride != bindingB.stride ||
                bindingA.inputRate != bindingB.inputRate) {
                return false;
            }
        }
        
        for (size_t i = 0; i < a.attributes.size(); ++i) {
            const auto& attrA = a.attributes[i];
            const auto& attrB = b.attributes[i];
            if (attrA.location != attrB.location ||
                attrA.binding != attrB.binding ||
                attrA.format != attrB.format ||
                attrA.offset != attrB.offset) {
                return false;
            }
        }
        
        return true;
    }

    struct RhiGraphicsPipelineDesc
    {
        std::vector<RhiShaderDesc> shaders;
        RhiVertexInputStateDesc vertexInput;
        RhiInputAssemblyStateDesc inputAssembly;
        RhiRasterizationStateDesc rasterization;
        RhiDepthStencilStateDesc depthStencil;
        RhiColorBlendStateDesc colorBlend;
        std::vector<void*> descriptorSetLayouts; // RhiDescriptorSetLayout* stored as void*
        
        // Equality operator for caching
        bool operator==(const RhiGraphicsPipelineDesc& other) const {
            // Compare shaders
            if (shaders.size() != other.shaders.size()) {
                return false;
            }
            for (size_t i = 0; i < shaders.size(); ++i) {
                if (!CompareShaderDesc(shaders[i], other.shaders[i])) {
                    return false;
                }
            }
            
            // Compare vertex input state
            if (!CompareVertexInputState(vertexInput, other.vertexInput)) {
                return false;
            }
            
            // Compare input assembly
            if (inputAssembly.topology != other.inputAssembly.topology ||
                inputAssembly.primitiveRestartEnable != other.inputAssembly.primitiveRestartEnable) {
                return false;
            }
            
            // Compare rasterization state
            if (rasterization.cullMode != other.rasterization.cullMode ||
                rasterization.frontFace != other.rasterization.frontFace ||
                rasterization.polygonMode != other.rasterization.polygonMode) {
                return false;
            }
            
            // Compare depth stencil state
            if (depthStencil.depthTestEnable != other.depthStencil.depthTestEnable ||
                depthStencil.depthWriteEnable != other.depthStencil.depthWriteEnable ||
                depthStencil.depthCompareOp != other.depthStencil.depthCompareOp) {
                return false;
            }
            
            // Compare descriptor set layouts
            if (descriptorSetLayouts.size() != other.descriptorSetLayouts.size()) {
                return false;
            }
            for (size_t i = 0; i < descriptorSetLayouts.size(); ++i) {
                if (descriptorSetLayouts[i] != other.descriptorSetLayouts[i]) {
                    return false;
                }
            }
            
            return true;
        }
    };

    struct RhiComputePipelineDesc
    {
        RhiShaderDesc computeShader;
        std::vector<void*> descriptorSetLayouts; // RhiDescriptorSetLayout* stored as void*
        
        // Equality operator for caching
        bool operator==(const RhiComputePipelineDesc& other) const {
            // Compare compute shader
            if (!CompareShaderDesc(computeShader, other.computeShader)) {
                return false;
            }
            
            // Compare descriptor set layouts
            if (descriptorSetLayouts.size() != other.descriptorSetLayouts.size()) {
                return false;
            }
            for (size_t i = 0; i < descriptorSetLayouts.size(); ++i) {
                if (descriptorSetLayouts[i] != other.descriptorSetLayouts[i]) {
                    return false;
                }
            }
            
            return true;
        }
    };
}