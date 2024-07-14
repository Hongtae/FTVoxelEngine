#include "VulkanExtensions.h"
#include "VulkanTimelineSemaphore.h"
#include "VulkanGraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanTimelineSemaphore::VulkanTimelineSemaphore(std::shared_ptr<VulkanGraphicsDevice> dev, VkSemaphore s)
    : device(dev)
    , semaphore(s) {
}

VulkanTimelineSemaphore::~VulkanTimelineSemaphore() {
    vkDestroySemaphore(device->device, semaphore, device->allocationCallbacks());
}

void VulkanTimelineSemaphore::signal(uint64_t value) {
    VkSemaphoreSignalInfo signalInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO };
    signalInfo.semaphore = semaphore;
    signalInfo.value = value;
    vkSignalSemaphore(device->device, &signalInfo);
}

bool VulkanTimelineSemaphore::wait(uint64_t value, uint64_t timeout) {
    VkSemaphoreWaitInfo waitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &semaphore;
    waitInfo.pValues = &value;
    return vkWaitSemaphores(device->device, &waitInfo, timeout) == VK_SUCCESS;
}

uint64_t VulkanTimelineSemaphore::value() const {
    uint64_t value = 0;
    vkGetSemaphoreCounterValue(device->device, semaphore, &value);
    return value;
}

#endif //#if FVCORE_ENABLE_VULKAN
