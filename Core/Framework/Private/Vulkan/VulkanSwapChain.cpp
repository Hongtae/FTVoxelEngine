#include <cmath>
#include "VulkanExtensions.h"
#include "VulkanSwapChain.h"
#include "VulkanGraphicsDevice.h"
#include "VulkanImage.h"
#include "VulkanImageView.h"
#include "VulkanSemaphore.h"
#include "VulkanCopyCommandEncoder.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

constexpr int maxFrameSemaphores = 3;

VulkanSwapChain::VulkanSwapChain(std::shared_ptr<VulkanCommandQueue> q, std::shared_ptr<Window> w)
    : cqueue(q)
    , window(w)
    , surface(nullptr)
    , swapchain(nullptr)
    , enableVSync(false)
    , deviceReset(false)
    , frameReady(nullptr)
    , frameTimelineSemaphore(VK_NULL_HANDLE)
    , frameCount(0) {
    auto& gdevice = cqueue->gdevice;
    VkDevice device = gdevice->device;

    while (frameSemaphores.size() < maxFrameSemaphores) {
        // create semaphore
        VkSemaphore semaphore = VK_NULL_HANDLE;
        VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkResult err = vkCreateSemaphore(device, &semaphoreCreateInfo, gdevice->allocationCallbacks(), &semaphore);
        if (err != VK_SUCCESS) {
            Log::error("vkCreateSemaphore failed: {}", err);
            FVASSERT(0);
        }
        if (semaphore != VK_NULL_HANDLE) {
            frameSemaphores.push_back({ semaphore, 0 });
        }
    }
    FVASSERT(!frameSemaphores.empty());
    frameSemaphores.shrink_to_fit();

    // create timeline semaphore
    VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkSemaphoreTypeCreateInfo typeCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    typeCreateInfo.initialValue = 0;
    semaphoreCreateInfo.pNext = &typeCreateInfo;

    VkResult err = vkCreateSemaphore(device, &semaphoreCreateInfo, gdevice->allocationCallbacks(), &frameTimelineSemaphore);
    if (err != VK_SUCCESS) {
        Log::error("vkCreateSemaphore failed: {}", err);
        FVASSERT(0);
    }

    window->addEventObserver(this, (Window::WindowEventHandler)[this](auto param) {
        this->onWindowEvent(param);
    });
}

VulkanSwapChain::~VulkanSwapChain() {
    window->removeEventObserver(this);

    cqueue->waitIdle();

    auto& gdevice = cqueue->gdevice;
    auto& instance = gdevice->instance;
    VkDevice device = gdevice->device;

    for (auto& imageView : imageViews) {
        imageView->image->image = VK_NULL_HANDLE;
        imageView->image = nullptr;
        imageView->waitSemaphore = VK_NULL_HANDLE;
        imageView->signalSemaphore = VK_NULL_HANDLE;
        FVASSERT_DEBUG(imageView->imageView);
    }
    imageViews.clear();

    if (swapchain)
        vkDestroySwapchainKHR(device, swapchain, gdevice->allocationCallbacks());

    if (surface)
        vkDestroySurfaceKHR(instance->instance, surface, gdevice->allocationCallbacks());

    for (auto frame : frameSemaphores) {
        vkDestroySemaphore(device, frame.semaphore, gdevice->allocationCallbacks());
    }
    vkDestroySemaphore(device, frameTimelineSemaphore, gdevice->allocationCallbacks());
}

