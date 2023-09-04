#include "Extensions.h"
#include "Sampler.h"
#include "GraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

Sampler::Sampler(std::shared_ptr<GraphicsDevice> dev, VkSampler s)
    : gdevice(dev)
    , sampler(s) {
}

Sampler::~Sampler() {
    vkDestroySampler(gdevice->device, sampler, gdevice->allocationCallbacks());
}

std::shared_ptr<FV::GraphicsDevice> Sampler::device() const {
    return gdevice;
}

#endif //#if FVCORE_ENABLE_VULKAN
