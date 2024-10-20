#include "VulkanRenderCommandEncoder.h"
#include "VulkanBufferView.h"
#include "VulkanImageView.h"
#include "VulkanGraphicsDevice.h"
#include "VulkanRenderPipelineState.h"
#include "VulkanDepthStencilState.h"
#include "VulkanShaderBindingSet.h"
#include "VulkanSemaphore.h"
#include "VulkanTimelineSemaphore.h"

#define FLIP_VIEWPORT_Y	1

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanRenderCommandEncoder::Encoder::Encoder(class VulkanCommandBuffer* cb, const RenderPassDescriptor& desc)
    : cbuffer(cb)
    , renderPassDescriptor(desc)
    , framebuffer(VK_NULL_HANDLE)
    , renderPass(VK_NULL_HANDLE) {
    commands.reserve(InitialNumberOfCommands);
    setupCommands.reserve(InitialNumberOfCommands);
    cleanupCommands.reserve(InitialNumberOfCommands);

    for (const auto& colorAttachment : renderPassDescriptor.colorAttachments) {
        if (auto rt = std::dynamic_pointer_cast<VulkanImageView>(colorAttachment.renderTarget); rt && rt->image) {
            this->addWaitSemaphore(rt->waitSemaphore, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
            this->addSignalSemaphore(rt->signalSemaphore, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
        }
    }
    if (renderPassDescriptor.depthStencilAttachment.renderTarget) {
        const auto& depthStencilAttachment = renderPassDescriptor.depthStencilAttachment;
        if (auto rt = std::dynamic_pointer_cast<VulkanImageView>(depthStencilAttachment.renderTarget); rt && rt->image) {
            this->addWaitSemaphore(rt->waitSemaphore, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
            this->addSignalSemaphore(rt->signalSemaphore, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
        }
    }
}

VulkanRenderCommandEncoder::Encoder::~Encoder() {
    std::shared_ptr<VulkanGraphicsDevice> dev = std::dynamic_pointer_cast<VulkanGraphicsDevice>(cbuffer->device());
    VkDevice device = dev->device;

    if (renderPass)
        vkDestroyRenderPass(device, renderPass, dev->allocationCallbacks());

    if (framebuffer)
        vkDestroyFramebuffer(device, framebuffer, dev->allocationCallbacks());
}

bool VulkanRenderCommandEncoder::Encoder::encode(VkCommandBuffer commandBuffer) {
    EncodingState state = { this };

    // collect image layout transition
    for (auto& ds : descriptorSets) {
        ds->collectImageViewLayouts(state.imageLayoutMap, state.imageViewLayoutMap);
    }
    // process pre-renderpass commands
    for (EncoderCommand& cmd : setupCommands)
        cmd(commandBuffer, state);

    // Set image layout transition
    for (auto& pair : state.imageLayoutMap) {
        VulkanImage* image = pair.first;
        VkImageLayout layout = pair.second;
        VkAccessFlags2 accessMask = VulkanImage::commonLayoutAccessMask(layout);

        image->setLayout(layout,
                         accessMask,
                         VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                         VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                         state.encoder->cbuffer->queueFamily()->familyIndex,
                         commandBuffer);
    }

    // initialize render pass
    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.flags = 0;
    renderingInfo.layerCount = 1;

    uint32_t frameWidth = 0;
    uint32_t frameHeight = 0;

    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    colorAttachments.reserve(renderPassDescriptor.colorAttachments.size());
    
    for (const auto& colorAttachment : renderPassDescriptor.colorAttachments) {
        VkRenderingAttachmentInfo attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        attachment.imageView = VK_NULL_HANDLE;
        attachment.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.resolveMode = VK_RESOLVE_MODE_NONE;
        attachment.resolveImageView = nullptr;
        attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        switch (colorAttachment.loadAction) {
        case RenderPassAttachmentDescriptor::LoadActionLoad:
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            break;
        case RenderPassAttachmentDescriptor::LoadActionClear:
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            break;
        default:
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            break;
        }
        switch (colorAttachment.storeAction) {
        case RenderPassAttachmentDescriptor::StoreActionDontCare:
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            break;
        case RenderPassAttachmentDescriptor::StoreActionStore:
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            break;
        }
        attachment.clearValue.color = {
            colorAttachment.clearColor.r,
            colorAttachment.clearColor.g,
            colorAttachment.clearColor.b,
            colorAttachment.clearColor.a
        };

        if (auto renderTarget = colorAttachment.renderTarget.get()) {
            VulkanImageView* imageView = dynamic_cast<VulkanImageView*>(renderTarget);
            FVASSERT_DEBUG(imageView);

            attachment.imageView = imageView->imageView;
            attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            if (VulkanImage* image = imageView->image.get()) {
                FVASSERT_DEBUG(isColorFormat(image->pixelFormat()));

                frameWidth = (frameWidth > 0) ? std::min(frameWidth, imageView->width()) : imageView->width();
                frameHeight = (frameHeight > 0) ? std::min(frameHeight, imageView->height()) : imageView->height();

                imageView->image->setLayout(
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    state.encoder->cbuffer->queueFamily()->familyIndex,
                    commandBuffer);
            }
        }
        colorAttachments.push_back(attachment);
    }
    if (colorAttachments.empty() == false) {
        renderingInfo.colorAttachmentCount = uint32_t(colorAttachments.size());
        renderingInfo.pColorAttachments = colorAttachments.data();
    }
    VkRenderingAttachmentInfo depthStencilAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

    if (auto imageView = dynamic_cast<const VulkanImageView*>(renderPassDescriptor.depthStencilAttachment.renderTarget.get());
        imageView != nullptr && imageView->image != nullptr) {

        // VUID-VkRenderingInfo-pDepthAttachment-06085
        depthStencilAttachment.imageView = imageView->imageView;
        depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthStencilAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
        depthStencilAttachment.resolveImageView = VK_NULL_HANDLE;
        depthStencilAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        switch (renderPassDescriptor.depthStencilAttachment.loadAction) {
        case RenderPassAttachmentDescriptor::LoadActionLoad:
            depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            break;
        case RenderPassAttachmentDescriptor::LoadActionClear:
            depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            break;
        default:
            depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            break;
        }
        switch (renderPassDescriptor.depthStencilAttachment.storeAction) {
        case RenderPassAttachmentDescriptor::StoreActionStore:
            depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            break;
        default:
            depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            break;
        }
        depthStencilAttachment.clearValue.depthStencil.depth = renderPassDescriptor.depthStencilAttachment.clearDepth;
        depthStencilAttachment.clearValue.depthStencil.stencil = renderPassDescriptor.depthStencilAttachment.clearStencil;

        if (isDepthFormat(imageView->pixelFormat())) {
            renderingInfo.pDepthAttachment = &depthStencilAttachment;
        }
        if (isStencilFormat(imageView->pixelFormat())) {
            renderingInfo.pStencilAttachment = &depthStencilAttachment;
        }

        frameWidth = (frameWidth > 0) ? std::min(frameWidth, imageView->width()) : imageView->width();
        frameHeight = (frameHeight > 0) ? std::min(frameHeight, imageView->height()) : imageView->height();

        imageView->image->setLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                    state.encoder->cbuffer->queueFamily()->familyIndex,
                                    commandBuffer);
    }

    renderingInfo.renderArea.offset.x = 0;
    renderingInfo.renderArea.offset.y = 0;
    renderingInfo.renderArea.extent.width = frameWidth;
    renderingInfo.renderArea.extent.height = frameHeight;
    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    // setup viewport
    bool flipY = FLIP_VIEWPORT_Y;
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(frameWidth);
    viewport.height = static_cast<float>(frameHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    if (flipY) // flip-Y-axis
    {
        viewport.y = viewport.y + viewport.height; // set origin to lower-left.
        viewport.height = -(viewport.height); // negative height.
    }

    if (!setDynamicStates.contains(VK_DYNAMIC_STATE_VIEWPORT)) {
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    }
    if (!setDynamicStates.contains(VK_DYNAMIC_STATE_SCISSOR)) {
        VkRect2D scissorRect = { {0, 0}, {frameWidth, frameHeight} };
        vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
    }
    if (!setDynamicStates.contains(VK_DYNAMIC_STATE_LINE_WIDTH)) {
        vkCmdSetLineWidth(commandBuffer, 1.0f);
    }
    if (!setDynamicStates.contains(VK_DYNAMIC_STATE_DEPTH_BIAS)) {
        // note: VkPipelineRasterizationStateCreateInfo.depthBiasEnable must be enabled.
        // vkCmdSetDepthBias(commandBuffer, 0, 0, 0)
    }
    if (!setDynamicStates.contains(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE)) {
        vkCmdSetDepthTestEnable(commandBuffer, VK_FALSE);
    }
    if (!setDynamicStates.contains(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE)) {
        vkCmdSetDepthWriteEnable(commandBuffer, VK_FALSE);
    }
    if (!setDynamicStates.contains(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP)) {
        vkCmdSetDepthCompareOp(commandBuffer, VK_COMPARE_OP_ALWAYS);
    }
    if (!setDynamicStates.contains(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE)) {
        vkCmdSetStencilTestEnable(commandBuffer, VK_FALSE);
    }
    if (!setDynamicStates.contains(VK_DYNAMIC_STATE_STENCIL_OP)) {
        vkCmdSetStencilOp(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK,
                          VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                          VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);
    }
    if (!setDynamicStates.contains(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE)) {
        vkCmdSetDepthBoundsTestEnable(commandBuffer, VK_FALSE);
    }
    if (!setDynamicStates.contains(VK_DYNAMIC_STATE_CULL_MODE)) {
        vkCmdSetCullMode(commandBuffer, VK_CULL_MODE_NONE);
    }
    if (!setDynamicStates.contains(VK_DYNAMIC_STATE_FRONT_FACE)) {
        vkCmdSetFrontFace(commandBuffer, VK_FRONT_FACE_CLOCKWISE);
    }

    // recording commands
    for (EncoderCommand& cmd : commands)
        cmd(commandBuffer, state);

    // end render pass
    vkCmdEndRendering(commandBuffer);

    // process post-renderpass commands
    for (EncoderCommand& cmd : cleanupCommands)
        cmd(commandBuffer, state);

    return true;
}

VulkanRenderCommandEncoder::VulkanRenderCommandEncoder(std::shared_ptr<VulkanCommandBuffer> cb, const RenderPassDescriptor& desc)
    : cbuffer(cb) {
    encoder = std::make_shared<Encoder>(cb.get(), desc);
}

void VulkanRenderCommandEncoder::endEncoding() {
    cbuffer->endEncoder(this, encoder);
    encoder = nullptr;
}

void VulkanRenderCommandEncoder::waitEvent(std::shared_ptr<GPUEvent> event) {
    auto semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(event);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

    encoder->addWaitSemaphore(semaphore->semaphore, semaphore->nextWaitValue(), pipelineStages);
    encoder->events.push_back(semaphore);
}

void VulkanRenderCommandEncoder::signalEvent(std::shared_ptr<GPUEvent> event) {
    auto semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(event);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

    encoder->addSignalSemaphore(semaphore->semaphore, semaphore->nextSignalValue(), pipelineStages);
    encoder->events.push_back(semaphore);
}

void VulkanRenderCommandEncoder::waitSemaphoreValue(std::shared_ptr<GPUSemaphore> sema, uint64_t value) {
    auto semaphore = std::dynamic_pointer_cast<VulkanTimelineSemaphore>(sema);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

    encoder->addWaitSemaphore(semaphore->semaphore, value, pipelineStages);
    encoder->semaphores.push_back(semaphore);
}

void VulkanRenderCommandEncoder::signalSemaphoreValue(std::shared_ptr<GPUSemaphore> sema, uint64_t value) {
    auto semaphore = std::dynamic_pointer_cast<VulkanTimelineSemaphore>(sema);
    FVASSERT_DEBUG(semaphore);

    VkPipelineStageFlags2 pipelineStages = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

    encoder->addSignalSemaphore(semaphore->semaphore, value, pipelineStages);
    encoder->semaphores.push_back(semaphore);
}

void VulkanRenderCommandEncoder::setViewport(const Viewport& v) {
    bool flipY = FLIP_VIEWPORT_Y;
    VkViewport viewport = { v.x, v.y, v.width, v.height, v.nearZ, v.farZ };
    if (flipY) // flip-Y-axis
    {
        viewport.y = viewport.y + viewport.height; // set origin to lower-left.
        viewport.height = -(viewport.height); // negative height.
    }
    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        vkCmdSetViewport(cbuffer, 0, 1, &viewport);
    };
    encoder->commands.push_back(command);
    if (encoder->drawCount == 0) {
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_VIEWPORT);
    }
}

void VulkanRenderCommandEncoder::setScissorRect(const ScissorRect& r) {
    VkRect2D scissorRect = { {r.x, r.y},{uint32_t(r.width), uint32_t(r.height)} };
    EncoderCommand command = [=](VkCommandBuffer cbuffer, EncodingState& state) mutable {
        vkCmdSetScissor(cbuffer, 0, 1, &scissorRect);
    };
    encoder->commands.push_back(command);
    if (encoder->drawCount == 0) {
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_SCISSOR);
    }
}

void VulkanRenderCommandEncoder::setRenderPipelineState(std::shared_ptr<RenderPipelineState> ps) {
    auto pipeline = std::dynamic_pointer_cast<VulkanRenderPipelineState>(ps);
    FVASSERT_DEBUG(pipeline);

    EncoderCommand command = [=](VkCommandBuffer buffer, EncodingState& state) mutable {
        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        state.pipelineState = pipeline.get();
    };
    encoder->commands.push_back(command);
    encoder->pipelineStateObjects.push_back(pipeline);
}

void VulkanRenderCommandEncoder::setVertexBuffer(std::shared_ptr<GPUBuffer> buffer, size_t offset, uint32_t index) {
    FVASSERT_DEBUG(buffer);
    setVertexBuffers(&buffer, &offset, index, 1);
}

void VulkanRenderCommandEncoder::setVertexBuffers(std::shared_ptr<GPUBuffer>* buffers, const size_t* offsets, uint32_t index, size_t count) {
    if (count > 1) {
        std::vector<VkBuffer> bufferObjects;
        std::vector<VkDeviceSize> bufferOffsets;

        bufferObjects.reserve(count);
        bufferOffsets.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            auto& bufferObject = buffers[i];
            auto bufferView = std::dynamic_pointer_cast<VulkanBufferView>(bufferObject);
            FVASSERT_DEBUG(bufferView);

            auto& buffer = bufferView->buffer;
            FVASSERT_DEBUG(buffer && buffer->buffer);

            bufferObjects.push_back(buffer->buffer);
            bufferOffsets.push_back(offsets[i]);

            encoder->buffers.push_back(bufferView);
        }
        size_t numBuffers = bufferObjects.size();
        size_t numOffsets = bufferOffsets.size();
        FVASSERT_DEBUG(numBuffers == count);
        FVASSERT_DEBUG(numOffsets == count);

        EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
            vkCmdBindVertexBuffers(commandBuffer, index, (uint32_t)count, bufferObjects.data(), bufferOffsets.data());
        };
        encoder->commands.push_back(command);
    } else if (count > 0) {
        auto& bufferObject = buffers[0];
        auto bufferView = std::dynamic_pointer_cast<VulkanBufferView>(bufferObject);
        FVASSERT_DEBUG(bufferView);
        auto& buffer = bufferView->buffer;
        VkDeviceSize of = offsets[0];

        EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
            vkCmdBindVertexBuffers(commandBuffer, index, (uint32_t)count, &buffer->buffer, &of);
        };
        encoder->commands.push_back(command);
        encoder->buffers.push_back(bufferView);
    }
}

