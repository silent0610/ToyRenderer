module;
#include <cstdint>
export module Engine.Rhi.Shader;

export namespace Engine::Rhi
{

    class RhiShader
    {
    public:
        virtual ~RhiShader() = default;

        // 获取入口点名称 (通常是 "main")
        virtual const char *GetEntryPoint() const = 0;
    };
}