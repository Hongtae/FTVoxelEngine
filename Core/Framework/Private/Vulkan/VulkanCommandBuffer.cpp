#include "VulkanCommandBuffer.h"
#include "VulkanRenderCommandEncoder.h"
#include "VulkanComputeCommandEncoder.h"
#include "VulkanCopyCommandEncoder.h"
#include "VulkanGraphicsDevice.h"
#include "VulkanCommandQueue.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanCommandBuffer::VulkanCommandBuffer(std::shared_ptr<VulkanCommandQueue> q, VkCommandPool p)
    : cpool(p)
    , cqueue(q) {
    FVASSERT_DEBUG(cpool);
}

VulkanCommandBuffer::~VulkanCommandBuffer() {
    auto gdevice = std::dynamic_pointer_cast<VulkanGraphicsDevice>(cqueue->device());

    if (commandBufferSubmitInfos.empty() == false) {
        std::vector<VkCommandBuffer> commandBuffers;
        commandBuffers.reserve(commandBufferSubmitInfos.size());
        for (auto& info : commandBufferSubmitInfos)
            commandBuffers.push_back(info.commandBuffer);

        vkFreeCommandBuffers(gdevice->device, cpool, (uint32_t)commandBuffers.size(), commandBuffers.data());
    }
    vkDestroyCommandPool(gdevice->device, cpool, gdevice->allocationCallbacks());
}

std::shared_ptr<RenderCommandEncoder> VulkanCommandBuffer::makeRenderCommandEncoder(const RenderPassDescriptor& rp) {
    if (cqueue->family->properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        return std::make_shared<VulkanRenderCommandEncoder>(shared_from_this(), rp);
    }
    return nullptr;
}

std::shared_ptr<ComputeCommandEncoder> VulkanCommandBuffer::makeComputeCommandEncoder() {
    if (cqueue->family->properties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
        return std::make_shared<VulkanComputeCommandEncoder>(shared_from_this());
    }
    return nullptr;
}

std::shared_ptr<CopyCommandEncoder> VulkanCommandBuffer::makeCopyCommandEncoder() {
    return std::make_shared<VulkanCopyCommandEncoder>(shared_from_this());
}

void VulkanCommandBuffer::addCompletedHandler(std::function<void()> op) {
    if (op) {
        completedHandlers.push_back(op);
    }
}

