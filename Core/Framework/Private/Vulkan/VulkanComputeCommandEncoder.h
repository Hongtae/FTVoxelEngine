#pragma once
#include <memory>
#include <vector>
#include "../../ComputeCommandEncoder.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanCommandBuffer.h"
#include "VulkanComputePipelineState.h"
#include "VulkanShaderBindingSet.h"
#include "VulkanSemaphore.h"
#include "VulkanTimelineSemaphore.h"

namespace FV {
    class VulkanComputeCommandEncoder : public ComputeCommandEncoder {
        class Encoder;
        struct EncodingState {
            Encoder* encoder;
            VulkanComputePipelineState* pipelineState;
            VulkanDescriptorSet::ImageLayoutMap imageLayoutMap;
            VulkanDescriptorSet::ImageViewLayoutMap imageViewLayoutMap;
        };
        using EncoderCommand = std::function<void(VkCommandBuffer, EncodingState&)>;
        class Encoder : public VulkanCommandEncoder {
        public:
            Encoder(VulkanCommandBuffer*);
            ~Encoder();

            bool encode(VkCommandBuffer) override;

            // Retain ownership of all encoded objects
            std::vector<std::shared_ptr<VulkanComputePipelineState>> pipelineStateObjects;
            std::vector<std::shared_ptr<VulkanDescriptorSet>> descriptorSets;
            std::vector<std::shared_ptr<VulkanSemaphore>> events;
            std::vector<std::shared_ptr<VulkanTimelineSemaphore>> semaphores;

            VulkanCommandBuffer* cbuffer;
            std::vector<EncoderCommand> commands;
            std::vector<EncoderCommand> setupCommands;
            std::vector<EncoderCommand> cleanupCommands;
        };
        std::shared_ptr<Encoder> encoder;

    public:
        VulkanComputeCommandEncoder(std::shared_ptr<VulkanCommandBuffer>);

        void endEncoding() override;
        bool isCompleted() const override { return encoder == nullptr; }
        std::shared_ptr<CommandBuffer> commandBuffer() override { return cbuffer; }

        void waitEvent(std::shared_ptr<GPUEvent>) override;
        void signalEvent(std::shared_ptr<GPUEvent>) override;
        void waitSemaphoreValue(std::shared_ptr<GPUSemaphore>, uint64_t) override;
        void signalSemaphoreValue(std::shared_ptr<GPUSemaphore>, uint64_t) override;

        void setResource(uint32_t set, std::shared_ptr<ShaderBindingSet>) override;
        void setComputePipelineState(std::shared_ptr<ComputePipelineState>) override;

        void pushConstant(uint32_t stages, uint32_t offset, uint32_t size, const void*) override;

        void dispatch(uint32_t, uint32_t, uint32_t) override;

        std::shared_ptr<VulkanCommandBuffer> cbuffer;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
