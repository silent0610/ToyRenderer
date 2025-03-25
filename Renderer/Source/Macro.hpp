#include "vulkan/vulkan.h"
import ToolMod;
#define VK_CHECK_RESULT(f)                                                                                                              \
    {                                                                                                                                   \
        VkResult res = (f);                                                                                                             \
        if (res != VK_SUCCESS)                                                                                                          \
        {                                                                                                                               \
            std::cout << "Fatal : VkResult is \"" << Tool::ErrorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << "\n"; \
            assert(res == VK_SUCCESS);                                                                                                  \
        }                                                                                                                               \
    }