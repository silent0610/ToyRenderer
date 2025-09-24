module;

export module RhiPipeline;

// Simplified pipeline interface for basic rendering  
export class RhiPipeline
{
public:
    virtual ~RhiPipeline() = default;
    
    // For now, we'll keep pipelines as opaque objects
    // In a full implementation, this would have more methods for configuration
    
    // Virtual method to get platform-specific handles
    virtual void* GetNativeHandle() const = 0;
};