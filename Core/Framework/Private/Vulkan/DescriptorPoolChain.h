#pragma once
#include <memory>
#include <vector>

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "DescriptorPool.h"

namespace FV::Vulkan {
    class GraphicsDevice;
    class DescriptorPoolChain {
    public:
        struct AllocationInfo {
            VkDescriptorSet descriptorSet;
            std::shared_ptr<DescriptorPool> descriptorPool;
        };

        bool allocateDescriptorSet(VkDescriptorSetLayout, AllocationInfo&);

        size_t cleanup();

        DescriptorPoolChain(GraphicsDevice*, const DescriptorPoolID&);
        ~DescriptorPoolChain();

        GraphicsDevice* gdevice;
        const DescriptorPoolID poolID;

        size_t descriptorPoolCount() const { return descriptorPools.size(); }

        std::shared_ptr<DescriptorPool> addNewPool(VkDescriptorPoolCreateFlags flags = 0);

        std::vector<std::shared_ptr<DescriptorPool>> descriptorPools;
        uint32_t maxSets;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
