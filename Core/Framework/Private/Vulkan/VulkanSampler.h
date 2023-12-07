#pragma once
#include <memory>
#include "../../Sampler.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanSampler : public SamplerState {
    public:
        VulkanSampler(std::shared_ptr<VulkanGraphicsDevice>, VkSampler);
        ~VulkanSampler();

        std::shared_ptr<GraphicsDevice> device() const override;

        std::shared_ptr<VulkanGraphicsDevice> gdevice;
        VkSampler sampler;
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
