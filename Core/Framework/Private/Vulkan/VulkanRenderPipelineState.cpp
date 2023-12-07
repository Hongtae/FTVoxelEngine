#include "VulkanExtensions.h"
#include "VulkanRenderPipelineState.h"
#include "VulkanGraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanRenderPipelineState::VulkanRenderPipelineState(std::shared_ptr<VulkanGraphicsDevice> d, VkPipeline p, VkPipelineLayout l)
    : gdevice(d)
    , pipeline(p)
    , layout(l) {
}

VulkanRenderPipelineState::~VulkanRenderPipelineState() {
    vkDestroyPipeline(gdevice->device, pipeline, gdevice->allocationCallbacks());
    vkDestroyPipelineLayout(gdevice->device, layout, gdevice->allocationCallbacks());
}

std::shared_ptr<GraphicsDevice> VulkanRenderPipelineState::device() const {
    return gdevice;
}

#endif //#if FVCORE_ENABLE_VULKAN