void VulkanRenderCommandEncoder::setDepthStencilState(std::shared_ptr<DepthStencilState> ds) {
    std::shared_ptr<VulkanDepthStencilState> depthStencil = nullptr;
    if (ds) {
        depthStencil = std::dynamic_pointer_cast<VulkanDepthStencilState>(ds);
        FVASSERT_DEBUG(depthStencil);
    }

    EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
        if (depthStencil) {
            depthStencil->bind(commandBuffer);
        } else {
            // reset to default
            vkCmdSetDepthTestEnable(commandBuffer, VK_FALSE);
            vkCmdSetStencilTestEnable(commandBuffer, VK_FALSE);
            vkCmdSetDepthBoundsTestEnable(commandBuffer, VK_FALSE);

            if (state.depthStencilState == nullptr) {
                vkCmdSetDepthCompareOp(commandBuffer, VK_COMPARE_OP_ALWAYS);
                vkCmdSetDepthWriteEnable(commandBuffer, VK_FALSE);
                vkCmdSetDepthBounds(commandBuffer, 0.0f, 1.0f);
                vkCmdSetStencilCompareMask(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0xffffffff);
                vkCmdSetStencilWriteMask(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0xffffffff);
                vkCmdSetStencilOp(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK,
                                  VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                                  VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);
            }
        }
        state.depthStencilState = depthStencil.get();
    };
    encoder->commands.push_back(command);
    if (encoder->drawCount == 0) {
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE);
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE);

        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_DEPTH_BOUNDS);

        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_STENCIL_OP);
    }
}

