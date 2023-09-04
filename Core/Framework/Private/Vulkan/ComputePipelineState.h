#pragma once
#include <memory>
#include "../../ComputePipeline.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>


namespace FV::Vulkan {
    class GraphicsDevice;
    class ComputePipelineState : public FV::ComputePipelineState {
    public:
        ComputePipelineState(std::shared_ptr<GraphicsDevice>, VkPipeline, VkPipelineLayout);
        ~ComputePipelineState();

        std::shared_ptr<FV::GraphicsDevice> device() const override;

        std::shared_ptr<GraphicsDevice> gdevice;
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
