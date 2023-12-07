#include "VulkanExtensions.h"
#include "VulkanComputePipelineState.h"
#include "VulkanGraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanComputePipelineState::VulkanComputePipelineState(std::shared_ptr<VulkanGraphicsDevice> d, VkPipeline p, VkPipelineLayout l)
    : gdevice(d)
    , pipeline(p)
    , layout(l) {
}

VulkanComputePipelineState::~VulkanComputePipelineState() {
    vkDestroyPipeline(gdevice->device, pipeline, gdevice->allocationCallbacks());
    vkDestroyPipelineLayout(gdevice->device, layout, gdevice->allocationCallbacks());
}

std::shared_ptr<GraphicsDevice> VulkanComputePipelineState::device() const {
    return gdevice;
}

#endif //#if FVCORE_ENABLE_VULKAN