void VulkanRenderCommandEncoder::setDepthClipMode(DepthClipMode mode) {
    auto gdevice = std::dynamic_pointer_cast<VulkanGraphicsDevice>(cbuffer->device());

    if (mode == DepthClipMode::Clamp && gdevice->features().depthClamp == VK_FALSE) {
        Log::warning("DepthClamp not supported for this hardware.");
    }
#if 0
    // VK_EXT_extended_dynamic_state3
    EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
        switch (mode) {
        case DepthClipMode::Clip:
            vkCmdSetDepthClampEnableEXT(commandBuffer, VK_FALSE);
            vkCmdSetDepthClipEnableEXT(commandBuffer, VK_TRUE);
            break;
        case DepthClipMode::Clamp:
            vkCmdSetDepthClipEnableEXT(commandBuffer, VK_FALSE);
            vkCmdSetDepthClampEnableEXT(commandBuffer, VK_TRUE);
            break;
        }
    };
    encoder->commands.push_back(command);
    if (encoder->numDrawCalls == 0) {
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT);
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT);
    }
#else
    if (mode == DepthClipMode::Clamp) {
        Log::error("setDepthClipMode failed: VK_EXT_extended_dynamic_state3 is not supported.");
    }
#endif
}

void VulkanRenderCommandEncoder::setCullMode(CullMode mode) {
    EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
        VkCullModeFlags flags = {};
        switch (mode) {
        case CullMode::None:    flags = VK_CULL_MODE_NONE;      break;
        case CullMode::Front:   flags = VK_CULL_MODE_FRONT_BIT; break;
        case CullMode::Back:    flags = VK_CULL_MODE_BACK_BIT;  break;
        }
        vkCmdSetCullMode(commandBuffer, flags);
    };
    encoder->commands.push_back(command);
    if (encoder->drawCount == 0) {
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_CULL_MODE);
    }
}

