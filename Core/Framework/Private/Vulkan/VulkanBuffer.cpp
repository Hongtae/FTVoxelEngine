#include "VulkanExtensions.h"
#include "VulkanBuffer.h"
#include "VulkanBufferView.h"
#include "VulkanGraphicsDevice.h"
#include "VulkanTypes.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanBuffer::VulkanBuffer(std::shared_ptr<VulkanDeviceMemory> mem, VkBuffer b, const VkBufferCreateInfo& createInfo)
    : deviceMemory(mem)
    , gdevice(mem->gdevice)
    , buffer(b)
    , usage(createInfo.usage)
    , sharingMode(createInfo.sharingMode)
    , size(createInfo.size) {
    FVASSERT_DEBUG(buffer);
    FVASSERT_DEBUG(deviceMemory);
    FVASSERT_DEBUG(deviceMemory->length >= size);
}

VulkanBuffer::VulkanBuffer(std::shared_ptr<VulkanGraphicsDevice> dev, VkBuffer b, VkDeviceSize s)
    : deviceMemory(nullptr)
    , gdevice(dev)
    , buffer(b)
    , size(s) {
    FVASSERT_DEBUG(buffer);
    FVASSERT_DEBUG(size > 0);
}

VulkanBuffer::~VulkanBuffer() {
    FVASSERT_DEBUG(buffer);
    vkDestroyBuffer(gdevice->device, buffer, gdevice->allocationCallbacks());
}

void* VulkanBuffer::contents() {
    if (deviceMemory)
        return deviceMemory->mapped;
    return nullptr;
}

void VulkanBuffer::flush(size_t offset, size_t size) {
    if (deviceMemory) {
        size_t length = deviceMemory->length;
        if (offset < length) {
            if (size > 0)
                deviceMemory->flush(offset, size);
        }
        deviceMemory->invalidate(0, VK_WHOLE_SIZE);
    }
}

size_t VulkanBuffer::length() const {
    return size;
}

std::shared_ptr<VulkanBufferView> VulkanBuffer::makeBufferView(PixelFormat pixelFormat, size_t offset, size_t range) {
    if (usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT ||
        usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) {
        VkFormat format = getVkFormat(pixelFormat);
        if (format != VK_FORMAT_UNDEFINED) {
            auto alignment = gdevice->properties().limits.minTexelBufferOffsetAlignment;
            FVASSERT_DEBUG(offset % alignment == 0);

            VkBufferViewCreateInfo bufferViewCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
            bufferViewCreateInfo.buffer = buffer;
            bufferViewCreateInfo.format = format;
            bufferViewCreateInfo.offset = offset;
            bufferViewCreateInfo.range = range;

            VkBufferView view = VK_NULL_HANDLE;
            VkResult err = vkCreateBufferView(gdevice->device, &bufferViewCreateInfo, gdevice->allocationCallbacks(), &view);
            if (err == VK_SUCCESS) {
                std::shared_ptr<VulkanBufferView> bufferView = std::make_shared<VulkanBufferView>(shared_from_this(), view, bufferViewCreateInfo);
                return bufferView;
            } else {
                Log::error(std::format("vkCreateBufferView failed: {}", err));
            }
        } else {
            Log::error("Buffer::CreateBufferView failed: Invalid pixel format!");
        }
    } else {
        Log::error("Buffer::CreateBufferView failed: Invalid buffer object (Not intended for texel buffer creation)");
    }
    return nullptr;
}

#endif //#if FVCORE_ENABLE_VULKAN
