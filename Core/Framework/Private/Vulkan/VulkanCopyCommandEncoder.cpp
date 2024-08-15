#include "VulkanCopyCommandEncoder.h"
#include "VulkanBufferView.h"
#include "VulkanImageView.h"
#include "VulkanGraphicsDevice.h"
#include "VulkanSemaphore.h"
#include "VulkanTimelineSemaphore.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanCopyCommandEncoder::Encoder::Encoder(VulkanCommandBuffer* cb)
    : cbuffer(cb) {
    commands.reserve(InitialNumberOfCommands);
    setupCommands.reserve(InitialNumberOfCommands);
    cleanupCommands.reserve(InitialNumberOfCommands);
}

VulkanCopyCommandEncoder::Encoder::~Encoder() {
}

bool VulkanCopyCommandEncoder::Encoder::encode(VkCommandBuffer commandBuffer) {
    // recording commands
    EncodingState state = { this };
    for (EncoderCommand& cmd : setupCommands) {
        cmd(commandBuffer, state);
    }
    for (EncoderCommand& cmd : commands) {
        cmd(commandBuffer, state);
    }
    for (EncoderCommand& cmd : cleanupCommands) {
        cmd(commandBuffer, state);
    }
    return true;
}

VulkanCopyCommandEncoder::VulkanCopyCommandEncoder(std::shared_ptr<VulkanCommandBuffer> cb)
    : cbuffer(cb) {
    encoder = std::make_shared<Encoder>(cb.get());
}

void VulkanCopyCommandEncoder::endEncoding() {
    cbuffer->endEncoder(this, encoder);
    encoder = nullptr;
}

void VulkanCopyCommandEncoder::waitEvent(std::shared_ptr<GPUEvent> event) {
    auto semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(event);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;

    encoder->addWaitSemaphore(semaphore->semaphore, semaphore->nextWaitValue(), pipelineStages);
    encoder->events.push_back(semaphore);
}

void VulkanCopyCommandEncoder::signalEvent(std::shared_ptr<GPUEvent> event) {
    auto semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(event);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;

    encoder->addSignalSemaphore(semaphore->semaphore, semaphore->nextSignalValue(), pipelineStages);
    encoder->events.push_back(semaphore);
}

void VulkanCopyCommandEncoder::waitSemaphoreValue(std::shared_ptr<GPUSemaphore> sema, uint64_t value) {
    auto semaphore = std::dynamic_pointer_cast<VulkanTimelineSemaphore>(sema);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;

    encoder->addWaitSemaphore(semaphore->semaphore, value, pipelineStages);
    encoder->semaphores.push_back(semaphore);
}

void VulkanCopyCommandEncoder::signalSemaphoreValue(std::shared_ptr<GPUSemaphore> sema, uint64_t value) {
    auto semaphore = std::dynamic_pointer_cast<VulkanTimelineSemaphore>(sema);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;

    encoder->addSignalSemaphore(semaphore->semaphore, value, pipelineStages);
    encoder->semaphores.push_back(semaphore);
}

void VulkanCopyCommandEncoder::waitSemaphore(VkSemaphore semaphore, uint64_t value, VkPipelineStageFlags2 flags) {
    FVASSERT_DEBUG(semaphore);
    encoder->addWaitSemaphore(semaphore, value, flags);
}

void VulkanCopyCommandEncoder::signalSemaphore(VkSemaphore semaphore, uint64_t value, VkPipelineStageFlags2 flags) {
    FVASSERT_DEBUG(semaphore);
    encoder->addSignalSemaphore(semaphore, value, flags);
}

void VulkanCopyCommandEncoder::copy(
    std::shared_ptr<GPUBuffer> src, size_t srcOffset,
    std::shared_ptr<GPUBuffer> dst, size_t dstOffset, size_t size) {
    auto srcView = std::dynamic_pointer_cast<VulkanBufferView>(src);
    auto dstView = std::dynamic_pointer_cast<VulkanBufferView>(dst);
    FVASSERT_DEBUG(srcView);
    FVASSERT_DEBUG(dstView);

    auto& srcBuffer = srcView->buffer;
    auto& dstBuffer = dstView->buffer;

    FVASSERT_DEBUG(srcBuffer && srcBuffer->buffer);
    FVASSERT_DEBUG(dstBuffer && dstBuffer->buffer);

    size_t srcLength = srcBuffer->length();
    size_t dstLength = dstBuffer->length();

    if (srcOffset + size > srcLength || dstOffset + size > dstLength) {
        Log::error("CopyCommandEncoder::copy failed: Invalid buffer region");
        return;
    }

    VkBufferCopy region = { srcOffset, dstOffset, size };

    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        vkCmdCopyBuffer(cbuffer,
                        srcBuffer->buffer,
                        dstBuffer->buffer,
                        1, &region);
    };
    encoder->commands.push_back(command);
    encoder->buffers.push_back(srcView);
    encoder->buffers.push_back(dstView);
}

