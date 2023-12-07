#include "VulkanSemaphore.h"
#include "VulkanExtensions.h"
#include "VulkanGraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanSemaphore::VulkanSemaphore(std::shared_ptr<VulkanGraphicsDevice> dev, VkSemaphore s)
    : device(dev)
    , semaphore(s) {
}

VulkanSemaphore::~VulkanSemaphore() {
    vkDestroySemaphore(device->device, semaphore, device->allocationCallbacks());
}

VulkanSemaphoreAutoIncrementalTimeline::VulkanSemaphoreAutoIncrementalTimeline(std::shared_ptr<VulkanGraphicsDevice> dev, VkSemaphore s)
    : VulkanSemaphore(dev, s)
    , waitValue(0)
    , signalValue(0) {
}

uint64_t VulkanSemaphoreAutoIncrementalTimeline::nextWaitValue() const {
    return waitValue.fetch_add(1, std::memory_order_relaxed);
}

uint64_t VulkanSemaphoreAutoIncrementalTimeline::nextSignalValue() const {
    return signalValue.fetch_add(1, std::memory_order_relaxed);
}
#endif //#if FVCORE_ENABLE_VULKAN
