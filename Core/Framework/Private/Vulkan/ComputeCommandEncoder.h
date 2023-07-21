#pragma once
#include <memory>
#include <vector>
#include "../../ComputeCommandEncoder.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "CommandBuffer.h"
#include "ComputePipelineState.h"
#include "ShaderBindingSet.h"
#include "Semaphore.h"
#include "TimelineSemaphore.h"

namespace FV::Vulkan
{
	class ComputeCommandEncoder : public FV::ComputeCommandEncoder
	{
        class Encoder;
        struct EncodingState
        {
            Encoder* encoder;
            ComputePipelineState* pipelineState;
            DescriptorSet::ImageLayoutMap imageLayoutMap;
            DescriptorSet::ImageViewLayoutMap imageViewLayoutMap;
        };
        using EncoderCommand = std::function<void(VkCommandBuffer, EncodingState&)>;
        class Encoder : public Vulkan::CommandEncoder
        {
        public:
            Encoder(CommandBuffer*);
            ~Encoder();

            bool encode(VkCommandBuffer) override;

            // Retain ownership of all encoded objects
            std::vector<std::shared_ptr<ComputePipelineState>> pipelineStateObjects;
            std::vector<std::shared_ptr<DescriptorSet>> descriptorSets;
            std::vector<std::shared_ptr<Semaphore>> events;
            std::vector<std::shared_ptr<TimelineSemaphore>> semaphores;

            CommandBuffer* cbuffer;
            std::vector<EncoderCommand> commands;
            std::vector<EncoderCommand> setupCommands;
            std::vector<EncoderCommand> cleanupCommands;
        };
        std::shared_ptr<Encoder> encoder;

	public:
		ComputeCommandEncoder(std::shared_ptr<CommandBuffer>);

        void endEncoding() override;
        bool isCompleted() const override { return encoder == nullptr; }
        std::shared_ptr<FV::CommandBuffer> commandBuffer() override { return cbuffer; }

        void waitEvent(std::shared_ptr<FV::GPUEvent>) override;
        void signalEvent(std::shared_ptr<FV::GPUEvent>) override;
        void waitSemaphoreValue(std::shared_ptr<FV::GPUSemaphore>, uint64_t) override;
        void signalSemaphoreValue(std::shared_ptr<FV::GPUSemaphore>, uint64_t) override;

        void setResources(uint32_t set, std::shared_ptr<FV::ShaderBindingSet>) override;
        void setComputePipelineState(std::shared_ptr<FV::ComputePipelineState>) override;

        void pushConstant(uint32_t stages, uint32_t offset, uint32_t size, const void*) override;

        void dispatch(uint32_t, uint32_t, uint32_t) override;

		std::shared_ptr<CommandBuffer> cbuffer;
	};
}
#endif //#if FVCORE_ENABLE_VULKAN
