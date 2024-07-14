#pragma once
#include "../../RenderCommandEncoder.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanCommandBuffer.h"
#include "VulkanSwapChain.h"
#include "VulkanRenderPipelineState.h"
#include "VulkanShaderBindingSet.h"
#include "VulkanDescriptorSet.h"
#include "VulkanSemaphore.h"
#include "VulkanTimelineSemaphore.h"

namespace FV {
    class VulkanRenderCommandEncoder : public RenderCommandEncoder {
        class Encoder;
        struct EncodingState {
            Encoder* encoder;
            VulkanRenderPipelineState* pipelineState;
            DepthStencilState* depthStencilState;
            VulkanDescriptorSet::ImageLayoutMap imageLayoutMap;
            VulkanDescriptorSet::ImageViewLayoutMap imageViewLayoutMap;
        };
        using EncoderCommand = std::function<void(VkCommandBuffer, EncodingState&)>;
        class Encoder : public VulkanCommandEncoder {
        public:
            Encoder(class VulkanCommandBuffer*, const RenderPassDescriptor& desc);
            ~Encoder();

            bool encode(VkCommandBuffer) override;

            // Retain ownership of all encoded objects
            std::vector<std::shared_ptr<VulkanRenderPipelineState>> pipelineStateObjects;
            std::vector<std::shared_ptr<VulkanDescriptorSet>> descriptorSets;
            std::vector<std::shared_ptr<VulkanBufferView>> buffers;
            std::vector<std::shared_ptr<VulkanSemaphore>> events;
            std::vector<std::shared_ptr<VulkanTimelineSemaphore>> semaphores;

            RenderPassDescriptor renderPassDescriptor;

            VkFramebuffer framebuffer;
            VkRenderPass renderPass;

            class VulkanCommandBuffer* cbuffer;
            std::vector<EncoderCommand> commands;
            std::vector<EncoderCommand> setupCommands;    // before renderPass
            std::vector<EncoderCommand> cleanupCommands;  // after renderPass

            uint32_t drawCount = 0;
            std::unordered_set<VkDynamicState> setDynamicStates;
        };
        std::shared_ptr<Encoder> encoder;

    public:
        VulkanRenderCommandEncoder(std::shared_ptr<VulkanCommandBuffer>, const RenderPassDescriptor&);

        void endEncoding() override;
        bool isCompleted() const override { return encoder == nullptr; }
        std::shared_ptr<CommandBuffer> commandBuffer() override { return cbuffer; }

        void waitEvent(std::shared_ptr<GPUEvent>) override;
        void signalEvent(std::shared_ptr<GPUEvent>) override;
        void waitSemaphoreValue(std::shared_ptr<GPUSemaphore>, uint64_t) override;
        void signalSemaphoreValue(std::shared_ptr<GPUSemaphore>, uint64_t) override;

        void setResource(uint32_t set, std::shared_ptr<ShaderBindingSet>) override;

        void setViewport(const Viewport&) override;
        void setScissorRect(const ScissorRect&) override;
        void setRenderPipelineState(std::shared_ptr<RenderPipelineState>) override;
        void setVertexBuffer(std::shared_ptr<GPUBuffer> buffer, size_t offset, uint32_t index) override;
        void setVertexBuffers(std::shared_ptr<GPUBuffer>* buffers, const size_t* offsets, uint32_t index, size_t count) override;

        void setDepthStencilState(std::shared_ptr<DepthStencilState>) override;
        void setDepthClipMode(DepthClipMode) override;
        void setCullMode(CullMode) override;
        void setFrontFacing(Winding) override;

        void setBlendColor(float r, float g, float b, float a) override;
        void setStencilReferenceValue(uint32_t) override;
        void setStencilReferenceValues(uint32_t front, uint32_t back) override;
        void setDepthBias(float depthBias, float slopeScale, float clamp) override;

        void pushConstant(uint32_t stages, uint32_t offset, uint32_t size, const void*) override;

        void memoryBarrier(RenderStages after, RenderStages before) override;

        void draw(uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t baseInstance) override;
        void drawIndexed(uint32_t indexCount, IndexType indexType, std::shared_ptr<GPUBuffer> indexBuffer, uint32_t indexBufferOffset, uint32_t instanceCount, uint32_t baseVertex, uint32_t baseInstance) override;

        std::shared_ptr<VulkanCommandBuffer> cbuffer;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
