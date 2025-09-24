module;

export module RhiFactory;
import Core;
import RhiDevice;
import RhiTypes;

// RHI Factory for creating devices across different backends
export enum class RhiBackend
{
    Vulkan,
    D3D12,    // Future
    Metal     // Future
};

// Factory function to create RHI devices
export Core::UniquePtr<RhiDevice> CreateRhiDevice(RhiBackend backend, const RhiDeviceDesc& desc);