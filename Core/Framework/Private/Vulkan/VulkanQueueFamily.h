#pragma once
#include <mutex>
#include <vector>

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanCommandQueue.h"

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanQueueFamily {
    public:
        VulkanQueueFamily(VkDevice device, uint32_t familyIndex, uint32_t queueCount, const VkQueueFamilyProperties& prop, bool supportPresentation);
        ~VulkanQueueFamily();

        std::shared_ptr<VulkanCommandQueue> makeCommandQueue(std::shared_ptr<VulkanGraphicsDevice>);
        void recycleQueue(VkQueue);

        const bool supportPresentation;
        const uint32_t familyIndex;
        const VkQueueFamilyProperties properties;

        std::mutex lock;
        std::vector<VkQueue> freeQueues;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
