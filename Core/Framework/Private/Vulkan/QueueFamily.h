#pragma once
#include <mutex>
#include <vector>

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "CommandQueue.h"

namespace FV::Vulkan {
    class GraphicsDevice;
    class QueueFamily {
    public:
        QueueFamily(VkDevice device, uint32_t familyIndex, uint32_t queueCount, const VkQueueFamilyProperties& prop, bool supportPresentation);
        ~QueueFamily();

        std::shared_ptr<CommandQueue> makeCommandQueue(std::shared_ptr<GraphicsDevice>);
        void recycleQueue(VkQueue);

        const bool supportPresentation;
        const uint32_t familyIndex;
        const VkQueueFamilyProperties properties;

        std::mutex lock;
        std::vector<VkQueue> freeQueues;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
