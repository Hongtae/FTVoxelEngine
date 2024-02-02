#include "VulkanExtensions.h"
#include "VulkanImage.h"
#include "VulkanImageView.h"
#include "VulkanGraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanImageView::VulkanImageView(std::shared_ptr<VulkanImage> img, VkImageView view, std::shared_ptr<VulkanImageView> pv)
    : image(img)
    , imageView(view)
    , gdevice(img->gdevice)
    , parentView(pv)
    , signalSemaphore(VK_NULL_HANDLE)
    , waitSemaphore(VK_NULL_HANDLE) {
}

VulkanImageView::VulkanImageView(std::shared_ptr<VulkanGraphicsDevice> dev, VkImageView view)
    : image(nullptr)
    , imageView(view)
    , gdevice(dev)
    , parentView(nullptr)
    , signalSemaphore(VK_NULL_HANDLE)
    , waitSemaphore(VK_NULL_HANDLE) {
}

VulkanImageView::~VulkanImageView() {
    if (imageView)
        vkDestroyImageView(gdevice->device, imageView, gdevice->allocationCallbacks());
    if (signalSemaphore)
        vkDestroySemaphore(gdevice->device, signalSemaphore, gdevice->allocationCallbacks());
    if (waitSemaphore)
        vkDestroySemaphore(gdevice->device, waitSemaphore, gdevice->allocationCallbacks());
}

std::shared_ptr<GraphicsDevice> VulkanImageView::device() const {
    return gdevice;
}

std::shared_ptr<Texture> VulkanImageView::parent() const {
    return parentView;
}

std::shared_ptr<Texture> VulkanImageView::makeTextureView(PixelFormat pf) const {
    if (image) {
        return image->makeImageView(pf);
    }
    return nullptr;
}

#endif //#if FVCORE_ENABLE_VULKAN
