module;

module RhiFactory;
import VulkanDevice;
import Core;

Core::UniquePtr<RhiDevice> CreateRhiDevice(RhiBackend backend, const RhiDeviceDesc& desc)
{
    switch (backend)
    {
        case RhiBackend::Vulkan:
            try {
                return Core::MakeUnique<VulkanDevice>(desc);
            } catch (const std::exception& e) {
                std::cerr << "[RhiFactory] Failed to create Vulkan device: " << e.what() << std::endl;
                return nullptr;
            }
            
        case RhiBackend::D3D12:
        case RhiBackend::Metal:
            std::cerr << "[RhiFactory] Backend not implemented yet" << std::endl;
            return nullptr;
            
        default:
            std::cerr << "[RhiFactory] Unknown backend" << std::endl;
            return nullptr;
    }
}