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

        void signal(uint64_t value);
        bool wait(uint64_t value, uint64_t timeout);
        uint64_t value() const;
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
