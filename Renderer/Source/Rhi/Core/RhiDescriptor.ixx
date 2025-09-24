module;
#include <vector>
#include <cstdint>

export module RhiDescriptor;

import Core;
import RhiTypes;

export {
    enum class RhiDescriptorType
    {
        UniformBuffer,
        StorageBuffer,
        CombinedImageSampler,
        StorageImage
    };

    enum class RhiShaderStageFlags : uint32_t
    {
        None = 0,
        Vertex = 1 << 0,
        Fragment = 1 << 1,
        Compute = 1 << 2,
        All = Vertex | Fragment | Compute
    };

    inline RhiShaderStageFlags operator|(RhiShaderStageFlags a, RhiShaderStageFlags b)
    {
        return static_cast<RhiShaderStageFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    struct RhiDescriptorSetLayoutBinding
    {
        uint32_t binding = 0;
        RhiDescriptorType descriptorType = RhiDescriptorType::UniformBuffer;
        uint32_t descriptorCount = 1;
        RhiShaderStageFlags stageFlags = RhiShaderStageFlags::Vertex;
    };

    struct RhiDescriptorSetLayoutDesc
    {
        std::vector<RhiDescriptorSetLayoutBinding> bindings;
    };

    struct RhiDescriptorPoolSize
    {
        RhiDescriptorType type;
        uint32_t descriptorCount;
    };

    struct RhiDescriptorPoolDesc
    {
        uint32_t maxSets = 1;
        std::vector<RhiDescriptorPoolSize> poolSizes;
    };

    class RhiDescriptorSetLayout
    {
    public:
        virtual ~RhiDescriptorSetLayout() = default;
        virtual void* GetNativeHandle() const = 0;
    };

    class RhiDescriptorPool
    {
    public:
        virtual ~RhiDescriptorPool() = default;
        virtual void* GetNativeHandle() const = 0;
    };

    class RhiDescriptorSet
    {
    public:
        virtual ~RhiDescriptorSet() = default;
        virtual void* GetNativeHandle() const = 0;
    };
}