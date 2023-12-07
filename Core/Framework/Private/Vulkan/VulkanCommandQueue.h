#pragma once
#include <memory>
#include <mutex>
#include "../../CommandQueue.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanQueueFamily;
    class VulkanCommandQueue : public CommandQueue, public std::enable_shared_from_this<VulkanCommandQueue> {
    public:
        VulkanCommandQueue(std::shared_ptr<VulkanGraphicsDevice>, VulkanQueueFamily*, VkQueue);
        ~VulkanCommandQueue();

        std::shared_ptr<CommandBuffer> makeCommandBuffer() override;
        std::shared_ptr<SwapChain> makeSwapChain(std::shared_ptr<Window>) override;

        bool submit(const VkSubmitInfo2* submits, uint32_t submitCount, std::function<void()> callback);

        bool waitIdle();

        uint32_t flags() const override;
        std::shared_ptr<GraphicsDevice> device() const override;

        VulkanQueueFamily* family;
        VkQueue queue;

        std::shared_ptr<VulkanGraphicsDevice> gdevice;
        std::mutex lock;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
