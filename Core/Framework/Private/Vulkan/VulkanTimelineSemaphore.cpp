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

#endif //#if FVCORE_ENABLE_VULKAN
