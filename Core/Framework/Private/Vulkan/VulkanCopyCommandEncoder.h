#pragma once
#include <memory>
#include <vector>
#include "../../CopyCommandEncoder.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanCommandBuffer.h"
#include "VulkanBufferView.h"
#include "VulkanImageView.h"
#include "VulkanSemaphore.h"
#include "VulkanTimelineSemaphore.h"

namespace FV {
    class VulkanCommandBuffer;
    class VulkanCopyCommandEncoder : public CopyCommandEncoder {
        class Encoder;
        struct EncodingState {
            Encoder* encoder;
        };
        using EncoderCommand = std::function<void(VkCommandBuffer, EncodingState&)>;
        class Encoder : public VulkanCommandEncoder {
        public:
            Encoder(VulkanCommandBuffer*);
            ~Encoder();

            bool encode(VkCommandBuffer) override;

            // Retain ownership of all encoded objects
            std::vector<std::shared_ptr<VulkanBufferView>> buffers;
            std::vector<std::shared_ptr<VulkanImageView>> textures;
            std::vector<std::shared_ptr<VulkanSemaphore>> events;
            std::vector<std::shared_ptr<VulkanTimelineSemaphore>> semaphores;

            class VulkanCommandBuffer* cbuffer;
            std::vector<EncoderCommand> commands;
            std::vector<EncoderCommand> setupCommands;
            std::vector<EncoderCommand> cleanupCommands;
        };
        std::shared_ptr<Encoder> encoder;

    public:
        VulkanCopyCommandEncoder(std::shared_ptr<VulkanCommandBuffer>);

        void endEncoding() override;
        bool isCompleted() const override { return encoder == nullptr; }
        std::shared_ptr<CommandBuffer> commandBuffer() override { return cbuffer; }

        void waitEvent(std::shared_ptr<GPUEvent>) override;
        void signalEvent(std::shared_ptr<GPUEvent>) override;
        void waitSemaphoreValue(std::shared_ptr<GPUSemaphore>, uint64_t) override;
        void signalSemaphoreValue(std::shared_ptr<GPUSemaphore>, uint64_t) override;

        void copy(std::shared_ptr<GPUBuffer> src,
                  size_t srcOffset,
                  std::shared_ptr<GPUBuffer> dst,
                  size_t dstOffset,
                  size_t size) override;

        void copy(std::shared_ptr<GPUBuffer> src,
                  const BufferImageOrigin& srcOffset,
                  std::shared_ptr<Texture> dst,
                  const TextureOrigin& dstOffset,
                  const TextureSize& size) override;

        void copy(std::shared_ptr<Texture> src,
                  const TextureOrigin& srcOffset,
                  std::shared_ptr<GPUBuffer> dst,
                  const BufferImageOrigin& dstOffset,
                  const TextureSize& size) override;

        void copy(std::shared_ptr<Texture> src,
                  const TextureOrigin& srcOffset,
                  std::shared_ptr<Texture> dst,
                  const TextureOrigin& dstOffset,
                  const TextureSize& size) override;

        void fill(std::shared_ptr<GPUBuffer> buffer,
                  size_t offset,
                  size_t length,
                  uint8_t value) override;

        void callback(std::function<void(VkCommandBuffer)>);

        std::shared_ptr<VulkanCommandBuffer> cbuffer;

    private:
        static void setupSubresource(const TextureOrigin& origin,
                                     uint32_t layerCount,
                                     PixelFormat pixelFormat,
                                     VkImageSubresourceLayers& subresource);
        static void setupSubresource(const TextureOrigin& origin,
                                     uint32_t layerCount,
                                     uint32_t levelCount,
                                     PixelFormat pixelFormat,
                                     VkImageSubresourceRange& subresource);
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
