#include "VulkanExtensions.h"
#include "VulkanSampler.h"
#include "VulkanGraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanSampler::VulkanSampler(std::shared_ptr<VulkanGraphicsDevice> dev, VkSampler s)
    : gdevice(dev)
    , sampler(s) {
}

VulkanSampler::~VulkanSampler() {
    vkDestroySampler(gdevice->device, sampler, gdevice->allocationCallbacks());
}

std::shared_ptr<GraphicsDevice> VulkanSampler::device() const {
    return gdevice;
}

#endif //#if FVCORE_ENABLE_VULKAN
