module;

export module PooledCommandBuffer;
import std;
import Core;
import RhiCommandBuffer;
import CommandBufferPool;
import Logger;


// RAII wrapper for CommandBuffer that automatically returns to pool when destroyed
export class PooledCommandBuffer {
private:
    Core::UniquePtr<RhiCommandBuffer> commandBuffer_;
    CommandBufferPool* pool_;
    
public:
    PooledCommandBuffer(Core::UniquePtr<RhiCommandBuffer> commandBuffer, CommandBufferPool* pool)
        : commandBuffer_(std::move(commandBuffer)), pool_(pool) 
    {
        Log::Debug("PooledCommandBuffer created");
    }
    
    ~PooledCommandBuffer() {
        if (commandBuffer_ && pool_) {
            Log::Debug("PooledCommandBuffer destructor - returning buffer to pool");
            pool_->ReturnBuffer(std::move(commandBuffer_));
        }
    }
    
    // Move-only semantics
    PooledCommandBuffer(const PooledCommandBuffer&) = delete;
    PooledCommandBuffer& operator=(const PooledCommandBuffer&) = delete;
    
    PooledCommandBuffer(PooledCommandBuffer&& other) noexcept
        : commandBuffer_(std::move(other.commandBuffer_)), pool_(other.pool_) 
    {
        other.pool_ = nullptr;
    }
    
    PooledCommandBuffer& operator=(PooledCommandBuffer&& other) noexcept {
        if (this != &other) {
            // Return current buffer before taking new one
            if (commandBuffer_ && pool_) {
                pool_->ReturnBuffer(std::move(commandBuffer_));
            }
            
            commandBuffer_ = std::move(other.commandBuffer_);
            pool_ = other.pool_;
            other.pool_ = nullptr;
        }
        return *this;
    }
    
    // Access the underlying CommandBuffer
    RhiCommandBuffer* operator->() const { return commandBuffer_.get(); }
    RhiCommandBuffer& operator*() const { return *commandBuffer_; }
    RhiCommandBuffer* Get() const { return commandBuffer_.get(); }
    // Release ownership (caller takes responsibility for returning to pool)
    Core::UniquePtr<RhiCommandBuffer> Release() {
        pool_ = nullptr; // Don't return to pool in destructor
        return std::move(commandBuffer_);
    }
    // Check validity
    bool IsValid() const { return commandBuffer_ != nullptr; }

};