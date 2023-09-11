#include "Extensions.h"
#include "RenderPipelineState.h"
#include "GraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

RenderPipelineState::RenderPipelineState(std::shared_ptr<GraphicsDevice> d, VkPipeline p, VkPipelineLayout l)
    : gdevice(d)
    , pipeline(p)
    , layout(l) {
}

RenderPipelineState::~RenderPipelineState() {
    vkDestroyPipeline(gdevice->device, pipeline, gdevice->allocationCallbacks());
    vkDestroyPipelineLayout(gdevice->device, layout, gdevice->allocationCallbacks());
}

std::shared_ptr<FV::GraphicsDevice> RenderPipelineState::device() const {
    return gdevice;
}

#endif //#if FVCORE_ENABLE_VULKAN
