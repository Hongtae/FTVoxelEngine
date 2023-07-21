#include "Extensions.h"
#include "Image.h"
#include "ImageView.h"
#include "GraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

ImageView::ImageView(std::shared_ptr<Image> img, VkImageView view, const VkImageViewCreateInfo& ci)
    : image(img)
    , imageView(view)
    , gdevice(img->gdevice)
    , signalSemaphore(VK_NULL_HANDLE)
    , waitSemaphore(VK_NULL_HANDLE)
{
}

ImageView::ImageView(std::shared_ptr<GraphicsDevice> dev, VkImageView view)
    : image(nullptr)
    , imageView(view)
    , gdevice(dev)
    , signalSemaphore(VK_NULL_HANDLE)
    , waitSemaphore(VK_NULL_HANDLE)
{
}

ImageView::~ImageView()
{
    if (imageView)
        vkDestroyImageView(gdevice->device, imageView, gdevice->allocationCallbacks());
    if (signalSemaphore)
        vkDestroySemaphore(gdevice->device, signalSemaphore, gdevice->allocationCallbacks());
    if (waitSemaphore)
        vkDestroySemaphore(gdevice->device, waitSemaphore, gdevice->allocationCallbacks());
}

std::shared_ptr<FV::GraphicsDevice> ImageView::device() const
{
    return gdevice;
}
#endif //#if FVCORE_ENABLE_VULKAN