void VulkanCopyCommandEncoder::copy(
    std::shared_ptr<GPUBuffer> src, const BufferImageOrigin& srcOffset,
    std::shared_ptr<Texture> dst, const TextureOrigin& dstOffset,
    const TextureSize& size) {
    auto srcView = std::dynamic_pointer_cast<VulkanBufferView>(src);
    auto dstView = std::dynamic_pointer_cast<VulkanImageView>(dst);
    FVASSERT_DEBUG(srcView);
    FVASSERT_DEBUG(dstView);

    FVASSERT_DEBUG((srcOffset.bufferOffset % 4) == 0);

    auto& buffer = srcView->buffer;
    auto& image = dstView->image;

    FVASSERT_DEBUG(buffer && buffer->buffer);
    FVASSERT_DEBUG(image && image->image);

    const TextureSize mipDimensions = {
        std::max(image->width() >> dstOffset.level, 1U),
        std::max(image->height() >> dstOffset.level, 1U),
        std::max(image->depth() >> dstOffset.level, 1U)
    };

    if (dstOffset.x + size.width > mipDimensions.width ||
        dstOffset.y + size.height > mipDimensions.height ||
        dstOffset.z + size.depth > mipDimensions.depth) {
        Log::error("CopyCommandEncoder::copy failed: Invalid texture region");
        return;
    }
    if (size.width > srcOffset.imageWidth || size.height > srcOffset.imageHeight) {
        Log::error("CopyCommandEncoder::copy failed: Invalid buffer region");
        return;
    }

    PixelFormat pixelFormat = image->pixelFormat();
    size_t bufferLength = buffer->length();
    size_t bytesPerPixel = pixelFormatBytesPerPixel(pixelFormat);
    FVASSERT_DEBUG(bytesPerPixel > 0);      // Unsupported texture format!

    size_t requiredBufferLengthForCopy = size_t(srcOffset.imageWidth) * size_t(srcOffset.imageHeight) * size.depth * bytesPerPixel + srcOffset.bufferOffset;
    if (requiredBufferLengthForCopy > bufferLength) {
        Log::error("CopyCommandEncoder::copy failed: buffer is too small!");
        return;
    }

    VkBufferImageCopy region = {};
    region.bufferOffset = srcOffset.bufferOffset;
    region.bufferRowLength = srcOffset.imageWidth;
    region.bufferImageHeight = srcOffset.imageHeight;
    region.imageOffset = { (int32_t)dstOffset.x, (int32_t)dstOffset.y,(int32_t)dstOffset.z };
    region.imageExtent = { size.width, size.height, size.depth };
    setupSubresource(dstOffset, 1, pixelFormat, region.imageSubresource);

    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        image->setLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_ACCESS_2_TRANSFER_WRITE_BIT,
                         VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                         state.encoder->cbuffer->queueFamily()->familyIndex,
                         cbuffer);

        vkCmdCopyBufferToImage(cbuffer,
                               buffer->buffer,
                               image->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &region);
    };
    encoder->commands.push_back(command);
    encoder->buffers.push_back(srcView);
    encoder->textures.push_back(dstView);
}

