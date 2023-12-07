#pragma once
#include "../../GPUResource.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanTimelineSemaphore : public GPUSemaphore {
    public:
        VulkanTimelineSemaphore(std::shared_ptr<VulkanGraphicsDevice>, VkSemaphore);
        ~VulkanTimelineSemaphore();

        std::shared_ptr<VulkanGraphicsDevice> device;
        VkSemaphore semaphore;
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
