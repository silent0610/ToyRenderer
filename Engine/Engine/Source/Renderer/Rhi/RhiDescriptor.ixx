module;
#include <cstdint>
export module Engine.Rhi.Descriptor;

import Engine.Rhi.Definition;
import Engine.Rhi.Buffer;
import Engine.Rhi.Texture;
import Engine.Rhi.Sampler;
import std;

export namespace Engine::Rhi
{
    // 描述符集布局 (Layout)
    // 对应 Vulkan 的 VkDescriptorSetLayout
    // 它是一个“模板”，定义了 Shader 需要什么资源
    class DescriptorSetLayout
    {
    public:
        virtual ~DescriptorSetLayout() = default;
        virtual void *GetNativeHandle() const = 0;
    };

    // 描述符集 (Set)
    // 对应 Vulkan 的 VkDescriptorSet
    // 它是 Layout 的一个“实例”，真正绑定了 buffer 和 texture
    class RhiDescriptorSet
    {
    public:
        virtual ~RhiDescriptorSet() = default;
        virtual void *GetNativeHandle() const = 0;

        // --- 核心接口：更新绑定 ---

        // 绑定 Uniform Buffer
        virtual void UpdateBuffer(uint32_t binding, const RhiBuffer* buffer, DescriptorType type,uint64_t offset = 0, uint64_t range = 0) = 0;

        // 绑定纹理 (需要 View 和 Sampler)
        virtual void UpdateTexture(uint32_t binding, const RhiTexture *texture, const Sampler *sampler) = 0;
    };
}