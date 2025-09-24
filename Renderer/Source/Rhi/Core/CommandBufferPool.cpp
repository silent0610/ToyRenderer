module;

module CommandBufferPool;
import PooledCommandBuffer;
import std;

CommandBufferPool::CommandBufferPool(RhiDevice* device)
    : device_(device), totalCreated_(0), currentlyInUse_(0), peakUsage_(0)
{
    Log::Debug("CommandBufferPool initialized");
}

Core::UniquePtr<RhiCommandBuffer> CommandBufferPool::AcquireBuffer() {
    Core::UniquePtr<RhiCommandBuffer> buffer;
    
    if (!availableBuffers_.empty()) {
        // Reuse existing buffer
        buffer = std::move(const_cast<Core::UniquePtr<RhiCommandBuffer>&>(availableBuffers_.front()));
        availableBuffers_.pop();
        
        // Reset the buffer for reuse
        RhiResult resetResult = buffer->Reset();
        if (resetResult != RhiResult::Success) {
            Log::Error("Failed to reset command buffer from pool");
            // Create a new buffer instead
            buffer = CreateNewBuffer();
        }
        
        Log::Debug("CommandBufferPool: Reused existing buffer");
    } else {
        // Create new buffer
        buffer = CreateNewBuffer();
        Log::Debug("CommandBufferPool: Created new buffer");
    }
    
    ++currentlyInUse_;
    peakUsage_ = std::max(peakUsage_, currentlyInUse_);
    
    Log::Debug(std::format("CommandBufferPool: Acquired buffer (in use: {}, available: {})", 
                          currentlyInUse_, availableBuffers_.size()));
    
    return buffer;
}

PooledCommandBuffer CommandBufferPool::AcquirePooledBuffer() {
    auto buffer = AcquireBuffer();
    return PooledCommandBuffer(std::move(buffer), this);
}

void CommandBufferPool::ReturnBuffer(Core::UniquePtr<RhiCommandBuffer> buffer) {
    if (!buffer) {
        Log::Warn("Attempted to return null buffer to pool");
        return;
    }
    
    // Reset buffer state for next use
    RhiResult resetResult = buffer->Reset();
    if (resetResult != RhiResult::Success) {
        Log::Warn("Failed to reset returned buffer - discarding");
        --currentlyInUse_;
        return;
    }
    
    availableBuffers_.push(std::move(buffer));
    --currentlyInUse_;
    
    Log::Debug(std::format("CommandBufferPool: Returned buffer (in use: {}, available: {})",
                          currentlyInUse_, availableBuffers_.size()));
}

void CommandBufferPool::Preallocate(size_t count) {
    Log::Info(std::format("CommandBufferPool: Preallocating {} buffers", count));
    
    for (size_t i = 0; i < count; ++i) {
        auto buffer = CreateNewBuffer();
        if (buffer) {
            availableBuffers_.push(std::move(buffer));
        }
    }
    
    Log::Info(std::format("CommandBufferPool: Preallocation complete (available: {})", 
                         availableBuffers_.size()));
}

void CommandBufferPool::Shrink() {
    size_t originalSize = availableBuffers_.size();
    
    // Keep at least 2 buffers available for double buffering
    while (availableBuffers_.size() > 2) {
        availableBuffers_.pop();
    }
    
    size_t removedCount = originalSize - availableBuffers_.size();
    if (removedCount > 0) {
        Log::Info(std::format("CommandBufferPool: Shrunk pool, removed {} unused buffers", removedCount));
    }
}

void CommandBufferPool::Clear() {
    while (!availableBuffers_.empty()) {
        availableBuffers_.pop();
    }
    allBuffers_.clear();
    
    totalCreated_ = 0;
    currentlyInUse_ = 0;
    peakUsage_ = 0;
    
    Log::Info("CommandBufferPool: Cleared all buffers");
}

void CommandBufferPool::LogStatistics() const {
    Log::Info(std::format("CommandBufferPool Statistics:"));
    Log::Info(std::format("  Total created: {}", totalCreated_));
    Log::Info(std::format("  Currently in use: {}", currentlyInUse_));
    Log::Info(std::format("  Available: {}", availableBuffers_.size()));
    Log::Info(std::format("  Peak usage: {}", peakUsage_));
    
    if (totalCreated_ > 0) {
        float reuseRatio = static_cast<float>(totalCreated_ - currentlyInUse_) / static_cast<float>(totalCreated_);
        Log::Info(std::format("  Reuse efficiency: {:.1f}%", reuseRatio * 100.0f));
    }
}

Core::UniquePtr<RhiCommandBuffer> CommandBufferPool::CreateNewBuffer() {
    auto buffer = device_->CreateCommandBuffer();
    if (!buffer) {
        Log::Error("Failed to create command buffer in pool");
        return nullptr;
    }
    
    ++totalCreated_;
    Log::Debug(std::format("CommandBufferPool: Created new buffer (total: {})", totalCreated_));
    
    return buffer;
}