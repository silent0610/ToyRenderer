module;
#include <cstdint>

export module RhiRenderPass;
import RhiTypes;

// Abstract render pass interface
export class RhiRenderPass
{
public:
    virtual ~RhiRenderPass() = default;
    
    // Basic render pass operations
    virtual void Begin(uint32_t width, uint32_t height) = 0;
    virtual void End() = 0;
    
    // Properties
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual RhiFormat GetColorFormat() const = 0;
    virtual RhiFormat GetDepthFormat() const = 0;
};