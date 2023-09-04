#include "Semaphore.h"
#include "Extensions.h"
#include "GraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

Semaphore::Semaphore(std::shared_ptr<GraphicsDevice> dev, VkSemaphore s)
    : device(dev)
    , semaphore(s) {
}

Semaphore::~Semaphore() {
    vkDestroySemaphore(device->device, semaphore, device->allocationCallbacks());
}

AutoIncrementalTimelineSemaphore::AutoIncrementalTimelineSemaphore(std::shared_ptr<GraphicsDevice> dev, VkSemaphore s)
    : Semaphore(dev, s)
    , waitValue(0)
    , signalValue(0) {
}

uint64_t AutoIncrementalTimelineSemaphore::nextWaitValue() const {
    return waitValue.fetch_add(1, std::memory_order_relaxed);
}

uint64_t AutoIncrementalTimelineSemaphore::nextSignalValue() const {
    return signalValue.fetch_add(1, std::memory_order_relaxed);
}
#endif //#if FVCORE_ENABLE_VULKAN
