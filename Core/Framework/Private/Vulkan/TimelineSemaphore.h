#pragma once
#include "../../GPUResource.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV::Vulkan {
    class GraphicsDevice;
    class TimelineSemaphore : public FV::GPUSemaphore {
    public:
        TimelineSemaphore(std::shared_ptr<GraphicsDevice>, VkSemaphore);
        ~TimelineSemaphore();

        std::shared_ptr<GraphicsDevice> device;
        VkSemaphore semaphore;
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
