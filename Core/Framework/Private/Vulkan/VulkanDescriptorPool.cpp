#include "VulkanExtensions.h"
#include "VulkanGraphicsDevice.h"
#include "VulkanDescriptorPool.h"
#include "VulkanShaderBindingSet.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanDescriptorPool::VulkanDescriptorPool(VulkanGraphicsDevice* dev, VkDescriptorPool p, const VkDescriptorPoolCreateInfo& ci, const VulkanDescriptorPoolID& pid)
    : gdevice(dev)
    , pool(p)
    , maxSets(ci.maxSets)
    , createFlags(ci.flags)
    , numAllocatedSets(0)
    , poolID(pid) {
    FVASSERT_DEBUG(pool);
}

VulkanDescriptorPool::~VulkanDescriptorPool() {
    vkDestroyDescriptorPool(gdevice->device, pool, gdevice->allocationCallbacks());
}

VkDescriptorSet VulkanDescriptorPool::allocateDescriptorSet(VkDescriptorSetLayout layout) {
    FVASSERT_DEBUG(layout != VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocateInfo.descriptorPool = pool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    VkResult err = vkAllocateDescriptorSets(gdevice->device, &allocateInfo, &descriptorSet);
    if (err == VK_SUCCESS) {
        FVASSERT_DEBUG(descriptorSet != VK_NULL_HANDLE);
        numAllocatedSets++;
    } else {
        //Log::error(std::format("vkAllocateDescriptorSets failed: {}", err));
    }

    return descriptorSet;
}

void VulkanDescriptorPool::releaseDescriptorSets(VkDescriptorSet* sets, uint32_t n) {
    FVASSERT_DEBUG(numAllocatedSets > 0);
    numAllocatedSets -= n;

    if (numAllocatedSets == 0) {
        VkResult err = vkResetDescriptorPool(gdevice->device, pool, 0);
        if (err != VK_SUCCESS) {
            Log::error(std::format("vkResetDescriptorPool failed: {}", err));
        }
    } else if (createFlags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) {
        VkResult err = vkFreeDescriptorSets(gdevice->device, pool, n, sets);
        FVASSERT_DEBUG(err == VK_SUCCESS);
        if (err != VK_SUCCESS) {
            Log::error(std::format("vkFreeDescriptorSets failed: {}", err));
        }
    }
}

#endif //#if FVCORE_ENABLE_VULKAN
