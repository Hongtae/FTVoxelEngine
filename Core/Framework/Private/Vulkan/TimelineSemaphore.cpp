#include "Extensions.h"
#include "TimelineSemaphore.h"
#include "GraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

TimelineSemaphore::TimelineSemaphore(std::shared_ptr<GraphicsDevice> dev, VkSemaphore s)
    : device(dev)
    , semaphore(s)
{
}

TimelineSemaphore::~TimelineSemaphore()
{
    vkDestroySemaphore(device->device, semaphore, device->allocationCallbacks());
}

#endif //#if FVCORE_ENABLE_VULKAN
