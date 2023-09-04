#include "Extensions.h"
#include "DepthStencilState.h"
#include "GraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

DepthStencilState::DepthStencilState(std::shared_ptr<GraphicsDevice> d)
    : gdevice(d)
    , depthTestEnable(VK_FALSE)
    , depthWriteEnable(VK_FALSE)
    , depthCompareOp(VK_COMPARE_OP_ALWAYS)
    , depthBoundsTestEnable(VK_FALSE)
    , minDepthBounds(0.0)
    , maxDepthBounds(1.0)
    , stencilTestEnable(VK_FALSE) {
    this->front = {
        VK_STENCIL_OP_KEEP,
        VK_STENCIL_OP_KEEP,
        VK_STENCIL_OP_KEEP,
        VK_COMPARE_OP_ALWAYS,
        0xffffffff,
        0xffffffff,
        0 };
    this->back = {
        VK_STENCIL_OP_KEEP,
        VK_STENCIL_OP_KEEP,
        VK_STENCIL_OP_KEEP,
        VK_COMPARE_OP_ALWAYS,
        0xffffffff,
        0xffffffff,
        0 };
}

DepthStencilState::~DepthStencilState() {
}

void DepthStencilState::bind(VkCommandBuffer commandBuffer) {
    vkCmdSetDepthTestEnable(commandBuffer, this->depthTestEnable);
    vkCmdSetStencilTestEnable(commandBuffer, this->stencilTestEnable);
    vkCmdSetDepthBoundsTestEnable(commandBuffer, this->depthBoundsTestEnable);

    if (this->depthTestEnable != VK_FALSE) {
        vkCmdSetDepthCompareOp(commandBuffer, this->depthCompareOp);
        vkCmdSetDepthWriteEnable(commandBuffer, this->depthWriteEnable);
    }

    if (this->depthBoundsTestEnable != VK_FALSE) {
        vkCmdSetDepthBounds(commandBuffer, this->minDepthBounds, this->maxDepthBounds);
    }

    if (this->stencilTestEnable != VK_FALSE) {
        // front face stencil
        vkCmdSetStencilCompareMask(commandBuffer,
                                   VK_STENCIL_FACE_FRONT_BIT,
                                   this->front.compareMask);
        vkCmdSetStencilWriteMask(commandBuffer,
                                 VK_STENCIL_FACE_FRONT_BIT,
                                 this->front.writeMask);
        vkCmdSetStencilOp(commandBuffer,
                          VK_STENCIL_FACE_FRONT_BIT,
                          this->front.failOp,
                          this->front.passOp,
                          this->front.depthFailOp,
                          this->front.compareOp);
        // back face stencil
        vkCmdSetStencilCompareMask(commandBuffer,
                                   VK_STENCIL_FACE_BACK_BIT,
                                   this->back.compareMask);
        vkCmdSetStencilWriteMask(commandBuffer,
                                 VK_STENCIL_FACE_BACK_BIT,
                                 this->back.writeMask);
        vkCmdSetStencilOp(commandBuffer,
                          VK_STENCIL_FACE_BACK_BIT,
                          this->back.failOp,
                          this->back.passOp,
                          this->back.depthFailOp,
                          this->back.compareOp);
    }
}

std::shared_ptr<FV::GraphicsDevice> DepthStencilState::device() const {
    return gdevice;
}

#endif //#if FVCORE_ENABLE_VULKAN
