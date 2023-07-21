#pragma once
#include <memory>
#include <vector>
#include <map>
#include "../../CommandBuffer.h"
#include "../../CommandQueue.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "QueueFamily.h"

namespace FV::Vulkan
{
    class CommandBuffer;
    class CommandEncoder
    {
    public:
        enum { InitialNumberOfCommands = 128 };

        struct WaitTimelineSemaphoreStageValue
        {
            VkPipelineStageFlags stages;
            uint64_t value; // 0 for non-timeline semaphore (binary semaphore)
        };
        std::map<VkSemaphore, WaitTimelineSemaphoreStageValue> waitSemaphores;
        std::map<VkSemaphore, uint64_t> signalSemaphores;

        virtual ~CommandEncoder() {}
        virtual bool encode(VkCommandBuffer) = 0;

        void addWaitSemaphore(VkSemaphore semaphore, uint64_t value, VkPipelineStageFlags flags)
        {
            if (semaphore != VK_NULL_HANDLE)
            {
                if (auto pair = waitSemaphores.emplace(semaphore, WaitTimelineSemaphoreStageValue{ flags, value });
                    pair.second == false) // semaphore already exists.
                {
                    if (value > pair.first->second.value)
                        pair.first->second.value = value;
                    pair.first->second.stages |= flags;
                }
            }
        }
        void addSignalSemaphore(VkSemaphore semaphore, uint64_t value)
        {
            if (semaphore != VK_NULL_HANDLE)
            {
                if (auto pair = signalSemaphores.emplace(semaphore, value);
                    pair.second == false)
                {
                    if (value > pair.first->second)
                        pair.first->second = value;
                }
            }
        }
    };

	class CommandBuffer : public FV::CommandBuffer, public std::enable_shared_from_this<CommandBuffer>
	{
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

        std::vector<VkSubmitInfo>           submitInfos;
        std::vector<VkCommandBuffer>        submitCommandBuffers;
        std::vector<VkSemaphore>            submitWaitSemaphores;
        std::vector<VkPipelineStageFlags>   submitWaitStageMasks;
        std::vector<VkSemaphore>            submitSignalSemaphores;

        std::vector<uint64_t>               submitWaitTimelineSemaphoreValues;
        std::vector<uint64_t>               submitSignalTimelineSemaphoreValues;
        std::vector<VkTimelineSemaphoreSubmitInfoKHR> submitTimelineSemaphoreInfos;

        std::vector<std::function<void()>> completedHandlers;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