void VulkanRenderCommandEncoder::setFrontFacing(Winding winding) {
    EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
        VkFrontFace frontFace = {};
        switch (winding) {
        case Winding::Clockwise:
            frontFace = VK_FRONT_FACE_CLOCKWISE;
            break;
        case Winding::CounterClockwise:
            frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            break;
        }
        vkCmdSetFrontFace(commandBuffer, frontFace);
    };
    encoder->commands.push_back(command);
    if (encoder->drawCount == 0) {
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_FRONT_FACE);
    }
}

void VulkanRenderCommandEncoder::setBlendColor(float r, float g, float b, float a) {
    EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
        float blendConstants[4] = { r,g,b,a };
        vkCmdSetBlendConstants(commandBuffer, blendConstants);
    };
    encoder->commands.push_back(command);
    if (encoder->drawCount == 0) {
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
    }
}

void VulkanRenderCommandEncoder::setStencilReferenceValue(uint32_t value) {
    EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
        vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, value);
    };
    encoder->commands.push_back(command);
    if (encoder->drawCount == 0) {
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
    }
}

void VulkanRenderCommandEncoder::setStencilReferenceValues(uint32_t front, uint32_t back) {
    EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
        vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_BIT, front);
        vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_BACK_BIT, back);
    };
    encoder->commands.push_back(command);
    if (encoder->drawCount == 0) {
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
    }
}