void VulkanCopyCommandEncoder::copy(std::shared_ptr<Texture> src,
                              const TextureOrigin& srcOffset,
                              std::shared_ptr<GPUBuffer> dst,
                              const BufferImageOrigin& dstOffset,
                              const TextureSize& size) {
    auto srcView = std::dynamic_pointer_cast<VulkanImageView>(src);
    auto dstView = std::dynamic_pointer_cast<VulkanBufferView>(dst);
    FVASSERT_DEBUG(srcView);
    FVASSERT_DEBUG(dstView);

    FVASSERT_DEBUG((dstOffset.bufferOffset % 4) == 0);

    auto& image = srcView->image;
    auto& buffer = dstView->buffer;

    FVASSERT_DEBUG(buffer && buffer->buffer);
    FVASSERT_DEBUG(image && image->image);

    const TextureSize mipDimensions = {
        std::max(image->width() >> srcOffset.level, 1U),
        std::max(image->height() >> srcOffset.level, 1U),
        std::max(image->depth() >> srcOffset.level, 1U)
    };

    if (srcOffset.x + size.width > mipDimensions.width ||
        srcOffset.y + size.height > mipDimensions.height ||
        srcOffset.z + size.depth > mipDimensions.depth) {
        Log::error("CopyCommandEncoder::copy failed: Invalid texture region");
        return;
    }
    if (size.width > dstOffset.imageWidth || size.height > dstOffset.imageHeight) {
        Log::error("CopyCommandEncoder::copy failed: Invalid buffer region");
        return;
    }

    PixelFormat pixelFormat = image->pixelFormat();
    size_t bufferLength = buffer->length();
    size_t bytesPerPixel = pixelFormatBytesPerPixel(pixelFormat);
    FVASSERT_DEBUG(bytesPerPixel > 0);      // Unsupported texture format!

    size_t requiredBufferLengthForCopy = size_t(dstOffset.imageWidth) * size_t(dstOffset.imageHeight) * size.depth * bytesPerPixel + dstOffset.bufferOffset;
    if (requiredBufferLengthForCopy > bufferLength) {
        Log::error("CopyCommandEncoder::copy failed: buffer is too small!");
        return;
    }

    VkBufferImageCopy region = {};
    region.bufferOffset = dstOffset.bufferOffset;
    region.bufferRowLength = dstOffset.imageWidth;
    region.bufferImageHeight = dstOffset.imageHeight;
    region.imageOffset = { (int32_t)srcOffset.x,(int32_t)srcOffset.y, (int32_t)srcOffset.z };
    region.imageExtent = { size.width, size.height,size.depth };
    setupSubresource(srcOffset, 1, pixelFormat, region.imageSubresource);

    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        image->setLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_ACCESS_2_TRANSFER_READ_BIT,
                         VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                         state.encoder->cbuffer->queueFamily()->familyIndex,
                         cbuffer);

        vkCmdCopyImageToBuffer(cbuffer,
                               image->image,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               buffer->buffer,
                               1, &region);
    };
    encoder->commands.push_back(command);
    encoder->textures.push_back(srcView);
    encoder->buffers.push_back(dstView);
}

void VulkanCopyCommandEncoder::copy(std::shared_ptr<Texture> src,
                              const TextureOrigin& srcOffset,
                              std::shared_ptr<Texture> dst,
                              const TextureOrigin& dstOffset,
                              const TextureSize& size) {
    auto srcView = std::dynamic_pointer_cast<VulkanImageView>(src);
    auto dstView = std::dynamic_pointer_cast<VulkanImageView>(dst);
    FVASSERT_DEBUG(srcView);
    FVASSERT_DEBUG(dstView);

    auto& srcImage = srcView->image;
    auto& dstImage = dstView->image;

    FVASSERT_DEBUG(srcImage && srcImage->image);
    FVASSERT_DEBUG(dstImage && dstImage->image);

    const TextureSize srcMipDimensions = {
        std::max(srcImage->width() >> srcOffset.level, 1U),
        std::max(srcImage->height() >> srcOffset.level, 1U),
        std::max(srcImage->depth() >> srcOffset.level, 1U)
    };
    const TextureSize dstMipDimensions = {
        std::max(dstImage->width() >> dstOffset.level, 1U),
        std::max(dstImage->height() >> dstOffset.level, 1U),
        std::max(dstImage->depth() >> dstOffset.level, 1U)
    };

    if (srcOffset.x + size.width > srcMipDimensions.width ||
        srcOffset.y + size.height > srcMipDimensions.height ||
        srcOffset.z + size.depth > srcMipDimensions.depth) {
        Log::error("CopyCommandEncoder::copy failed: Invalid source texture region");
        return;
    }
    if (dstOffset.x + size.width > dstMipDimensions.width ||
        dstOffset.y + size.height > dstMipDimensions.height ||
        dstOffset.z + size.depth > dstMipDimensions.depth) {
        Log::error("CopyCommandEncoder::copy failed: Invalid destination texture region");
        return;
    }

    PixelFormat srcPixelFormat = srcImage->pixelFormat();
    PixelFormat dstPixelFormat = dstImage->pixelFormat();
    size_t srcBytesPerPixel = pixelFormatBytesPerPixel(srcPixelFormat);
    size_t dstBytesPerPixel = pixelFormatBytesPerPixel(dstPixelFormat);

    FVASSERT_DEBUG(srcBytesPerPixel > 0);      // Unsupported texture format!
    FVASSERT_DEBUG(dstBytesPerPixel > 0);      // Unsupported texture format!

    if (srcBytesPerPixel != dstBytesPerPixel) {
        Log::error("CopyCommandEncoder::copy failed: Incompatible pixel formats");
        return;
    }

    VkImageCopy region = {};
    setupSubresource(srcOffset, 1, srcPixelFormat, region.srcSubresource);
    setupSubresource(dstOffset, 1, dstPixelFormat, region.dstSubresource);

    FVASSERT_DEBUG(region.srcSubresource.aspectMask);
    FVASSERT_DEBUG(region.dstSubresource.aspectMask);

    region.srcOffset = { (int32_t)srcOffset.x, (int32_t)srcOffset.y,(int32_t)srcOffset.z };
    region.dstOffset = { (int32_t)dstOffset.x, (int32_t)dstOffset.y,(int32_t)dstOffset.z };
    region.extent = { size.width, size.height, size.depth };

    VkImageMemoryBarrier imageMemoryBarriers[2] = {
        { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER },
        { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER }
    };
    imageMemoryBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[0].image = srcImage->image;
    setupSubresource(srcOffset, 1, 1, srcPixelFormat, imageMemoryBarriers[0].subresourceRange);

    imageMemoryBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[1].image = dstImage->image;
    setupSubresource(dstOffset, 1, 1, dstPixelFormat, imageMemoryBarriers[1].subresourceRange);

    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        srcImage->setLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            VK_ACCESS_2_TRANSFER_READ_BIT,
                            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                            state.encoder->cbuffer->queueFamily()->familyIndex,
                            cbuffer);

        dstImage->setLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_ACCESS_2_TRANSFER_WRITE_BIT,
                            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                            state.encoder->cbuffer->queueFamily()->familyIndex,
                            cbuffer);

        vkCmdCopyImage(cbuffer,
                       srcImage->image,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       dstImage->image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &region);
    };
    encoder->commands.push_back(command);
    encoder->textures.push_back(srcView);
    encoder->textures.push_back(dstView);
}

