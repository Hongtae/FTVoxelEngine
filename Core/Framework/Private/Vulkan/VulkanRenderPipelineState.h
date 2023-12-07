#pragma once
#include <memory>
#include "../../RenderPipeline.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanRenderPipelineState : public RenderPipelineState {
    public:
        VulkanRenderPipelineState(std::shared_ptr<VulkanGraphicsDevice>, VkPipeline, VkPipelineLayout);
        ~VulkanRenderPipelineState();

        std::shared_ptr<GraphicsDevice> device() const override;

        std::shared_ptr<VulkanGraphicsDevice> gdevice;
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
