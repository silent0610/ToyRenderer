module;
#include "cstdint"
export module Engine.Rhi.Buffer;
import Engine.Rhi.Definition;
import std;
export namespace Engine::Rhi
{
    class RhiBuffer
    {
    public:
        virtual ~RhiBuffer() = default;

        virtual void *GetNativeHandle() const = 0;
        virtual const BufferDesc &GetDesc() const = 0;
        virtual uint64_t GetSize() const = 0;

        virtual void *Map() = 0;
        virtual void Unmap() = 0;

        virtual void WriteData(const void* data, uint64_t size, uint64_t offset = 0)
        {
            void* ptr = Map();
            if (ptr)
            {
                std::memcpy(static_cast<uint8_t*>(ptr) + offset, data, size);
                Unmap();
            }
        }
    };
}