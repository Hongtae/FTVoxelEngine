#include "VulkanExtensions.h"
#include "VulkanBuffer.h"
#include "VulkanBufferView.h"
#include "VulkanGraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanBufferView::VulkanBufferView(std::shared_ptr<VulkanBuffer> b)
    : buffer(b)
    , bufferView(VK_NULL_HANDLE)
    , gdevice(b->gdevice) {
}

VulkanBufferView::VulkanBufferView(std::shared_ptr<VulkanBuffer> b, VkBufferView v, const VkBufferViewCreateInfo&)
    : buffer(b)
    , bufferView(v)
    , gdevice(b->gdevice) {
}

VulkanBufferView::VulkanBufferView(std::shared_ptr<VulkanGraphicsDevice> dev, VkBufferView view)
    : buffer(nullptr)
    , bufferView(view)
    , gdevice(dev) {
}

VulkanBufferView::~VulkanBufferView() {
    if (bufferView != VK_NULL_HANDLE) {
        vkDestroyBufferView(gdevice->device, bufferView, gdevice->allocationCallbacks());
    }
}

std::shared_ptr<GraphicsDevice> VulkanBufferView::device() const {
    return gdevice;
}

#endif //#if FVCORE_ENABLE_VULKAN