void VulkanRenderCommandEncoder::setDepthBias(float depthBias, float slopeScale, float clamp) {
    EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
        vkCmdSetDepthBias(commandBuffer, depthBias, clamp, slopeScale);
    };
    encoder->commands.push_back(command);
    if (encoder->drawCount == 0) {
        encoder->setDynamicStates.insert(VK_DYNAMIC_STATE_DEPTH_BIAS);
    }
}

void VulkanRenderCommandEncoder::pushConstant(uint32_t stages, uint32_t offset, uint32_t size, const void* data) {
    VkShaderStageFlags stageFlags = 0;
    for (auto& stage : {
        std::pair(ShaderStage::Vertex, VK_SHADER_STAGE_VERTEX_BIT),
        std::pair(ShaderStage::TessellationControl, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
        std::pair(ShaderStage::TessellationEvaluation, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
        std::pair(ShaderStage::Geometry, VK_SHADER_STAGE_GEOMETRY_BIT),
        std::pair(ShaderStage::Fragment, VK_SHADER_STAGE_FRAGMENT_BIT),
         }) {
        if (stages & (uint32_t)stage.first)
            stageFlags |= stage.second;
    }
    if (stageFlags && size > 0) {
        FVASSERT_DEBUG(data);

        const uint8_t* cdata = reinterpret_cast<const uint8_t*>(data);
        auto buffer = std::make_shared<std::vector<uint8_t>>(&cdata[0], &cdata[size]);

        EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
            if (state.pipelineState) {
                vkCmdPushConstants(commandBuffer,
                                   state.pipelineState->layout,
                                   stageFlags,
                                   offset, size,
                                   buffer->data());
            }
        };
        encoder->commands.push_back(command);
    }
}

void VulkanRenderCommandEncoder::memoryBarrier(RenderStages after, RenderStages before) {
    auto stageMask = [](RenderStages stages) {
        VkPipelineStageFlags2 mask = VK_PIPELINE_STAGE_2_NONE;
        using T = std::underlying_type_t<RenderStages>;
        if (T(stages) & T(RenderStages::Vertex)) {
            mask |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
        }
        if (T(stages) & T(RenderStages::Fragment)) {
            mask |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        }
        if (T(stages) & T(RenderStages::Object)) {
            mask |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT;
        }
        if (T(stages) & T(RenderStages::Mesh)) {
            mask |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
        }
        return mask;
    };

    auto srcStages = stageMask(after);
    auto dstStages = stageMask(before);

    EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
        VkMemoryBarrier2 memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
        memoryBarrier.srcStageMask = srcStages;
        memoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
        memoryBarrier.dstStageMask = dstStages;
        memoryBarrier.dstAccessMask = VK_ACCESS_2_NONE;

        VkDependencyInfo dependencyInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dependencyInfo.memoryBarrierCount = 1;
        dependencyInfo.pMemoryBarriers = &memoryBarrier;

        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    };
    encoder->commands.push_back(command);
}

