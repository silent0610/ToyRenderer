module;

export module RhiBuffer;
import RhiTypes;

// Abstract buffer interface - simple but functional
export class RhiBuffer
{
public:
    virtual ~RhiBuffer() = default;

    // Basic buffer operations
    virtual void* Map() = 0;
    virtual void Unmap() = 0;
    virtual void UpdateData(const void* data, size_t size, size_t offset = 0) = 0;
    
    // Properties
    virtual size_t GetSize() const = 0;
    virtual RhiBufferUsage GetUsage() const = 0;
    virtual void* GetNativeHandle() const = 0;
};