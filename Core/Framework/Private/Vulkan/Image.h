#pragma once
#include <memory>
#include <mutex>
#include "../../Texture.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "Types.h"
#include "DeviceMemory.h"

namespace FV::Vulkan {
    class GraphicsDevice;
    class Image {
    public:
        Image(std::shared_ptr<DeviceMemory>, VkImage, const VkImageCreateInfo&);
        Image(std::shared_ptr<GraphicsDevice>, VkImage);
        ~Image();

        uint32_t width() const {
            return extent.width;
        }
        uint32_t height() const {
            return extent.height;
        }
        uint32_t depth() const {
            return extent.depth;
        }
        uint32_t mipmapCount() const {
            return mipLevels;
        }
        uint32_t arrayLength() const {
            return arrayLayers;
        }
        TextureType type() const {
            switch (imageType) {
            case VK_IMAGE_TYPE_1D:
                return TextureType1D;
            case VK_IMAGE_TYPE_2D:
                return TextureType2D;
            case VK_IMAGE_TYPE_3D:
                return TextureType3D;
            }
            return TextureTypeUnknown;
        }
        PixelFormat pixelFormat() const {
            return getPixelFormat(format);
        }

        VkImageLayout setLayout(VkImageLayout layout,
                                VkAccessFlags2 accessMask,
                                VkPipelineStageFlags2 stageBegin,
                                VkPipelineStageFlags2 stageEnd,
                                uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                VkCommandBuffer commandBuffer = VK_NULL_HANDLE) const;

        VkImageLayout layout() const;

        static VkAccessFlags2 commonLayoutAccessMask(VkImageLayout);

        VkImage					image;
        VkImageType				imageType;
        VkFormat				format;
        VkExtent3D				extent;
        uint32_t				mipLevels;
        uint32_t				arrayLayers;
        VkImageUsageFlags		usage;

        std::shared_ptr<DeviceMemory>  deviceMemory;
        std::shared_ptr<GraphicsDevice> gdevice;

    private:
        struct LayoutAccessInfo {
            VkImageLayout layout;
            VkAccessFlags2 accessMask;
            VkPipelineStageFlags2 stageMaskBegin;
            VkPipelineStageFlags2 stageMaskEnd;
            uint32_t queueFamilyIndex;
        };
        mutable std::mutex layoutLock;
        mutable LayoutAccessInfo    layoutInfo;
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
