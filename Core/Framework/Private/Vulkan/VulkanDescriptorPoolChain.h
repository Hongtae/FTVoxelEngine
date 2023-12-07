#pragma once
#include <memory>
#include <vector>

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanDescriptorPool.h"

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanDescriptorPoolChain {
    public:
        struct AllocationInfo {
            VkDescriptorSet descriptorSet;
            std::shared_ptr<VulkanDescriptorPool> descriptorPool;
        };

        bool allocateDescriptorSet(VkDescriptorSetLayout, AllocationInfo&);

        size_t cleanup();

        VulkanDescriptorPoolChain(VulkanGraphicsDevice*, const VulkanDescriptorPoolID&);
        ~VulkanDescriptorPoolChain();

        VulkanGraphicsDevice* gdevice;
        const VulkanDescriptorPoolID poolID;

        size_t descriptorPoolCount() const { return descriptorPools.size(); }

        std::shared_ptr<VulkanDescriptorPool> addNewPool(VkDescriptorPoolCreateFlags flags = 0);

        std::vector<std::shared_ptr<VulkanDescriptorPool>> descriptorPools;
        uint32_t maxSets;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
