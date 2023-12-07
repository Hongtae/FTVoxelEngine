#pragma once
#include <memory>
#include "../../DepthStencil.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanDepthStencilState : public DepthStencilState {
    public:
        VulkanDepthStencilState(std::shared_ptr<VulkanGraphicsDevice>);
        ~VulkanDepthStencilState();

        VkBool32 depthTestEnable;
        VkBool32 depthWriteEnable;
        VkCompareOp depthCompareOp;
        VkBool32 depthBoundsTestEnable;
        float minDepthBounds;
        float maxDepthBounds;

        VkStencilOpState front;
        VkStencilOpState back;
        VkBool32 stencilTestEnable;

        void bind(VkCommandBuffer);

        std::shared_ptr<GraphicsDevice> device() const override;
        std::shared_ptr<VulkanGraphicsDevice> gdevice;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
