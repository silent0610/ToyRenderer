module;
#include <vector>
#include <memory>
#include <queue>

export module CommandBufferPool;
import Core;
import RhiDevice;
import RhiCommandBuffer;
import RhiTypes;
import Logger;
import std;

// Forward declaration
export class PooledCommandBuffer;

// Pool manager for efficient CommandBuffer reuse
export class CommandBufferPool {
private:
    RhiDevice* device_;
    std::queue<Core::UniquePtr<RhiCommandBuffer>> availableBuffers_;
    std::vector<Core::UniquePtr<RhiCommandBuffer>> allBuffers_;
    
    // Statistics
    mutable size_t totalCreated_;
    mutable size_t currentlyInUse_;
    mutable size_t peakUsage_;
    
public:
    explicit CommandBufferPool(RhiDevice* device);
    ~CommandBufferPool() = default;
    
    // Non-copyable but movable
    CommandBufferPool(const CommandBufferPool&) = delete;
    CommandBufferPool& operator=(const CommandBufferPool&) = delete;
    CommandBufferPool(CommandBufferPool&&) = default;
    CommandBufferPool& operator=(CommandBufferPool&&) = default;
    
    // Pool operations
    Core::UniquePtr<RhiCommandBuffer> AcquireBuffer();
    PooledCommandBuffer AcquirePooledBuffer(); // RAII version
    void ReturnBuffer(Core::UniquePtr<RhiCommandBuffer> buffer);
    
    // Pool management
    void Preallocate(size_t count);
    void Shrink(); // Remove unused buffers to save memory
    void Clear();  // Return all buffers and clear pool
    
    // Statistics
    size_t GetTotalCreated() const { return totalCreated_; }
    size_t GetCurrentlyInUse() const { return currentlyInUse_; }
    size_t GetPeakUsage() const { return peakUsage_; }
    size_t GetAvailableCount() const { return availableBuffers_.size(); }
    
    void LogStatistics() const;

private:
    Core::UniquePtr<RhiCommandBuffer> CreateNewBuffer();
};