bool VulkanCommandBuffer::commit() {
    auto gdevice = std::dynamic_pointer_cast<VulkanGraphicsDevice>(cqueue->device());
    VkDevice device = gdevice->device;

    std::scoped_lock guard(lock);

    auto cleanup = [this, device](bool result) {
        if (commandBufferSubmitInfos.empty() == false) {
            std::vector<VkCommandBuffer> commandBuffers;
            commandBuffers.reserve(commandBufferSubmitInfos.size());
            for (auto& info : commandBufferSubmitInfos)
                commandBuffers.push_back(info.commandBuffer);

            vkFreeCommandBuffers(device, cpool, (uint32_t)commandBuffers.size(), commandBuffers.data());
        }

        submitInfos.clear();
        commandBufferSubmitInfos.clear();
        waitSemaphores.clear();
        signalSemaphores.clear();
        return result;
    };

    if (submitInfos.size() != encoders.size()) {
        cleanup(false);

        // reserve storage for semaphores.
        size_t numWaitSemaphores = 0;
        size_t numSignalSemaphores = 0;
        for (auto& encoder : encoders) {
            numWaitSemaphores += encoder->waitSemaphores.size();
            numSignalSemaphores += encoder->signalSemaphores.size();
        }
        waitSemaphores.reserve(numWaitSemaphores);
        signalSemaphores.reserve(numSignalSemaphores);

        commandBufferSubmitInfos.reserve(encoders.size());
        submitInfos.reserve(encoders.size());

        for (auto& encoder : encoders) {
            size_t commandBuffersOffset = commandBufferSubmitInfos.size();
            size_t waitSemaphoresOffset = waitSemaphores.size();
            size_t signalSemaphoresOffset = signalSemaphores.size();

            VkCommandBufferAllocateInfo  bufferInfo = {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
            };
            bufferInfo.commandPool = cpool;
            bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            bufferInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
            VkResult err = vkAllocateCommandBuffers(device, &bufferInfo, &commandBuffer);
            if (err != VK_SUCCESS) {
                Log::error("vkAllocateCommandBuffers failed: {}", err);
                return cleanup(false);
            }
            VkCommandBufferSubmitInfo cbufferSubmitInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
            cbufferSubmitInfo.commandBuffer = commandBuffer;
            cbufferSubmitInfo.deviceMask = 0;
            commandBufferSubmitInfos.push_back(cbufferSubmitInfo);

            auto appendSemaphores = [](const std::map<VkSemaphore, VulkanCommandEncoder::TimelineSemaphoreStageValue>& semaphores, std::vector<VkSemaphoreSubmitInfo>& container) {
                for (auto& pair : semaphores) {
                    VkSemaphore semaphore = pair.first;
                    VkPipelineStageFlags2 stages = pair.second.stages;
                    uint64_t value = pair.second.value;

                    FVASSERT_DEBUG(semaphore);
                    FVASSERT_DEBUG((stages & VK_PIPELINE_STAGE_2_HOST_BIT) == 0);

                    VkSemaphoreSubmitInfo semaphoreSubmitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
                    semaphoreSubmitInfo.semaphore = semaphore;
                    semaphoreSubmitInfo.value = value;
                    semaphoreSubmitInfo.stageMask = stages;
                    semaphoreSubmitInfo.deviceIndex = 0;
                    container.push_back(semaphoreSubmitInfo);
                }
            };
            appendSemaphores(encoder->waitSemaphores, waitSemaphores);
            appendSemaphores(encoder->signalSemaphores, signalSemaphores);

            VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
            bool result = encoder->encode(commandBuffer);
            vkEndCommandBuffer(commandBuffer);

            if (!result) {
                return cleanup(false);
            }

            VkSubmitInfo2 submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };

            if (commandBufferSubmitInfos.size() > commandBuffersOffset) {
                uint32_t count = uint32_t(commandBufferSubmitInfos.size() - commandBuffersOffset);
                VkCommandBufferSubmitInfo* cbufferSubmitInfos = commandBufferSubmitInfos.data();

                submitInfo.commandBufferInfoCount = count;
                submitInfo.pCommandBufferInfos = &cbufferSubmitInfos[commandBuffersOffset];
            }
            if (waitSemaphores.size() > waitSemaphoresOffset) {
                uint32_t count = uint32_t(waitSemaphores.size() - waitSemaphoresOffset);
                VkSemaphoreSubmitInfo* semaphoreInfos = waitSemaphores.data();

                submitInfo.waitSemaphoreInfoCount = count;
                submitInfo.pWaitSemaphoreInfos = &semaphoreInfos[waitSemaphoresOffset];
            }
            if (signalSemaphores.size() > signalSemaphoresOffset) {
                uint32_t count = uint32_t(signalSemaphores.size() - signalSemaphoresOffset);
                VkSemaphoreSubmitInfo* semaphoreInfos = signalSemaphores.data();

                submitInfo.signalSemaphoreInfoCount = count;
                submitInfo.pSignalSemaphoreInfos = &semaphoreInfos[signalSemaphoresOffset];
            }
            submitInfos.push_back(submitInfo);
        }
    }

    if (submitInfos.empty() == false) {
        FVASSERT_DEBUG(submitInfos.size() == encoders.size());

        auto cb = this->shared_from_this();
        FVASSERT_DEBUG(cb);
        bool result = cqueue->submit(submitInfos.data(),
                                     (uint32_t)submitInfos.size(),
                                     [cb] {
                                         for (auto& op : cb->completedHandlers)
                                             op();
                                     });
        return result;
    }
    return true;
}

void VulkanCommandBuffer::endEncoder(CommandEncoder*, std::shared_ptr<VulkanCommandEncoder> encoder) {
    this->encoders.push_back(encoder);
}

VulkanQueueFamily* VulkanCommandBuffer::queueFamily() {
    return cqueue->family;
}

std::shared_ptr<CommandQueue> VulkanCommandBuffer::queue() const {
    return cqueue;
}

#endif //#if FVCORE_ENABLE_VULKAN