bool VulkanSwapChain::setup() {
    auto& gdevice = cqueue->gdevice;
    auto& instance = gdevice->instance;
    auto& physicalDevice = gdevice->physicalDevice;
    VkDevice device = gdevice->device;

    uint32_t queueFamilyIndex = cqueue->family->familyIndex;

    VkResult err = VK_SUCCESS;

    // create VkSurfaceKHR
#ifdef VK_USE_PLATFORM_ANDROID_KHR
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = { VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR };
    surfaceCreateInfo.window = (ANativeWindow*)window->platformHandle();
    err = vkCreateAndroidSurfaceKHR(instance, &surfaceCreateInfo, gdevice->allocationCallbacks, &surface);
    if (err != VK_SUCCESS) {
        Log::error("vkCreateAndroidSurfaceKHR failed: {}", err);
        return false;
    }
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    surfaceCreateInfo.hinstance = (HINSTANCE)GetModuleHandleW(NULL);
    surfaceCreateInfo.hwnd = (HWND)window->platformHandle();
    err = instance->extensionProc.vkCreateWin32SurfaceKHR(instance->instance, &surfaceCreateInfo, gdevice->allocationCallbacks(), &surface);
    if (err != VK_SUCCESS) {
        Log::error("vkCreateWin32SurfaceKHR failed: {}", err);
        return false;
    }
#endif

    VkBool32 surfaceSupported = VK_FALSE;
    err = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice.device, queueFamilyIndex, surface, &surfaceSupported);
    if (err != VK_SUCCESS) {
        Log::error("vkGetPhysicalDeviceSurfaceSupportKHR failed: {}", err);
        return false;
    }
    if (!surfaceSupported) {
        Log::error("VkSurfaceKHR not support with QueueFamily at index: {:d}", queueFamilyIndex);
        return false;
    }

    // get color format, color space
    // Get list of supported surface formats
    uint32_t surfaceFormatCount;
    err = instance->extensionProc.vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice.device, surface, &surfaceFormatCount, NULL);
    if (err != VK_SUCCESS) {
        Log::error("vkGetPhysicalDeviceSurfaceFormatsKHR failed: {}", err);
        return false;
    }
    if (surfaceFormatCount == 0) {
        Log::error("vkGetPhysicalDeviceSurfaceFormatsKHR failed: {}", err);
        return false;
    }

    availableSurfaceFormats.clear();
    availableSurfaceFormats.resize(surfaceFormatCount);
    err = instance->extensionProc.vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice.device, surface, &surfaceFormatCount, availableSurfaceFormats.data());
    if (err != VK_SUCCESS) {
        Log::error("vkGetPhysicalDeviceSurfaceFormatsKHR failed: {}", err);
        return false;
    }

    // If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
    // there is no preferered format, so we assume VK_FORMAT_B8G8R8A8_UNORM
    if ((surfaceFormatCount == 1) && (availableSurfaceFormats[0].format == VK_FORMAT_UNDEFINED)) {
        this->surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
    } else {
        // Always select the first available color format
        // If you need a specific format (e.g. SRGB) you'd need to
        // iterate over the list of available surface format and
        // check for it's presence
        this->surfaceFormat.format = availableSurfaceFormats[0].format;
    }
    this->surfaceFormat.colorSpace = availableSurfaceFormats[0].colorSpace;

    // create swapchain
    return this->updateDevice();
}

