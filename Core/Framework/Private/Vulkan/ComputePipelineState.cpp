#include "Extensions.h"
#include "ComputePipelineState.h"
#include "GraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

ComputePipelineState::ComputePipelineState(std::shared_ptr<GraphicsDevice> d, VkPipeline p, VkPipelineLayout l)
	: gdevice(d)
	, pipeline(p)
	, layout(l)
{
}

ComputePipelineState::~ComputePipelineState()
{
	vkDestroyPipeline(gdevice->device, pipeline, gdevice->allocationCallbacks());
	vkDestroyPipelineLayout(gdevice->device, layout, gdevice->allocationCallbacks());
}

std::shared_ptr<FV::GraphicsDevice> ComputePipelineState::device() const
{
	return gdevice; 
}

#endif //#if FVCORE_ENABLE_VULKAN
