#include "../../Logger.h"
#include "VulkanExtensions.h"
#include "VulkanQueueFamily.h"
#include "VulkanCommandQueue.h"
#include "VulkanGraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanQueueFamily::VulkanQueueFamily(VkDevice device, uint32_t fIndex, uint32_t count, const VkQueueFamilyProperties& prop, bool presentationSupport)
    : familyIndex(fIndex)
    , supportPresentation(presentationSupport)
    , properties(prop) {
    freeQueues.reserve(count);
    for (uint32_t queueIndex = 0; queueIndex < count; ++queueIndex) {
        VkQueue queue = nullptr;
        vkGetDeviceQueue(device, familyIndex, queueIndex, &queue);
        if (queue)
            freeQueues.push_back(queue);
    }
    freeQueues.shrink_to_fit();
}

VulkanQueueFamily::~VulkanQueueFamily() {
}

std::shared_ptr<VulkanCommandQueue> VulkanQueueFamily::makeCommandQueue(std::shared_ptr<VulkanGraphicsDevice> dev) {
    std::scoped_lock guard(lock);
    if (freeQueues.empty() == false) {
        VkQueue queue = freeQueues.back();
        freeQueues.pop_back();
        auto commandQueue = std::make_shared<VulkanCommandQueue>(dev, this, queue);
        Log::info(std::format(
            "Command-Queue with family-index: {:d} has been created.",
            this->familyIndex));
        return commandQueue;
    }
    return nullptr;
}

void VulkanQueueFamily::recycleQueue(VkQueue queue) {
    std::scoped_lock guard(lock);
    Log::info(std::format(
        "Command-Queue with family-index: {:d} was reclaimed for recycling.",
        this->familyIndex));
    freeQueues.push_back(queue);
}

#endif //#if FVCORE_ENABLE_VULKAN
