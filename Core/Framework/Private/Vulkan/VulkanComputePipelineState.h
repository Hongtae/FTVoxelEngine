#pragma once
#include <memory>
#include "../../ComputePipeline.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>


namespace FV {
    class VulkanGraphicsDevice;
    class VulkanComputePipelineState : public ComputePipelineState {
    public:
        VulkanComputePipelineState(std::shared_ptr<VulkanGraphicsDevice>, VkPipeline, VkPipelineLayout);
        ~VulkanComputePipelineState();

        std::shared_ptr<GraphicsDevice> device() const override;

        std::shared_ptr<VulkanGraphicsDevice> gdevice;
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