void VulkanCopyCommandEncoder::fill(std::shared_ptr<GPUBuffer> buffer,
                              size_t offset, size_t length, uint8_t value) {
    auto bufferView = std::dynamic_pointer_cast<VulkanBufferView>(buffer);
    FVASSERT_DEBUG(bufferView);
    auto& buf = bufferView->buffer;
    FVASSERT_DEBUG(buf && buf->buffer);

    size_t bufferLength = buf->length();
    if (offset + length > bufferLength) {
        Log::error("CopyCommandEncoder::fill failed: Invalid buffer region");
        return;
    }

    uint32_t data = (uint32_t(value) << 24 |
                     uint32_t(value) << 16 |
                     uint32_t(value) << 8 |
                     uint32_t(value));

    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        vkCmdFillBuffer(cbuffer,
                        buf->buffer,
                        static_cast<VkDeviceSize>(offset),
                        static_cast<VkDeviceSize>(length),
                        data);
    };
    encoder->commands.push_back(command);
    encoder->buffers.push_back(bufferView);
}

void VulkanCopyCommandEncoder::callback(std::function<void(VkCommandBuffer)> fn) {
    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        fn(cbuffer);
    };
    encoder->commands.push_back(command);
}

void VulkanCopyCommandEncoder::setupSubresource(const TextureOrigin& origin,
                                          uint32_t layerCount,
                                          PixelFormat pixelFormat,
                                          VkImageSubresourceLayers& subresource) {
    if (isColorFormat(pixelFormat))
        subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    else {
        subresource.aspectMask = 0;
        if (isDepthFormat(pixelFormat))
            subresource.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (isStencilFormat(pixelFormat))
            subresource.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    subresource.mipLevel = origin.level;
    subresource.baseArrayLayer = origin.layer;
    subresource.layerCount = layerCount;
}

void VulkanCopyCommandEncoder::setupSubresource(const TextureOrigin& origin,
                                          uint32_t layerCount,
                                          uint32_t levelCount,
                                          PixelFormat pixelFormat,
                                          VkImageSubresourceRange& subresource) {
    if (isColorFormat(pixelFormat))
        subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    else {
        subresource.aspectMask = 0;
        if (isDepthFormat(pixelFormat))
            subresource.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (isStencilFormat(pixelFormat))
            subresource.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    subresource.baseMipLevel = origin.level;
    subresource.baseArrayLayer = origin.layer;
    subresource.layerCount = layerCount;
    subresource.levelCount = levelCount;
}

#endif //#if FVCORE_ENABLE_VULKAN
