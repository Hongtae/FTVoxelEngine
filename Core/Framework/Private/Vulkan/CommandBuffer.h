#pragma once
#include <memory>
#include <vector>
#include <map>
#include "../../CommandBuffer.h"
#include "../../CommandQueue.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "QueueFamily.h"

namespace FV::Vulkan {
    class CommandBuffer;
    class CommandEncoder {
    public:
        enum { InitialNumberOfCommands = 128 };

        struct TimelineSemaphoreStageValue {
            VkPipelineStageFlags2 stages;
            uint64_t value; // 0 for non-timeline semaphore (binary semaphore)
        };
        std::map<VkSemaphore, TimelineSemaphoreStageValue> waitSemaphores;
        std::map<VkSemaphore, TimelineSemaphoreStageValue> signalSemaphores;

        virtual ~CommandEncoder() {}
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

    class CommandBuffer : public FV::CommandBuffer, public std::enable_shared_from_this<CommandBuffer> {
    public:
        CommandBuffer(std::shared_ptr<CommandQueue>, VkCommandPool);
        ~CommandBuffer();

        std::shared_ptr<FV::RenderCommandEncoder> makeRenderCommandEncoder(const RenderPassDescriptor&) override;
        std::shared_ptr<FV::ComputeCommandEncoder> makeComputeCommandEncoder() override;
        std::shared_ptr<FV::CopyCommandEncoder> makeCopyCommandEncoder() override;

        void addCompletedHandler(std::function<void()>) override;
        bool commit() override;

        std::shared_ptr<FV::CommandQueue> queue() const override;

        QueueFamily* queueFamily();

        void endEncoder(FV::CommandEncoder*, std::shared_ptr<CommandEncoder>);

    private:
        VkCommandPool cpool;
        std::shared_ptr<CommandQueue> cqueue;
        std::vector<std::shared_ptr<CommandEncoder>> encoders;

        std::vector<VkSubmitInfo2>              submitInfos;
        std::vector<VkCommandBufferSubmitInfo>  commandBufferSubmitInfos;
        std::vector<VkSemaphoreSubmitInfo>      waitSemaphores;
        std::vector<VkSemaphoreSubmitInfo>      signalSemaphores;

        std::vector<std::function<void()>> completedHandlers;

        std::mutex lock;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
