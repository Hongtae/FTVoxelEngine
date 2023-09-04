#include "Extensions.h"
#include "GraphicsDevice.h"
#include "DescriptorPool.h"
#include "DescriptorPoolChain.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

DescriptorPoolChain::DescriptorPoolChain(GraphicsDevice* dev, const DescriptorPoolID& pid)
    : gdevice(dev)
    , poolID(pid)
    , maxSets(0) {
    FVASSERT_DEBUG(gdevice);
    FVASSERT_DEBUG(poolID.mask);
}

DescriptorPoolChain::~DescriptorPoolChain() {

}

bool DescriptorPoolChain::allocateDescriptorSet(VkDescriptorSetLayout layout, AllocationInfo& info) {
    FVASSERT_DEBUG(gdevice);

    for (auto it = descriptorPools.begin(); it != descriptorPools.end(); ++it) {
        std::shared_ptr<DescriptorPool> pool = *it;
        VkDescriptorSet ds = pool->allocateDescriptorSet(layout);
        if (ds != VK_NULL_HANDLE) {
            info.descriptorSet = ds;
            info.descriptorPool = pool;

            if (it != descriptorPools.begin()) {
                // bring pool to front.
                descriptorPools.erase(it);
                descriptorPools.insert(descriptorPools.begin(), pool);
            }
            return true;
        }
    }
    std::shared_ptr<DescriptorPool> pool = addNewPool(0);
    if (pool) {
        VkDescriptorSet ds = pool->allocateDescriptorSet(layout);
        if (ds != VK_NULL_HANDLE) {
            info.descriptorSet = ds;
            info.descriptorPool = pool;
            return true;
        }
    }
    return false;
}

std::shared_ptr<DescriptorPool> DescriptorPoolChain::addNewPool(VkDescriptorPoolCreateFlags flags) {
    FVASSERT_DEBUG(gdevice);

    maxSets = maxSets * 2 + 1;

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(numDescriptorTypes);
    for (uint32_t i = 0; i < numDescriptorTypes; ++i) {
        if (poolID.typeSize[i] > 0) {
            VkDescriptorType type = descriptorTypeAtIndex(i);
            VkDescriptorPoolSize poolSize = {
                type,
                poolID.typeSize[i] * maxSets
            };
            poolSizes.push_back(poolSize);
        }
    }

    VkDescriptorPoolCreateInfo ci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    ci.flags = flags;
    ci.poolSizeCount = (uint32_t)poolSizes.size();
    ci.pPoolSizes = poolSizes.data();
    ci.maxSets = maxSets;

    // setup.
    FVASSERT_DEBUG(ci.maxSets);
    FVASSERT_DEBUG(ci.poolSizeCount);
    FVASSERT_DEBUG(ci.pPoolSizes);

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult err = vkCreateDescriptorPool(gdevice->device,
                                          &ci,
                                          gdevice->allocationCallbacks(),
                                          &pool);
    if (err != VK_SUCCESS) {
        Log::error(std::format("vkCreateDescriptorPool failed: {}", err));
        return nullptr;
    }
    FVASSERT_DEBUG(pool);

    std::shared_ptr<DescriptorPool> dp = std::make_shared<DescriptorPool>(gdevice, pool, ci, poolID);
    descriptorPools.insert(descriptorPools.begin(), dp);
    return dp;
}

size_t DescriptorPoolChain::cleanup() {
    std::vector<std::shared_ptr<DescriptorPool>> poolCopy = std::move(descriptorPools);

    std::vector<std::shared_ptr<DescriptorPool>> emptyPools;
    emptyPools.reserve(poolCopy.size());
    for (auto& pool : poolCopy) {
        if (pool->numAllocatedSets)
            descriptorPools.push_back(pool);
        else
            emptyPools.push_back(pool);
    }
    if (emptyPools.size() > 1) {
        std::sort(emptyPools.begin(), emptyPools.end(),
                  [](auto& lhs, auto& rhs)->bool {
                      return lhs->maxSets > rhs->maxSets;
                  });
    }
    if (emptyPools.size() > 0 && descriptorPools.size())
        descriptorPools.push_back(emptyPools.front()); // add first (biggest) pool for reuse.

    return descriptorPools.size();
}

#endif //#if FVCORE_ENABLE_VULKAN