bool VulkanSwapChain::updateDevice() {
    auto& gdevice = cqueue->gdevice;
    auto& physicalDevice = gdevice->physicalDevice;
    VkDevice device = gdevice->device;

    Size resolution = this->window->resolution();
    uint32_t width = static_cast<uint32_t>(std::lround(resolution.width));
    uint32_t height = static_cast<uint32_t>(std::lround(resolution.height));

    VkResult err = VK_SUCCESS;
    VkSwapchainKHR swapchainOld = this->swapchain;

    // Get physical device surface properties and formats
    VkSurfaceCapabilitiesKHR surfaceCaps;
    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice.device, surface, &surfaceCaps);
    if (err != VK_SUCCESS) {
        Log::error("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: {}", err);
        return false;
    }

    // Get available present modes
    uint32_t presentModeCount;
    err = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice.device, surface, &presentModeCount, nullptr);
    if (err != VK_SUCCESS) {
        Log::error("vkGetPhysicalDeviceSurfacePresentModesKHR failed: {}", err);
        return false;
    }
    if (presentModeCount == 0) {
        Log::error("vkGetPhysicalDeviceSurfacePresentModesKHR failed: {}", err);
        return false;
    }

    std::vector<VkPresentModeKHR> presentModes;
    presentModes.resize(presentModeCount);

    err = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice.device, surface, &presentModeCount, presentModes.data());
    if (err != VK_SUCCESS) {
        Log::error("vkGetPhysicalDeviceSurfacePresentModesKHR failed: {}", err);
        return false;
    }

    VkExtent2D swapchainExtent = {};
    // If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
    if (surfaceCaps.currentExtent.width == (uint32_t)-1) {
        // If the surface size is undefined, the size is set to
        // the size of the images requested.
        swapchainExtent.width = width;
        swapchainExtent.height = height;
    } else {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = surfaceCaps.currentExtent;
        width = surfaceCaps.currentExtent.width;
        height = surfaceCaps.currentExtent.height;
    }

    // Select a present mode for the swapchain

    // VK_PRESENT_MODE_IMMEDIATE_KHR
    // VK_PRESENT_MODE_MAILBOX_KHR
    // VK_PRESENT_MODE_FIFO_KHR
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR

    // The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
    // This mode waits for the vertical blank ("v-sync")
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    // If v-sync is not requested, try to find a mailbox mode
    // It's the lowest latency non-tearing present mode available
    if (!this->enableVSync) {
        for (size_t i = 0; i < presentModeCount; i++) {
            if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
            if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)) {
                swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }

    // Determine the number of images
    uint32_t desiredNumberOfSwapchainImages = surfaceCaps.minImageCount + 1;
    if ((surfaceCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfaceCaps.maxImageCount)) {
        desiredNumberOfSwapchainImages = surfaceCaps.maxImageCount;
    }

    // Find the transformation of the surface
    VkSurfaceTransformFlagsKHR preTransform;
    if (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        // We prefer a non-rotated transform
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        preTransform = surfaceCaps.currentTransform;
    }

    VkSwapchainCreateInfoKHR swapchainCI = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchainCI.surface = surface;
    swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
    swapchainCI.imageFormat = this->surfaceFormat.format;
    swapchainCI.imageColorSpace = this->surfaceFormat.colorSpace;
    swapchainCI.imageExtent = { swapchainExtent.width, swapchainExtent.height };
    swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
    swapchainCI.imageArrayLayers = 1;
    swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCI.queueFamilyIndexCount = 0;
    swapchainCI.pQueueFamilyIndices = NULL;
    swapchainCI.presentMode = swapchainPresentMode;
    swapchainCI.oldSwapchain = swapchainOld;
    // Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
    swapchainCI.clipped = VK_TRUE;
    swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    // Set additional usage flag for blitting from the swapchain images if supported
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(physicalDevice.device, this->surfaceFormat.format, &formatProps);
    if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) {
        swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    err = vkCreateSwapchainKHR(device, &swapchainCI, gdevice->allocationCallbacks(), &this->swapchain);
    if (err != VK_SUCCESS) {
        Log::error("vkCreateSwapchainKHR failed: {}", err);
        return false;
    }

    Log::info("VkSwapchainKHR created. ({:d} x {:d}, V-sync: {}, {})",
              swapchainExtent.width, swapchainExtent.height,
              this->enableVSync,
              [](VkPresentModeKHR mode)->const char* {
                  switch (mode) {
                  case VK_PRESENT_MODE_IMMEDIATE_KHR:
                      return "VK_PRESENT_MODE_IMMEDIATE_KHR";
                  case VK_PRESENT_MODE_MAILBOX_KHR:
                      return "VK_PRESENT_MODE_MAILBOX_KHR";
                  case VK_PRESENT_MODE_FIFO_KHR:
                      return "VK_PRESENT_MODE_FIFO_KHR";
                  case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
                      return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
                  }
                  return "## UNKNOWN ##";
              }(swapchainPresentMode));

    // If an existing swap chain is re-created, destroy the old swap chain
    // This also cleans up all the presentable images
    if (swapchainOld) {
        vkDestroySwapchainKHR(device, swapchainOld, gdevice->allocationCallbacks());
    }

    for (auto& imageView : imageViews) {
        imageView->image->image = VK_NULL_HANDLE;
        imageView->image = nullptr;
        imageView->waitSemaphore = VK_NULL_HANDLE;
        imageView->signalSemaphore = VK_NULL_HANDLE;
        FVASSERT_DEBUG(imageView->imageView);
    }
    imageViews.clear();

    uint32_t swapchainImageCount = 0;
    err = vkGetSwapchainImagesKHR(device, this->swapchain, &swapchainImageCount, nullptr);
    if (err != VK_SUCCESS) {
        Log::error("vkGetSwapchainImagesKHR failed: {}", err);
        return false;
    }

    // Get the swap chain images
    std::vector<VkImage> swapchainImages(swapchainImageCount, VK_NULL_HANDLE);
    err = vkGetSwapchainImagesKHR(device, this->swapchain, &swapchainImageCount, swapchainImages.data());
    if (err != VK_SUCCESS) {
        Log::error("vkGetSwapchainImagesKHR failed: {}", err);
        return false;
    }

    // Get the swap chain buffers containing the image and imageview
    this->imageViews.reserve(swapchainImages.size());
    for (VkImage image : swapchainImages) {
        VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        imageViewCreateInfo.format = this->surfaceFormat.format;
        imageViewCreateInfo.components = {
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A
        };
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = image;

        VkImageView imageView = VK_NULL_HANDLE;
        err = vkCreateImageView(device, &imageViewCreateInfo, gdevice->allocationCallbacks(), &imageView);
        if (err != VK_SUCCESS) {
            Log::error("vkCreateImageView failed: {}", err);
            return false;
        }

        std::shared_ptr<VulkanImage> swapChainImage = std::make_shared<VulkanImage>(gdevice, image);
        swapChainImage->imageType = VK_IMAGE_TYPE_2D;
        swapChainImage->format = swapchainCI.imageFormat;
        swapChainImage->extent = { swapchainExtent.width, swapchainExtent.height, 1 };
        swapChainImage->mipLevels = 1;
        swapChainImage->arrayLayers = swapchainCI.imageArrayLayers;
        swapChainImage->usage = swapchainCI.imageUsage;

        std::shared_ptr<VulkanImageView> swapChainImageView = std::make_shared<VulkanImageView>(gdevice, imageView);
        swapChainImageView->image = swapChainImage;
        swapChainImageView->waitSemaphore = VK_NULL_HANDLE;
        swapChainImageView->signalSemaphore = VK_NULL_HANDLE;

        this->imageViews.push_back(swapChainImageView);
    }

    return true;
}

