module;
#include <cstdint>
export module Engine.Render.Pass;

import std;
import Engine.Rhi.Device;
import Engine.RenderGraph;

export namespace Engine::Render
{
    class IPass
    {
    public:
        virtual ~IPass() = default;
        virtual void Init(Rhi::RhiDevice* device) {};

        virtual void OnResize(uint32_t width, uint32_t height) {};

        virtual std::string GetName() const = 0;
    };
}