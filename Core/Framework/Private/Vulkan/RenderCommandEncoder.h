#pragma once
#include "../../RenderCommandEncoder.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "CommandBuffer.h"
#include "SwapChain.h"
#include "RenderPipelineState.h"
#include "ShaderBindingSet.h"
#include "DescriptorSet.h"
#include "Semaphore.h"
#include "TimelineSemaphore.h"

namespace FV::Vulkan {
    class RenderCommandEncoder : public FV::RenderCommandEncoder {
        class Encoder;
        struct EncodingState {
            Encoder* encoder;
            RenderPipelineState* pipelineState;
            DepthStencilState* depthStencilState;
            DescriptorSet::ImageLayoutMap imageLayoutMap;
            DescriptorSet::ImageViewLayoutMap imageViewLayoutMap;
        };
        using EncoderCommand = std::function<void(VkCommandBuffer, EncodingState&)>;
        class Encoder : public Vulkan::CommandEncoder {
        public:
            Encoder(class CommandBuffer*, const RenderPassDescriptor& desc);
            ~Encoder();

            bool encode(VkCommandBuffer) override;

            // Retain ownership of all encoded objects
            std::vector<std::shared_ptr<RenderPipelineState>> pipelineStateObjects;
            std::vector<std::shared_ptr<DescriptorSet>> descriptorSets;
            std::vector<std::shared_ptr<BufferView>> buffers;
            std::vector<std::shared_ptr<Semaphore>> events;
            std::vector<std::shared_ptr<TimelineSemaphore>> semaphores;

            RenderPassDescriptor renderPassDescriptor;

            VkFramebuffer framebuffer;
            VkRenderPass renderPass;

            class CommandBuffer* cbuffer;
            std::vector<EncoderCommand> commands;
            std::vector<EncoderCommand> setupCommands;    // before renderPass
            std::vector<EncoderCommand> cleanupCommands;  // after renderPass
        };
        std::shared_ptr<Encoder> encoder;

    public:
        RenderCommandEncoder(std::shared_ptr<CommandBuffer>, const RenderPassDescriptor&);

        void endEncoding() override;
        bool isCompleted() const override { return encoder == nullptr; }
        std::shared_ptr<FV::CommandBuffer> commandBuffer() override { return cbuffer; }

        void waitEvent(std::shared_ptr<FV::GPUEvent>) override;
        void signalEvent(std::shared_ptr<FV::GPUEvent>) override;
        void waitSemaphoreValue(std::shared_ptr<FV::GPUSemaphore>, uint64_t) override;
        void signalSemaphoreValue(std::shared_ptr<FV::GPUSemaphore>, uint64_t) override;

        void setResource(uint32_t set, std::shared_ptr<FV::ShaderBindingSet>) override;

        void setViewport(const Viewport&) override;
        void setScissorRect(const ScissorRect&) override;
        void setRenderPipelineState(std::shared_ptr<FV::RenderPipelineState>) override;
        void setVertexBuffer(std::shared_ptr<FV::GPUBuffer> buffer, size_t offset, uint32_t index) override;
        void setVertexBuffers(std::shared_ptr<FV::GPUBuffer>* buffers, const size_t* offsets, uint32_t index, size_t count) override;

        void setDepthStencilState(std::shared_ptr<FV::DepthStencilState>) override;
        void setDepthClipMode(DepthClipMode) override;
        void setCullMode(CullMode) override;
        void setFrontFacing(Winding) override;

        void setBlendColor(float r, float g, float b, float a) override;
        void setStencilReferenceValue(uint32_t) override;
        void setStencilReferenceValues(uint32_t front, uint32_t back) override;
        void setDepthBias(float depthBias, float slopeScale, float clamp) override;

        void pushConstant(uint32_t stages, uint32_t offset, uint32_t size, const void*) override;

        void draw(uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t baseInstance) override;
        void drawIndexed(uint32_t indexCount, IndexType indexType, std::shared_ptr<GPUBuffer> indexBuffer, uint32_t indexBufferOffset, uint32_t instanceCount, uint32_t baseVertex, uint32_t baseInstance) override;

        std::shared_ptr<CommandBuffer> cbuffer;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
