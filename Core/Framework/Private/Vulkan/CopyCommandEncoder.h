#pragma once
#include <memory>
#include <vector>
#include "../../CopyCommandEncoder.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "CommandBuffer.h"
#include "BufferView.h"
#include "ImageView.h"
#include "Semaphore.h"
#include "TimelineSemaphore.h"

namespace FV::Vulkan {
    class CommandBuffer;
    class CopyCommandEncoder : public FV::CopyCommandEncoder {
        class Encoder;
        struct EncodingState {
            Encoder* encoder;
        };
        using EncoderCommand = std::function<void(VkCommandBuffer, EncodingState&)>;
        class Encoder : public Vulkan::CommandEncoder {
        public:
            Encoder(CommandBuffer*);
            ~Encoder();

            bool encode(VkCommandBuffer) override;

            // Retain ownership of all encoded objects
            std::vector<std::shared_ptr<BufferView>> buffers;
            std::vector<std::shared_ptr<ImageView>> textures;
            std::vector<std::shared_ptr<Semaphore>> events;
            std::vector<std::shared_ptr<TimelineSemaphore>> semaphores;

            class CommandBuffer* cbuffer;
            std::vector<EncoderCommand> commands;
            std::vector<EncoderCommand> setupCommands;
            std::vector<EncoderCommand> cleanupCommands;
        };
        std::shared_ptr<Encoder> encoder;

    public:
        CopyCommandEncoder(std::shared_ptr<CommandBuffer>);

        void endEncoding() override;
        bool isCompleted() const override { return encoder == nullptr; }
        std::shared_ptr<FV::CommandBuffer> commandBuffer() override { return cbuffer; }

        void waitEvent(std::shared_ptr<FV::GPUEvent>) override;
        void signalEvent(std::shared_ptr<FV::GPUEvent>) override;
        void waitSemaphoreValue(std::shared_ptr<FV::GPUSemaphore>, uint64_t) override;
        void signalSemaphoreValue(std::shared_ptr<FV::GPUSemaphore>, uint64_t) override;

        void copy(std::shared_ptr<FV::GPUBuffer> src,
                  size_t srcOffset,
                  std::shared_ptr<FV::GPUBuffer> dst,
                  size_t dstOffset,
                  size_t size) override;

        void copy(std::shared_ptr<FV::GPUBuffer> src,
                  const BufferImageOrigin& srcOffset,
                  std::shared_ptr<FV::Texture> dst,
                  const TextureOrigin& dstOffset,
                  const TextureSize& size) override;

        void copy(std::shared_ptr<FV::Texture> src,
                  const TextureOrigin& srcOffset,
                  std::shared_ptr<FV::GPUBuffer> dst,
                  const BufferImageOrigin& dstOffset,
                  const TextureSize& size) override;

        void copy(std::shared_ptr<FV::Texture> src,
                  const TextureOrigin& srcOffset,
                  std::shared_ptr<FV::Texture> dst,
                  const TextureOrigin& dstOffset,
                  const TextureSize& size) override;

        void fill(std::shared_ptr<FV::GPUBuffer> buffer,
                  size_t offset,
                  size_t length,
                  uint8_t value) override;

        void callback(std::function<void(VkCommandBuffer)>);

        std::shared_ptr<CommandBuffer> cbuffer;

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