void VulkanSwapChain::setPixelFormat(PixelFormat pf) {
    std::scoped_lock guard(lock);
    VkFormat format = getVkFormat(pf);

    if (format != this->surfaceFormat.format) {
        if (isColorFormat(pf)) {
            bool formatChanged = false;
            if (availableSurfaceFormats.size() == 1 && availableSurfaceFormats.front().format == VK_FORMAT_UNDEFINED) {
                formatChanged = true;
                this->surfaceFormat.format = format;
                this->surfaceFormat.colorSpace = availableSurfaceFormats.front().colorSpace;
            } else {
                for (const VkSurfaceFormatKHR& fmt : availableSurfaceFormats) {
                    if (fmt.format == format) {
                        formatChanged = true;
                        this->surfaceFormat = fmt;
                        break;
                    }
                }
            }
            if (formatChanged) {
                this->deviceReset = true;
                Log::debug("SwapChain::setColorPixelFormat value changed!");
            } else {
                Log::error("SwapChain::setDepthStencilPixelFormat failed! (not supported format)");
            }
        } else {
            Log::error("SwapChain::setDepthStencilPixelFormat failed! (invalid format)");
        }
    }
}

PixelFormat VulkanSwapChain::pixelFormat() const {
    std::scoped_lock guard(lock);
    return getPixelFormat(this->surfaceFormat.format);
}

RenderPassDescriptor VulkanSwapChain::currentRenderPassDescriptor() {
    if (renderPassDescriptor.colorAttachments.empty())
        this->setupFrame();

    FVASSERT_DEBUG(renderPassDescriptor.colorAttachments.empty() == false);
    return renderPassDescriptor;
}

size_t VulkanSwapChain::maximumBufferCount() const {
    std::scoped_lock guard(lock);
    return imageViews.size();
}

void VulkanSwapChain::setupFrame() {
    auto& gdevice = cqueue->gdevice;
    VkDevice device = gdevice->device;

    auto index = frameCount % frameSemaphores.size();
    frameReady = &frameSemaphores.at(index);
    if (frameReady->frameIndex > 0) {
        uint64_t value = frameReady->frameIndex;
        VkSemaphoreWaitInfo waitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &frameTimelineSemaphore;
        waitInfo.pValues = &value;
        VkResult err = vkWaitSemaphores(device, &waitInfo, UINT64_MAX);
        if (err != VK_SUCCESS) {
            Log::error("vkWaitSemaphores failed: {}", err);
            FVASSERT_DEBUG(0);
        }
    }

    std::unique_lock guard(lock);
    VkResult err = vkAcquireNextImageKHR(device, this->swapchain, UINT64_MAX, frameReady->semaphore, VK_NULL_HANDLE, &this->frameIndex);
    guard.unlock();

    switch (err) {
    case VK_SUCCESS:
    case VK_TIMEOUT:
    case VK_NOT_READY:
    case VK_SUBOPTIMAL_KHR:
        break;
    default:
        Log::error("vkAcquireNextImageKHR failed: {}", err);
    }

    auto renderTarget = imageViews.at(frameIndex);
    renderTarget->waitSemaphore = frameReady->semaphore;
    renderTarget->signalSemaphore = frameReady->semaphore;

    RenderPassColorAttachmentDescriptor colorAttachment = {};
    colorAttachment.renderTarget = renderTarget;
    colorAttachment.clearColor = Color(0, 0, 0, 0);
    colorAttachment.loadAction = RenderPassAttachmentDescriptor::LoadActionClear;
    colorAttachment.storeAction = RenderPassAttachmentDescriptor::StoreActionStore;

    this->renderPassDescriptor.colorAttachments.clear();
    this->renderPassDescriptor.colorAttachments.push_back(colorAttachment);
}

