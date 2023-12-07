#include "VulkanComputeCommandEncoder.h"
#include "VulkanComputePipelineState.h"
#include "VulkanShaderBindingSet.h"
#include "VulkanGraphicsDevice.h"
#include "VulkanSemaphore.h"
#include "VulkanTimelineSemaphore.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanComputeCommandEncoder::Encoder::Encoder(VulkanCommandBuffer* cb)
    : cbuffer(cb) {
    commands.reserve(InitialNumberOfCommands);
    setupCommands.reserve(InitialNumberOfCommands);
    cleanupCommands.reserve(InitialNumberOfCommands);
}

VulkanComputeCommandEncoder::Encoder::~Encoder() {
}

bool VulkanComputeCommandEncoder::Encoder::encode(VkCommandBuffer commandBuffer) {
    // recording commands
    EncodingState state = { this };
    // collect image layout transition
    for (auto& ds : descriptorSets) {
        ds->collectImageViewLayouts(state.imageLayoutMap, state.imageViewLayoutMap);
    }
    for (EncoderCommand& cmd : setupCommands) {
        cmd(commandBuffer, state);
    }
    // Set image layout transition
    for (auto& pair : state.imageLayoutMap) {
        VulkanImage* image = pair.first;
        VkImageLayout layout = pair.second;
        VkAccessFlags2 accessMask = VulkanImage::commonLayoutAccessMask(layout);

        image->setLayout(layout,
                         accessMask,
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                         state.encoder->cbuffer->queueFamily()->familyIndex,
                         commandBuffer);
    }
    for (EncoderCommand& cmd : commands) {
        cmd(commandBuffer, state);
    }
    for (EncoderCommand& cmd : cleanupCommands) {
        cmd(commandBuffer, state);
    }
    return true;
}

VulkanComputeCommandEncoder::VulkanComputeCommandEncoder(std::shared_ptr<VulkanCommandBuffer> cb)
    : cbuffer(cb) {
    encoder = std::make_shared<Encoder>(cb.get());
}

void VulkanComputeCommandEncoder::endEncoding() {
    cbuffer->endEncoder(this, encoder);
    encoder = nullptr;
}

void VulkanComputeCommandEncoder::waitEvent(std::shared_ptr<GPUEvent> event) {
    auto semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(event);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    encoder->addWaitSemaphore(semaphore->semaphore, semaphore->nextWaitValue(), pipelineStages);
    encoder->events.push_back(semaphore);
}

void VulkanComputeCommandEncoder::signalEvent(std::shared_ptr<GPUEvent> event) {
    auto semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(event);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    encoder->addSignalSemaphore(semaphore->semaphore, semaphore->nextSignalValue(), pipelineStages);
    encoder->events.push_back(semaphore);
}

void VulkanComputeCommandEncoder::waitSemaphoreValue(std::shared_ptr<GPUSemaphore> sema, uint64_t value) {
    auto semaphore = std::dynamic_pointer_cast<VulkanTimelineSemaphore>(sema);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    encoder->addWaitSemaphore(semaphore->semaphore, value, pipelineStages);
    encoder->semaphores.push_back(semaphore);
}

void VulkanComputeCommandEncoder::signalSemaphoreValue(std::shared_ptr<GPUSemaphore> sema, uint64_t value) {
    auto semaphore = std::dynamic_pointer_cast<VulkanTimelineSemaphore>(sema);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    encoder->addSignalSemaphore(semaphore->semaphore, value, pipelineStages);
    encoder->semaphores.push_back(semaphore);
}

void VulkanComputeCommandEncoder::setResource(uint32_t index, std::shared_ptr<ShaderBindingSet> set) {
    std::shared_ptr<VulkanDescriptorSet> descriptorSet = nullptr;
    if (set) {
        auto bindingSet = std::dynamic_pointer_cast<VulkanShaderBindingSet>(set);
        descriptorSet = bindingSet->makeDescriptorSet();
        FVASSERT_DEBUG(descriptorSet);
        encoder->descriptorSets.push_back(descriptorSet);
    }
    if (descriptorSet == nullptr)
        return;

    EncoderCommand preCommand = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        descriptorSet->updateImageViewLayouts(state.imageViewLayoutMap);
    };
    encoder->setupCommands.push_back(preCommand);

    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        if (state.pipelineState) {
            VkDescriptorSet ds = descriptorSet->descriptorSet;
            FVASSERT_DEBUG(ds != VK_NULL_HANDLE);

            vkCmdBindDescriptorSets(cbuffer,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    state.pipelineState->layout,
                                    index,
                                    1,
                                    &ds,
                                    0,      // dynamic offsets
                                    nullptr);
        }
    };
    encoder->commands.push_back(command);
}

void VulkanComputeCommandEncoder::setComputePipelineState(std::shared_ptr<ComputePipelineState> ps) {
    auto pipeline = std::dynamic_pointer_cast<VulkanComputePipelineState>(ps);
    FVASSERT_DEBUG(pipeline);

    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        vkCmdBindPipeline(cbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
        state.pipelineState = pipeline.get();
    };
    encoder->commands.push_back(command);
    encoder->pipelineStateObjects.push_back(pipeline);
}

void VulkanComputeCommandEncoder::pushConstant(uint32_t stages, uint32_t offset, uint32_t size, const void* data) {
    VkShaderStageFlags stageFlags = 0;
    for (auto& stage : {
            std::pair(ShaderStage::Compute, VK_SHADER_STAGE_COMPUTE_BIT),
         }) {
        if (stages & (uint32_t)stage.first)
            stageFlags |= stage.second;
    }
    if (stageFlags && size > 0) {
        FVASSERT_DEBUG(data);

        const uint8_t* cdata = reinterpret_cast<const uint8_t*>(data);
        auto buffer = std::make_shared<std::vector<uint8_t>>(&cdata[0], &cdata[size]);

        EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
            if (state.pipelineState) {
                vkCmdPushConstants(cbuffer,
                                   state.pipelineState->layout,
                                   stageFlags,
                                   offset, size,
                                   buffer->data());
            }
        };
        encoder->commands.push_back(command);
    }
}

void VulkanComputeCommandEncoder::dispatch(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ) {
    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        vkCmdDispatch(cbuffer, numGroupsX, numGroupsY, numGroupsZ);
    };
    encoder->commands.push_back(command);
}

#endif //#if FVCORE_ENABLE_VULKAN
