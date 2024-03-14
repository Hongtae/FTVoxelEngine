#pragma once
#include <memory>
#include <vector>
#include <map>
#include "../../CommandBuffer.h"
#include "../../CommandQueue.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanQueueFamily.h"

namespace FV {
    class VulkanCommandBuffer;
    class VulkanCommandEncoder {
    public:
        enum { InitialNumberOfCommands = 128 };

        struct TimelineSemaphoreStageValue {
            VkPipelineStageFlags2 stages;
            uint64_t value; // 0 for non-timeline semaphore (binary semaphore)
        };
        std::map<VkSemaphore, TimelineSemaphoreStageValue> waitSemaphores;
        std::map<VkSemaphore, TimelineSemaphoreStageValue> signalSemaphores;

        virtual ~VulkanCommandEncoder() = default;
        virtual bool encode(VkCommandBuffer) = 0;

        void addWaitSemaphore(VkSemaphore semaphore, uint64_t value, VkPipelineStageFlags2 flags) {
            if (semaphore != VK_NULL_HANDLE) {
                if (auto pair = waitSemaphores.emplace(semaphore, TimelineSemaphoreStageValue{ flags, value });
                    pair.second == false) { // semaphore already exists.
                    if (value > pair.first->second.value)
                        pair.first->second.value = value;
                    pair.first->second.stages |= flags;
                }
            }
        }
        void addSignalSemaphore(VkSemaphore semaphore, uint64_t value, VkPipelineStageFlags2 flags) {
            if (semaphore != VK_NULL_HANDLE) {
                if (auto pair = signalSemaphores.emplace(semaphore, TimelineSemaphoreStageValue{ flags, value });
                    pair.second == false) {
                    if (value > pair.first->second.value)
                        pair.first->second.value = value;
                    pair.first->second.stages |= flags;
                }
            }
        }
    };

    class VulkanCommandBuffer : public CommandBuffer, public std::enable_shared_from_this<VulkanCommandBuffer> {
    public:
        VulkanCommandBuffer(std::shared_ptr<VulkanCommandQueue>, VkCommandPool);
        ~VulkanCommandBuffer();

        std::shared_ptr<RenderCommandEncoder> makeRenderCommandEncoder(const RenderPassDescriptor&) override;
        std::shared_ptr<ComputeCommandEncoder> makeComputeCommandEncoder() override;
        std::shared_ptr<CopyCommandEncoder> makeCopyCommandEncoder() override;

        void addCompletedHandler(std::function<void()>) override;
        bool commit() override;

        std::shared_ptr<CommandQueue> queue() const override;

        VulkanQueueFamily* queueFamily();

        void endEncoder(CommandEncoder*, std::shared_ptr<VulkanCommandEncoder>);

    private:
        VkCommandPool cpool;
        std::shared_ptr<VulkanCommandQueue> cqueue;
        std::vector<std::shared_ptr<VulkanCommandEncoder>> encoders;

        std::vector<VkSubmitInfo2>              submitInfos;
        std::vector<VkCommandBufferSubmitInfo>  commandBufferSubmitInfos;
        std::vector<VkSemaphoreSubmitInfo>      waitSemaphores;
        std::vector<VkSemaphoreSubmitInfo>      signalSemaphores;

        std::vector<std::function<void()>> completedHandlers;

        std::mutex lock;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