bool VulkanSwapChain::present(GPUEvent** waitEvents, size_t numEvents) {
    FVASSERT_DEBUG(frameReady);

    auto presentSrc = imageViews.at(frameIndex);
    if (presentSrc->image->layout() != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        if (auto cbuffer = cqueue->makeCommandBuffer()) {
            if (auto encoder = std::dynamic_pointer_cast<VulkanCopyCommandEncoder>(cbuffer->makeCopyCommandEncoder())) {
                encoder->callback([&](auto commandBuffer) {
                    presentSrc->image->setLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_2_NONE,
                    VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    cqueue->family->familyIndex,
                    commandBuffer);
                });
                encoder->waitSemaphore(frameReady->semaphore, 0, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
                encoder->signalSemaphore(frameReady->semaphore, 0, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
                encoder->endEncoding();
                cbuffer->commit();
            }
        }
    }

    std::vector<VkSemaphore> waitSemaphores;
    waitSemaphores.reserve(numEvents + 1);

    for (size_t i = 0; i < numEvents; ++i) {
        GPUEvent* event = waitEvents[i];
        VulkanSemaphore* s = dynamic_cast<VulkanSemaphore*>(event);
        FVASSERT_DEBUG(s);
        waitSemaphores.push_back(s->semaphore);
    }
    waitSemaphores.push_back(frameReady->semaphore);

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &this->swapchain;
    presentInfo.pImageIndices = &this->frameIndex;

    // Check if a wait semaphore has been specified to wait for before presenting the image
    presentInfo.pWaitSemaphores = waitSemaphores.data();
    presentInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();

    VkResult err = vkQueuePresentKHR(cqueue->queue, &presentInfo);
    if (err != VK_SUCCESS) {
        Log::error("vkQueuePresentKHR failed: {}", err);
        //FVASSERT_DEBUG(err == VK_SUCCESS);
    }

    renderPassDescriptor.colorAttachments.clear();

    bool resetSwapchain = err == VK_ERROR_OUT_OF_DATE_KHR;
    std::unique_lock guard(lock, std::defer_lock_t{});
    if (resetSwapchain == false) {
        // Check if a device reset is requested and update the device if necessary.
        guard.lock();
        bool resetSwapchain = this->deviceReset;
        guard.unlock();
    }

    if (resetSwapchain) {
        auto& gdevice = cqueue->gdevice;
        vkDeviceWaitIdle(gdevice->device);
        guard.lock();
        this->deviceReset = false;
        guard.unlock();
        if (this->updateDevice() == false) {
            Log::error("VulkanSwapChain.updateDevice() failed.");
        }
    }

    frameCount += 1;
    if (err == VK_SUCCESS) {
        frameReady->frameIndex = frameCount;

        // signal timeline semaphore
        VkSubmitInfo2 submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
        VkSemaphoreSubmitInfo signalSemaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
        signalSemaphoreInfo.semaphore = frameTimelineSemaphore;
        signalSemaphoreInfo.value = frameCount;
        signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_NONE;
        signalSemaphoreInfo.deviceIndex = 0;

        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;

        if (vkQueueSubmit2(cqueue->queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            Log::error("vkQueueSubmit2 failed: {}", err);
            //FVASSERT_DEBUG(0);
        }
    } else {
        frameReady->frameIndex = 0;
    }

    return err == VK_SUCCESS;
}

void VulkanSwapChain::onWindowEvent(const Window::WindowEvent& e) {
    if (e.type == Window::WindowEvent::WindowResized) {
        std::scoped_lock guard(lock);
        this->deviceReset = true;
    }
}

#endif //#if FVCORE_ENABLE_VULKAN