void VulkanRenderCommandEncoder::draw(uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t baseInstance) {
    if (vertexCount > 0 && instanceCount > 0) {
        EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
            vkCmdDraw(commandBuffer,
                      vertexCount,
                      instanceCount,
                      vertexStart,
                      baseInstance);
        };
        encoder->commands.push_back(command);
        encoder->drawCount++;
    }
}

void VulkanRenderCommandEncoder::drawIndexed(uint32_t indexCount, IndexType indexType, std::shared_ptr<GPUBuffer> indexBuffer, uint32_t indexBufferOffset, uint32_t instanceCount, uint32_t baseVertex, uint32_t baseInstance) {
    if (indexCount > 0 && instanceCount > 0 && indexBuffer) {
        auto bufferView = std::dynamic_pointer_cast<VulkanBufferView>(indexBuffer);
        FVASSERT_DEBUG(bufferView);

        if (bufferView == nullptr)
            return;

        FVASSERT_DEBUG(bufferView->buffer);
        if (bufferView->buffer == nullptr)
            return;

        auto& buffer = bufferView->buffer;
        VkIndexType type = {};
        switch (indexType) {
        case IndexType::UInt16: type = VK_INDEX_TYPE_UINT16; break;
        case IndexType::UInt32: type = VK_INDEX_TYPE_UINT32; break;
        }

        EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
            vkCmdBindIndexBuffer(commandBuffer, buffer->buffer, VkDeviceSize(indexBufferOffset), type);
            vkCmdDrawIndexed(commandBuffer,
                             indexCount,
                             instanceCount,
                             0,  // firstIndex = 0
                             baseVertex,
                             baseInstance);
        };
        encoder->buffers.push_back(bufferView);
        encoder->commands.push_back(command);
        encoder->drawCount++;
    }
}

void VulkanRenderCommandEncoder::setResource(uint32_t index, std::shared_ptr<ShaderBindingSet> set) {
    std::shared_ptr<VulkanDescriptorSet> descriptorSet = nullptr;
    if (set) {
        auto bindingSet = std::dynamic_pointer_cast<VulkanShaderBindingSet>(set);
        FVASSERT_DEBUG(bindingSet);

        descriptorSet = bindingSet->makeDescriptorSet();
        FVASSERT_DEBUG(descriptorSet);

        encoder->descriptorSets.push_back(descriptorSet);
    }
    if (descriptorSet == nullptr)
        return;

    EncoderCommand preCommand = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
        descriptorSet->updateImageViewLayouts(state.imageViewLayoutMap);
    };
    encoder->setupCommands.push_back(preCommand);

    EncoderCommand command = [=](VkCommandBuffer commandBuffer, EncodingState& state) mutable {
        if (state.pipelineState) {
            VkDescriptorSet ds = descriptorSet->descriptorSet;
            FVASSERT_DEBUG(ds != VK_NULL_HANDLE);

            vkCmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
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

#endif //#if FVCORE_ENABLE_VULKAN
