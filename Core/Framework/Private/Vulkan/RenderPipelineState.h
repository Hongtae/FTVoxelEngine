#pragma once
#include <memory>
#include "../../RenderPipeline.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV::Vulkan
{
	class GraphicsDevice;
	class RenderPipelineState : public FV::RenderPipelineState
	{
	public:
		RenderPipelineState(std::shared_ptr<GraphicsDevice>, VkPipeline, VkPipelineLayout, VkRenderPass);
		~RenderPipelineState();

		std::shared_ptr<FV::GraphicsDevice> device() const override;

		std::shared_ptr<GraphicsDevice> gdevice;
		VkPipeline pipeline;
		VkPipelineLayout layout;
		VkRenderPass renderPass;
	};
}
#endif //#if FVCORE_ENABLE_VULKAN
