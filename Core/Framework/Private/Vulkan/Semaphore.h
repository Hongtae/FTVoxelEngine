#pragma once
#include <memory>
#include <atomic>
#include "../../GPUResource.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV::Vulkan
{
    class GraphicsDevice;
    class Semaphore : public FV::GPUEvent
    {
    public:
        Semaphore(std::shared_ptr<GraphicsDevice>, VkSemaphore);
        ~Semaphore();

        std::shared_ptr<GraphicsDevice> device;
        VkSemaphore semaphore;

        virtual uint64_t nextWaitValue() const { return 0; }
        virtual uint64_t nextSignalValue() const { return 0; }
    };

    class AutoIncrementalTimelineSemaphore : public Semaphore
    {
        mutable std::atomic<uint64_t> waitValue;
        mutable std::atomic<uint64_t> signalValue;

    public:
        AutoIncrementalTimelineSemaphore(std::shared_ptr<GraphicsDevice>, VkSemaphore);

        uint64_t nextWaitValue() const override;
        uint64_t nextSignalValue() const override;
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
