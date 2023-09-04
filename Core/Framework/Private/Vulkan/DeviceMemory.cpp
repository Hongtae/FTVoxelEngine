#include <algorithm>
#include "Extensions.h"
#include "DeviceMemory.h"
#include "GraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

DeviceMemory::DeviceMemory(std::shared_ptr<GraphicsDevice> dev, VkDeviceMemory mem, VkMemoryType t, VkDeviceSize s)
    : gdevice(dev)
    , memory(mem)
    , type(t)
    , length(s)
    , mapped(nullptr) {
    FVASSERT_DEBUG(memory);
    FVASSERT_DEBUG(length > 0);

    if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        VkDeviceSize offset = 0;
        VkDeviceSize size = VK_WHOLE_SIZE;

        VkResult err = vkMapMemory(gdevice->device, memory, offset, size, 0, &mapped);
        if (err != VK_SUCCESS) {
            Log::error(std::format("vkMapMemory failed: {}", err));
        }
    }
}

DeviceMemory::~DeviceMemory() {
    FVASSERT_DEBUG(memory != VK_NULL_HANDLE);

    if (mapped)
        vkUnmapMemory(gdevice->device, memory);

    vkFreeMemory(gdevice->device, memory, gdevice->allocationCallbacks());
}

bool DeviceMemory::invalidate(uint64_t offset, uint64_t size) {
    FVASSERT_DEBUG(memory != VK_NULL_HANDLE);

    if (mapped && (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
        if (offset < length) {
            VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
            range.memory = memory;
            range.offset = offset;
            if (size == VK_WHOLE_SIZE)
                range.size = size;
            else
                range.size = std::min(size, length - offset);
            VkResult err = vkInvalidateMappedMemoryRanges(gdevice->device, 1, &range);
            if (err == VK_SUCCESS) {
                return true;
            } else {
                Log::error(std::format("vkInvalidateMappedMemoryRanges failed: {}", err));
            }
        } else {
            Log::error("DeviceMemory::invalidate() failed: Out of range");
        }
    }
    return false;
}

bool DeviceMemory::flush(uint64_t offset, uint64_t size) {
    FVASSERT_DEBUG(memory != VK_NULL_HANDLE);

    if (mapped && (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
        if (offset < length) {
            VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
            range.memory = memory;
            range.offset = offset;
            if (size == VK_WHOLE_SIZE)
                range.size = size;
            else
                range.size = std::min(size, length - offset);
            VkResult err = vkFlushMappedMemoryRanges(gdevice->device, 1, &range);
            if (err == VK_SUCCESS) {
                return true;
            } else {
                Log::error(std::format("vkFlushMappedMemoryRanges failed: {}", err));
            }
        } else {
            Log::error("DeviceMemory::flush() failed: Out of range");
        }
    }
    return false;
}

#endif //#if FVCORE_ENABLE_VULKAN
