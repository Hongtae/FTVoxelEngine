#include "Extensions.h"
#include "Buffer.h"
#include "BufferView.h"
#include "GraphicsDevice.h"
#include "Types.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

Buffer::Buffer(std::shared_ptr<DeviceMemory> mem, VkBuffer b, const VkBufferCreateInfo& createInfo)
    : deviceMemory(mem)
    , gdevice(mem->gdevice)
	, buffer(b)
    , usage(createInfo.usage)
    , sharingMode(createInfo.sharingMode)
{
    FVASSERT_DEBUG(deviceMemory);
	FVASSERT_DEBUG(deviceMemory->length > 0);
}

Buffer::Buffer(std::shared_ptr<GraphicsDevice> dev, VkBuffer b)
    : deviceMemory(nullptr)
    , gdevice(dev)
    , buffer(b)
{
}

Buffer::~Buffer()
{
    if (buffer)
    {
        vkDestroyBuffer(gdevice->device, buffer, gdevice->allocationCallbacks());
    }
}

void* Buffer::contents()
{
    return deviceMemory->mapped;
}

void Buffer::flush(size_t offset, size_t size)
{
    size_t length = deviceMemory->length;
    if (offset < length)
    {
        if (size > 0)
            deviceMemory->flush(offset, size);
    }
    deviceMemory->invalidate(0, VK_WHOLE_SIZE);
}

size_t Buffer::length() const
{
    return deviceMemory->length;
}

std::shared_ptr<BufferView> Buffer::makeBufferView(PixelFormat pixelFormat, size_t offset, size_t range)
{
    if (usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT ||
        usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)
    {
        VkFormat format = getPixelFormat(pixelFormat);
        if (format != VK_FORMAT_UNDEFINED)
        {
            auto alignment = gdevice->properties().limits.minTexelBufferOffsetAlignment;
            FVASSERT_DEBUG(offset % alignment == 0);

            VkBufferViewCreateInfo bufferViewCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
            bufferViewCreateInfo.buffer = buffer;
            bufferViewCreateInfo.format = format;
            bufferViewCreateInfo.offset = offset;
            bufferViewCreateInfo.range = range;

            VkBufferView view = VK_NULL_HANDLE;
            VkResult err = vkCreateBufferView(gdevice->device, &bufferViewCreateInfo, gdevice->allocationCallbacks(), &view);
            if (err == VK_SUCCESS)
            {
                std::shared_ptr<BufferView> bufferView = std::make_shared<BufferView>(shared_from_this(), view, bufferViewCreateInfo);
                return bufferView;
            }
            else
            {
                Log::error(std::format("vkCreateBufferView failed: {}", getVkResultString(err)));
            }
        }
        else
        {
            Log::error("Buffer::CreateBufferView failed: Invalid pixel format!");
        }
    }
    else
    {
        Log::error("Buffer::CreateBufferView failed: Invalid buffer object (Not intended for texel buffer creation)");
    }
    return nullptr;
}

#endif //#if FVCORE_ENABLE_VULKAN
