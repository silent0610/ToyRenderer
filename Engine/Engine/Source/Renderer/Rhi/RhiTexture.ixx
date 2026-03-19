module;
#include <cstdint>
export module Engine.Rhi.Texture;
import Engine.Rhi.Definition;

export namespace Engine::Rhi
{
	class RhiTexture
	{
	public:
		virtual ~RhiTexture() = default;
        virtual const Engine::Rhi::TextureDesc& GetDesc() const = 0;
        uint32_t GetWidth() const
        {
            return GetDesc().Width;
        }
        uint32_t GetHeight() const
        {
            return GetDesc().Height;
        }
        PixelFormat GetFormat() const
        {
            return GetDesc().Format;
        }
        virtual void* GetNativeHandle() const = 0;
	};
} // namespace Engine::Rhi