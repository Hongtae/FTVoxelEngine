#pragma once
#include <memory>
#include <atomic>
#include "../../GPUResource.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanSemaphore : public GPUEvent {
    public:
        VulkanSemaphore(std::shared_ptr<VulkanGraphicsDevice>, VkSemaphore);
        ~VulkanSemaphore();

        std::shared_ptr<VulkanGraphicsDevice> device;
        VkSemaphore semaphore;

        virtual uint64_t nextWaitValue() const { return 0; }
        virtual uint64_t nextSignalValue() const { return 0; }

        virtual bool isBinarySemaphore() const { return true; }
    };

    class VulkanSemaphoreAutoIncrementalTimeline : public VulkanSemaphore {
        mutable std::atomic<uint64_t> waitValue;
        mutable std::atomic<uint64_t> signalValue;

    public:
        VulkanSemaphoreAutoIncrementalTimeline(std::shared_ptr<VulkanGraphicsDevice>, VkSemaphore);

        uint64_t nextWaitValue() const override;
        uint64_t nextSignalValue() const override;

        bool isBinarySemaphore() const override { return false; }
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
