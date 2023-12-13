#include "VulkanExtensions.h"
#include "VulkanBuffer.h"
#include "VulkanBufferView.h"
#include "VulkanGraphicsDevice.h"
#include "VulkanTypes.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanBuffer::VulkanBuffer(std::shared_ptr<VulkanGraphicsDevice> dev, const VulkanMemoryBlock& mem, VkBuffer b, const VkBufferCreateInfo& createInfo)
    : memory(mem)
    , gdevice(dev)
    , buffer(b)
    , usage(createInfo.usage)
    , sharingMode(createInfo.sharingMode)
    , size(createInfo.size) {
    FVASSERT_DEBUG(buffer);
}

VulkanBuffer::VulkanBuffer(std::shared_ptr<VulkanGraphicsDevice> dev, VkBuffer b, VkDeviceSize s)
    : memory{}
    , gdevice(dev)
    , buffer(b)
    , size(s) {
    FVASSERT_DEBUG(buffer);
    FVASSERT_DEBUG(size > 0);
}

VulkanBuffer::~VulkanBuffer() {
    FVASSERT_DEBUG(buffer);
    vkDestroyBuffer(gdevice->device, buffer, gdevice->allocationCallbacks());
    if (memory.has_value())
        memory.value().chunk->pool->dealloc(memory.value());
}

void* VulkanBuffer::contents() {
    if (memory.has_value()) {
        auto& mem = memory.value();
        auto chunk = mem.chunk;
        FVASSERT_DEBUG(chunk);
        uint8_t* mapped = reinterpret_cast<uint8_t*>(chunk->mapped);
        if (mapped) {
            return mapped + mem.offset;
        }
    }
    return nullptr;
}

void VulkanBuffer::flush(size_t offset, size_t size) {
    if (memory.has_value()) {
        auto& mem = memory.value();
        auto chunk = mem.chunk;
        FVASSERT_DEBUG(chunk);
        if (offset < mem.size) {
            size_t s = std::min(mem.size - offset, size);
            chunk->flush(mem.offset + offset, s);
        }
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
