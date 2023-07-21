#include "Extensions.h"
#include "Buffer.h"
#include "BufferView.h"
#include "GraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

BufferView::BufferView(std::shared_ptr<Buffer> b)
    : buffer(b)
    , bufferView(VK_NULL_HANDLE)
    , gdevice(b->gdevice)
{
}

BufferView::BufferView(std::shared_ptr<Buffer> b, VkBufferView v, const VkBufferViewCreateInfo&)
    : buffer(b)
    , bufferView(v)
    , gdevice(b->gdevice)
{
}

BufferView::BufferView(std::shared_ptr<GraphicsDevice> dev, VkBufferView view)
    : buffer(nullptr)
    , bufferView(view)
    , gdevice(dev)
{
}

BufferView::~BufferView()
{
    if (bufferView != VK_NULL_HANDLE)
    {
        vkDestroyBufferView(gdevice->device, bufferView, gdevice->allocationCallbacks());
    }
}

#endif //#if FVCORE_ENABLE_VULKAN
