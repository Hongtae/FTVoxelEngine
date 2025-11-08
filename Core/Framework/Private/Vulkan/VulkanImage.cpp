#include "VulkanExtensions.h"
#include "VulkanImage.h"
#include "VulkanImageView.h"
#include "VulkanGraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanImage::VulkanImage(std::shared_ptr<VulkanGraphicsDevice> dev, const VulkanMemoryBlock& mem, VkImage img, const VkImageCreateInfo& ci)
    : image(img)
    , memory(mem)
    , gdevice(dev)
    , imageType(VK_IMAGE_TYPE_1D)
    , format(VK_FORMAT_UNDEFINED)
    , extent({ 0,0,0 })
    , mipLevels(1)
    , arrayLayers(1)
    , usage(0)
    , layoutInfo{ ci.initialLayout } {
    imageType = ci.imageType;
    format = ci.format;
    extent = ci.extent;
    mipLevels = ci.mipLevels;
    arrayLayers = ci.arrayLayers;
    usage = ci.usage;

    layoutInfo.accessMask = VK_ACCESS_2_NONE;
    layoutInfo.stageMaskBegin = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    layoutInfo.stageMaskEnd = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    layoutInfo.queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    if (layoutInfo.layout == VK_IMAGE_LAYOUT_UNDEFINED ||
        layoutInfo.layout == VK_IMAGE_LAYOUT_PREINITIALIZED)
        layoutInfo.stageMaskEnd = VK_PIPELINE_STAGE_2_HOST_BIT;

    FVASSERT_DEBUG(memory);
    FVASSERT_DEBUG(extent.width > 0);
    FVASSERT_DEBUG(extent.height > 0);
    FVASSERT_DEBUG(extent.depth > 0);
    FVASSERT_DEBUG(mipLevels > 0);
    FVASSERT_DEBUG(arrayLayers > 0);
    FVASSERT_DEBUG(format != VK_FORMAT_UNDEFINED);
}

VulkanImage::VulkanImage(std::shared_ptr<VulkanGraphicsDevice> dev, VkImage img)
    : image(img)
    , gdevice(dev)
    , memory{}
    , imageType(VK_IMAGE_TYPE_1D)
    , format(VK_FORMAT_UNDEFINED)
    , extent({ 0,0,0 })
    , mipLevels(1)
    , arrayLayers(1)
    , usage(0)
    , layoutInfo{ VK_IMAGE_LAYOUT_UNDEFINED } {
    layoutInfo.accessMask = 0;
    layoutInfo.stageMaskBegin = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    layoutInfo.stageMaskEnd = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    layoutInfo.queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
}

VulkanImage::~VulkanImage() {
    if (image) {
        vkDestroyImage(gdevice->device, image, gdevice->allocationCallbacks());
    }
    if (memory)
        memory.value().chunk->pool->dealloc(memory.value());
}

std::shared_ptr<VulkanImageView> VulkanImage::makeImageView(PixelFormat format, std::shared_ptr<VulkanImageView> parent) {
    if (this->usage & (VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_STORAGE_BIT |
                       VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
        VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        imageViewCreateInfo.image = this->image;

        auto type = this->type();
        auto pixelFormat = this->pixelFormat();

        switch (type) {
        case TextureType1D:
            if (this->arrayLayers > 1)
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            else
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
            break;
        case TextureType2D:
            if (this->arrayLayers > 1)
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            else
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        case TextureType3D:
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        case TextureTypeCube:
            if (this->arrayLayers > 1)
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            else
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
        default:
            FVASSERT_DEBUG("Uknown texture type!");
            return nullptr;
        }

        imageViewCreateInfo.format = getVkFormat(format);
        imageViewCreateInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };

        if (isColorFormat(pixelFormat))
            imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
        if (isDepthFormat(pixelFormat))
            imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (isStencilFormat(pixelFormat))
            imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = this->arrayLayers;
        imageViewCreateInfo.subresourceRange.levelCount = this->mipLevels;

        VkImageView imageView = VK_NULL_HANDLE;
        VkResult err = vkCreateImageView(gdevice->device, &imageViewCreateInfo, gdevice->allocationCallbacks(), &imageView);
        if (err != VK_SUCCESS) {
            Log::error("vkCreateImageView failed: {}", err);
            return nullptr;
        }
        return std::make_shared<VulkanImageView>(shared_from_this(), imageView, parent);
    }
    return nullptr;
}

VkImageLayout VulkanImage::setLayout(VkImageLayout layout,
                                     VkAccessFlags2 accessMask,
                                     VkPipelineStageFlags2 stageBegin,
                                     VkPipelineStageFlags2 stageEnd,
                                     uint32_t queueFamilyIndex,
                                     VkCommandBuffer commandBuffer) const {
    FVASSERT_DEBUG(layout != VK_IMAGE_LAYOUT_UNDEFINED);
    FVASSERT_DEBUG(layout != VK_IMAGE_LAYOUT_PREINITIALIZED);
    FVASSERT_DEBUG(commandBuffer != VK_NULL_HANDLE);

    std::scoped_lock guard(layoutLock);

    VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.srcAccessMask = layoutInfo.accessMask;
    barrier.dstAccessMask = accessMask;
    barrier.oldLayout = layoutInfo.layout;
    barrier.newLayout = layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;

    PixelFormat pixelFormat = this->pixelFormat();
    if (isColorFormat(pixelFormat))
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    else {
        if (isDepthFormat(pixelFormat))
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (isStencilFormat(pixelFormat))
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    barrier.srcStageMask = layoutInfo.stageMaskEnd;

    if (layoutInfo.queueFamilyIndex != queueFamilyIndex) {
        if (layoutInfo.queueFamilyIndex == VK_QUEUE_FAMILY_IGNORED || queueFamilyIndex == VK_QUEUE_FAMILY_IGNORED) {
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        } else {
            barrier.srcQueueFamilyIndex = layoutInfo.queueFamilyIndex;
            barrier.dstQueueFamilyIndex = queueFamilyIndex;
        }
    }
    if (barrier.srcStageMask == VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT) {
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }
    barrier.dstStageMask = stageBegin;
#if 0
    // full pipeline barrier for debugging
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
#endif
    VkDependencyInfo dependencyInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

    VkImageLayout oldLayout = layoutInfo.layout;
    layoutInfo.layout = layout;
    layoutInfo.stageMaskBegin = stageBegin;
    layoutInfo.stageMaskEnd = stageEnd;
    layoutInfo.accessMask = accessMask;
    layoutInfo.queueFamilyIndex = queueFamilyIndex;
    return oldLayout;
}

VkImageLayout VulkanImage::layout() const {
    std::scoped_lock guard(layoutLock);
    return layoutInfo.layout;
}

VkAccessFlags2 VulkanImage::commonLayoutAccessMask(VkImageLayout layout) {
    VkAccessFlags2 accessMask = VK_ACCESS_2_NONE;
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        accessMask = VK_ACCESS_2_NONE;
        break;
    case VK_IMAGE_LAYOUT_GENERAL:
        accessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        accessMask = VK_ACCESS_2_HOST_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        accessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
        accessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        accessMask = VK_ACCESS_2_SHADER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        accessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        accessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        accessMask = VK_ACCESS_2_NONE;
        break;
    default:
        accessMask = VK_ACCESS_2_NONE;
        break;
    }
    return accessMask;
}

#endif //#if FVCORE_ENABLE_VULKAN
