#include "CommandBuffer.h"
#include "RenderCommandEncoder.h"
#include "ComputeCommandEncoder.h"
#include "CopyCommandEncoder.h"
#include "GraphicsDevice.h"
#include "CommandQueue.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

CommandBuffer::CommandBuffer(std::shared_ptr<CommandQueue> q, VkCommandPool p)
	: cpool(p)
	, cqueue(q)
{
	FVASSERT_DEBUG(cpool);
}

CommandBuffer::~CommandBuffer()
{
    auto gdevice = std::dynamic_pointer_cast<GraphicsDevice>(cqueue->device());

    if (submitCommandBuffers.empty() == false)
    {
        vkFreeCommandBuffers(gdevice->device, cpool, submitCommandBuffers.size(), submitCommandBuffers.data());
    }
	vkDestroyCommandPool(gdevice->device, cpool, gdevice->allocationCallbacks());
}

std::shared_ptr<FV::RenderCommandEncoder> CommandBuffer::makeRenderCommandEncoder(const RenderPassDescriptor& rp)
{
    if (cqueue->family->properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
    {
        return std::make_shared<RenderCommandEncoder>(shared_from_this(), rp);
    }
    return nullptr;
}

std::shared_ptr<FV::ComputeCommandEncoder> CommandBuffer::makeComputeCommandEncoder()
{
    if (cqueue->family->properties.queueFlags & VK_QUEUE_COMPUTE_BIT)
    {
        return std::make_shared<ComputeCommandEncoder>(shared_from_this());
    }
    return nullptr;
}

std::shared_ptr<FV::CopyCommandEncoder> CommandBuffer::makeCopyCommandEncoder()
{
    return std::make_shared<CopyCommandEncoder>(shared_from_this());
}

void CommandBuffer::addCompletedHandler(std::function<void()> op)
{
    if (op)
    {
        completedHandlers.push_back(op);
    }
}

bool CommandBuffer::commit()
{
    auto gdevice = std::dynamic_pointer_cast<GraphicsDevice>(cqueue->device());
    VkDevice device = gdevice->device;

    auto cleanup = [this, device](bool result)
    {
        if (submitCommandBuffers.empty() == false)
            vkFreeCommandBuffers(device, cpool, submitCommandBuffers.size(), submitCommandBuffers.data());

        submitInfos.clear();
        submitCommandBuffers.clear();
        submitWaitSemaphores.clear();
        submitWaitStageMasks.clear();
        submitSignalSemaphores.clear();

        submitWaitTimelineSemaphoreValues.clear();
        submitSignalTimelineSemaphoreValues.clear();
        submitTimelineSemaphoreInfos.clear();
        return result;
    };

    if (submitInfos.size() != encoders.size())
    {
        cleanup(false);

        // reserve storage for semaphores.
        size_t numWaitSemaphores = 0;
        size_t numSignalSemaphores = 0;
        for (auto& encoder : encoders)
        {
            numWaitSemaphores += encoder->waitSemaphores.size();
            numSignalSemaphores += encoder->signalSemaphores.size();
        }
        submitWaitSemaphores.reserve(numWaitSemaphores);
        submitWaitStageMasks.reserve(numWaitSemaphores);
        submitSignalSemaphores.reserve(numSignalSemaphores);

        submitWaitTimelineSemaphoreValues.reserve(numWaitSemaphores);
        submitSignalTimelineSemaphoreValues.reserve(numSignalSemaphores);
        
        submitCommandBuffers.reserve(encoders.size());
        submitInfos.reserve(encoders.size());
        submitTimelineSemaphoreInfos.reserve(encoders.size());

        for (auto& encoder : encoders)
        {
            size_t commandBuffersOffset = submitCommandBuffers.size();
            size_t waitSemaphoresOffset = submitWaitSemaphores.size();
            size_t signalSemaphoresOffset = submitSignalSemaphores.size();

            FVASSERT_DEBUG(submitWaitStageMasks.size() == waitSemaphoresOffset);
            FVASSERT_DEBUG(submitWaitTimelineSemaphoreValues.size() == waitSemaphoresOffset);
            FVASSERT_DEBUG(submitSignalTimelineSemaphoreValues.size() == signalSemaphoresOffset);

            VkCommandBufferAllocateInfo  bufferInfo = {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
            };
            bufferInfo.commandPool = cpool;
            bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            bufferInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
            VkResult err = vkAllocateCommandBuffers(device, &bufferInfo, &commandBuffer);
            if (err != VK_SUCCESS)
            {
                Log::error(std::format("vkAllocateCommandBuffers failed: {}", getVkResultString(err)));
                return cleanup(false);
            }
            submitCommandBuffers.push_back(commandBuffer);

            for (auto& pair : encoder->waitSemaphores)
            {
                VkSemaphore semaphore = pair.first;
                VkPipelineStageFlags stages = pair.second.stages;
                uint64_t value = pair.second.value;

                FVASSERT_DEBUG(semaphore);
                FVASSERT_DEBUG((stages & VK_PIPELINE_STAGE_HOST_BIT) == 0);

                submitWaitSemaphores.push_back(semaphore);
                submitWaitStageMasks.push_back(stages);
                submitWaitTimelineSemaphoreValues.push_back(value);
            }
            FVASSERT_DEBUG(submitWaitSemaphores.size() <= numWaitSemaphores);
            FVASSERT_DEBUG(submitWaitStageMasks.size() <= numWaitSemaphores);
            FVASSERT_DEBUG(submitWaitTimelineSemaphoreValues.size() <= numWaitSemaphores);

            for (auto& pair : encoder->signalSemaphores)
            {
                VkSemaphore semaphore = pair.first;
                uint64_t value = pair.second;

                FVASSERT_DEBUG(semaphore);
                submitSignalSemaphores.push_back(semaphore);
                submitSignalTimelineSemaphoreValues.push_back(value);
            }
            FVASSERT_DEBUG(submitSignalSemaphores.size() <= numSignalSemaphores);
            FVASSERT_DEBUG(submitSignalTimelineSemaphoreValues.size() <= numSignalSemaphores);

            VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
            bool result = encoder->encode(commandBuffer);
            vkEndCommandBuffer(commandBuffer);

            if (!result)
            {
                return cleanup(false);
            }

            FVASSERT_DEBUG(submitWaitSemaphores.size() == submitWaitStageMasks.size());

            VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            VkTimelineSemaphoreSubmitInfoKHR timelineSemaphoreSubmitInfo = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR };

            if (submitCommandBuffers.size() > commandBuffersOffset)
            {
                uint32_t count = submitCommandBuffers.size() - commandBuffersOffset;
                VkCommandBuffer* commandBuffers = submitCommandBuffers.data();
                submitInfo.commandBufferCount = count;
                submitInfo.pCommandBuffers = &commandBuffers[commandBuffersOffset];
            }
            if (submitWaitSemaphores.size() > waitSemaphoresOffset)
            {
                uint32_t count = submitWaitSemaphores.size() - waitSemaphoresOffset;
                VkSemaphore* semaphores = submitWaitSemaphores.data();
                VkPipelineStageFlags* stages = submitWaitStageMasks.data();
                const uint64_t* timelineValues = submitWaitTimelineSemaphoreValues.data();

                submitInfo.waitSemaphoreCount = count;
                submitInfo.pWaitSemaphores = &semaphores[waitSemaphoresOffset];
                submitInfo.pWaitDstStageMask = &stages[waitSemaphoresOffset];

                timelineSemaphoreSubmitInfo.pWaitSemaphoreValues = &timelineValues[waitSemaphoresOffset];
                timelineSemaphoreSubmitInfo.waitSemaphoreValueCount = count;
            }
            if (submitSignalSemaphores.size() > signalSemaphoresOffset)
            {
                uint32_t count = submitSignalSemaphores.size() - signalSemaphoresOffset;
                VkSemaphore* semaphores = submitSignalSemaphores.data();
                const uint64_t* timelineValue = submitSignalTimelineSemaphoreValues.data();

                submitInfo.signalSemaphoreCount = count;
                submitInfo.pSignalSemaphores = &semaphores[signalSemaphoresOffset];

                timelineSemaphoreSubmitInfo.pSignalSemaphoreValues = &timelineValue[signalSemaphoresOffset];
                timelineSemaphoreSubmitInfo.signalSemaphoreValueCount = count;
            }
            auto index = submitTimelineSemaphoreInfos.size();
            submitTimelineSemaphoreInfos.push_back(timelineSemaphoreSubmitInfo);
            VkTimelineSemaphoreSubmitInfoKHR& semaphoreInfo = submitTimelineSemaphoreInfos.at(index);
            submitInfo.pNext = &semaphoreInfo;
            submitInfos.push_back(submitInfo);
        }
    }

    if (submitInfos.size() > 0)
    {
        FVASSERT_DEBUG(submitTimelineSemaphoreInfos.size() == encoders.size());
        FVASSERT_DEBUG(submitTimelineSemaphoreInfos.size() == submitInfos.size());

        auto cb = this->shared_from_this();
        FVASSERT_DEBUG(cb);
        bool result = cqueue->submit(submitInfos.data(), submitInfos.size(),
                                     [cb]()
                                     {
                                         for (auto& op : cb->completedHandlers)
                                             op();
                                     });
        return result;
    }
    return true;
}

void CommandBuffer::endEncoder(FV::CommandEncoder*, std::shared_ptr<CommandEncoder> encoder)
{
    this->encoders.push_back(encoder);
}

QueueFamily* CommandBuffer::queueFamily()
{
    return cqueue->family;
}

std::shared_ptr<FV::CommandQueue> CommandBuffer::queue() const
{
    return cqueue;
}

#endif //#if FVCORE_ENABLE_VULKAN
