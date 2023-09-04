#pragma once
#include <memory>
#include "../../DepthStencil.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV::Vulkan {
    class GraphicsDevice;
    class DepthStencilState : public FV::DepthStencilState {
    public:
        DepthStencilState(std::shared_ptr<GraphicsDevice>);
        ~DepthStencilState();

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

        std::shared_ptr<FV::GraphicsDevice> device() const override;
        std::shared_ptr<GraphicsDevice> gdevice;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
