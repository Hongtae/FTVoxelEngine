#pragma once
#include <memory>
#include "../../Sampler.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV::Vulkan
{
    class GraphicsDevice;
    class Sampler : public FV::SamplerState
    {
    public:
        Sampler(std::shared_ptr<GraphicsDevice>, VkSampler);
        ~Sampler();

        std::shared_ptr<GraphicsDevice> gdevice;
        VkSampler sampler;
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
