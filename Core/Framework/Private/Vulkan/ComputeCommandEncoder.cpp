#include "ComputeCommandEncoder.h"
#include "ComputePipelineState.h"
#include "ShaderBindingSet.h"
#include "GraphicsDevice.h"
#include "Semaphore.h"
#include "TimelineSemaphore.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

ComputeCommandEncoder::Encoder::Encoder(CommandBuffer* cb)
    : cbuffer(cb) {
    commands.reserve(InitialNumberOfCommands);
    setupCommands.reserve(InitialNumberOfCommands);
    cleanupCommands.reserve(InitialNumberOfCommands);
}

ComputeCommandEncoder::Encoder::~Encoder() {
}

bool ComputeCommandEncoder::Encoder::encode(VkCommandBuffer commandBuffer) {
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
        Image* image = pair.first;
        VkImageLayout layout = pair.second;
        VkAccessFlags2 accessMask = Image::commonLayoutAccessMask(layout);

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

ComputeCommandEncoder::ComputeCommandEncoder(std::shared_ptr<CommandBuffer> cb)
    : cbuffer(cb) {
    encoder = std::make_shared<Encoder>(cb.get());
}

void ComputeCommandEncoder::endEncoding() {
    cbuffer->endEncoder(this, encoder);
    encoder = nullptr;
}

void ComputeCommandEncoder::waitEvent(std::shared_ptr<FV::GPUEvent> event) {
    auto semaphore = std::dynamic_pointer_cast<Semaphore>(event);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    encoder->addWaitSemaphore(semaphore->semaphore, semaphore->nextWaitValue(), pipelineStages);
    encoder->events.push_back(semaphore);
}

void ComputeCommandEncoder::signalEvent(std::shared_ptr<FV::GPUEvent> event) {
    auto semaphore = std::dynamic_pointer_cast<Semaphore>(event);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    encoder->addSignalSemaphore(semaphore->semaphore, semaphore->nextSignalValue(), pipelineStages);
    encoder->events.push_back(semaphore);
}

void ComputeCommandEncoder::waitSemaphoreValue(std::shared_ptr<FV::GPUSemaphore> sema, uint64_t value) {
    auto semaphore = std::dynamic_pointer_cast<TimelineSemaphore>(sema);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    encoder->addWaitSemaphore(semaphore->semaphore, value, pipelineStages);
    encoder->semaphores.push_back(semaphore);
}

void ComputeCommandEncoder::signalSemaphoreValue(std::shared_ptr<FV::GPUSemaphore> sema, uint64_t value) {
    auto semaphore = std::dynamic_pointer_cast<TimelineSemaphore>(sema);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    encoder->addSignalSemaphore(semaphore->semaphore, value, pipelineStages);
    encoder->semaphores.push_back(semaphore);
}

void ComputeCommandEncoder::setResources(uint32_t index, std::shared_ptr<FV::ShaderBindingSet> set) {
    std::shared_ptr<DescriptorSet> descriptorSet = nullptr;
    if (set) {
        auto bindingSet = std::dynamic_pointer_cast<ShaderBindingSet>(set);
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

void ComputeCommandEncoder::setComputePipelineState(std::shared_ptr<FV::ComputePipelineState> ps) {
    auto pipeline = std::dynamic_pointer_cast<ComputePipelineState>(ps);
    FVASSERT_DEBUG(pipeline);

    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        vkCmdBindPipeline(cbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
        state.pipelineState = pipeline.get();
    };
    encoder->commands.push_back(command);
    encoder->pipelineStateObjects.push_back(pipeline);
}

void ComputeCommandEncoder::pushConstant(uint32_t stages, uint32_t offset, uint32_t size, const void* data) {
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

void ComputeCommandEncoder::dispatch(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ) {
    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        vkCmdDispatch(cbuffer, numGroupsX, numGroupsY, numGroupsZ);
    };
    encoder->commands.push_back(command);
}

#endif //#if FVCORE_ENABLE_VULKAN
