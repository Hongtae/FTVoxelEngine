#include "Extensions.h"
#include "GraphicsDevice.h"
#include "DescriptorPool.h"
#include "ShaderBindingSet.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

DescriptorPool::DescriptorPool(GraphicsDevice* dev, VkDescriptorPool p, const VkDescriptorPoolCreateInfo& ci, const DescriptorPoolID& pid)
    : gdevice(dev)
    , pool(p)
    , maxSets(ci.maxSets)
    , createFlags(ci.flags)
    , numAllocatedSets(0)
    , poolID(pid)
{
    FVASSERT_DEBUG(pool);
}

DescriptorPool::~DescriptorPool()
{
    vkDestroyDescriptorPool(gdevice->device, pool, gdevice->allocationCallbacks());
}

VkDescriptorSet DescriptorPool::allocateDescriptorSet(VkDescriptorSetLayout layout)
{
    FVASSERT_DEBUG(layout != VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocateInfo.descriptorPool = pool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    VkResult err = vkAllocateDescriptorSets(gdevice->device, &allocateInfo, &descriptorSet);
    if (err == VK_SUCCESS)
    {
        FVASSERT_DEBUG(descriptorSet != VK_NULL_HANDLE);
        numAllocatedSets++;
    }
    else
    {
        //Log::error(std::format("vkAllocateDescriptorSets failed: {}",
        //                       getVkResultString(err)));
    }

    return descriptorSet;
}

void DescriptorPool::releaseDescriptorSets(VkDescriptorSet* sets, uint32_t n)
{
    FVASSERT_DEBUG(numAllocatedSets > 0);
    numAllocatedSets -= n;

    if (numAllocatedSets == 0)
    {
        VkResult err = vkResetDescriptorPool(gdevice->device, pool, 0);
        if (err != VK_SUCCESS)
        {
            Log::error(std::format("vkResetDescriptorPool failed: {}",
                getVkResultString(err)));
        }
    }
    else if (createFlags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
    {
        VkResult err = vkFreeDescriptorSets(gdevice->device, pool, n, sets);
        FVASSERT_DEBUG(err == VK_SUCCESS);
        if (err != VK_SUCCESS)
        {
            Log::error(std::format("vkFreeDescriptorSets failed: {}",
                getVkResultString(err)));
        }
    }
}

#endif //#if FVCORE_ENABLE_VULKAN
