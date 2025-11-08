#include "VulkanCommandQueue.h"
#include "VulkanCommandBuffer.h"
#include "VulkanGraphicsDevice.h"
#include "VulkanSwapChain.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanCommandQueue::VulkanCommandQueue(std::shared_ptr<VulkanGraphicsDevice> d, VulkanQueueFamily* f, VkQueue q)
    : gdevice(d)
    , family(f)
    , queue(q) {
}

VulkanCommandQueue::~VulkanCommandQueue() {
    vkQueueWaitIdle(queue);
    family->recycleQueue(queue);
}

std::shared_ptr<CommandBuffer> VulkanCommandQueue::makeCommandBuffer() {
    VkCommandPoolCreateInfo cmdPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cmdPoolCreateInfo.queueFamilyIndex = this->family->familyIndex;
    cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkResult err = vkCreateCommandPool(gdevice->device, &cmdPoolCreateInfo, gdevice->allocationCallbacks(), &commandPool);
    if (err != VK_SUCCESS) {
        Log::error("vkCreateCommandPool failed: {}", err);
        return nullptr;
    }

    std::shared_ptr<VulkanCommandBuffer> buffer = std::make_shared<VulkanCommandBuffer>(shared_from_this(), commandPool);
    return buffer;
}

std::shared_ptr<SwapChain> VulkanCommandQueue::makeSwapChain(std::shared_ptr<Window> window) {
    if (this->family->supportPresentation == false) {
        Log::error("Vulkan WSI not supported with this queue family. Try to use other queue family!");
        return nullptr;
    }

    auto swapchain = std::make_shared<VulkanSwapChain>(shared_from_this(), window);
    if (swapchain->setup()) {
        return swapchain;
    } else {
        Log::error("VulkanSwapChain.setup() failed.");
    }
    return nullptr;
}

bool VulkanCommandQueue::submit(const VkSubmitInfo2* submits, uint32_t submitCount, std::function<void()> callback) {
    VkFence fence = VK_NULL_HANDLE;
    if (callback)
        fence = gdevice->fence();

    std::unique_lock guard(lock);
    VkResult err = vkQueueSubmit2(queue, submitCount, submits, fence);
    guard.unlock();

    if (err != VK_SUCCESS) {
        Log::error("vkQueueSubmit2 failed: {}", err);
        FVASSERT(err == VK_SUCCESS);
    }
    if (fence)
        gdevice->addFenceCompletionHandler(fence, callback);

    return err == VK_SUCCESS;
}

bool VulkanCommandQueue::waitIdle() {
    std::unique_lock guard(lock);
    return vkQueueWaitIdle(queue) == VK_SUCCESS;
}

uint32_t VulkanCommandQueue::flags() const {
    VkQueueFlags queueFlags = family->properties.queueFlags;
    uint32_t flags = Copy; /* Copy = 0 */
    if (queueFlags & VK_QUEUE_GRAPHICS_BIT)
        flags = flags | Render;
    if (queueFlags & VK_QUEUE_COMPUTE_BIT)
        flags = flags | Compute;
    return flags;
}

std::shared_ptr<GraphicsDevice> VulkanCommandQueue::device() const {
    return gdevice;
}
#endif //#if FVCORE_ENABLE_VULKAN
