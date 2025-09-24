module;

export module VulkanFactory;
import Core;
import RhiDevice;
import RhiTypes;
import RhiFactory;

// Legacy factory function for creating Vulkan RHI devices
// Now uses the new RhiFactory system
export namespace Rhi 
{
    Core::UniquePtr<RhiDevice> CreateVulkanDevice(void* window)
    {
        RhiDeviceDesc desc;
        desc.windowHandle = window;
        desc.enableValidation = true;
        desc.applicationName = "MyToyRenderer";
        
        return CreateRhiDevice(RhiBackend::Vulkan, desc);
    }
}