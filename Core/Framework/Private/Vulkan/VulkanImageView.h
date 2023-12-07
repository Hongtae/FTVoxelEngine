#pragma once
#include "../../Texture.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanImage.h"

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanImageView : public Texture {
    public:
        VulkanImageView(std::shared_ptr<VulkanImage>, VkImageView, const VkImageViewCreateInfo&);
        VulkanImageView(std::shared_ptr<VulkanGraphicsDevice>, VkImageView);
        ~VulkanImageView();

        VkImageView imageView;
        VkSemaphore waitSemaphore;
        VkSemaphore signalSemaphore;

        std::shared_ptr<VulkanImage> image;
        std::shared_ptr<VulkanGraphicsDevice> gdevice;

        uint32_t width() const override {
            return image->width();
        }
        uint32_t height() const override {
            return image->height();
        }
        uint32_t depth() const override {
            return image->depth();
        }
        uint32_t mipmapCount() const override {
            return image->mipmapCount();
        }
        uint32_t arrayLength() const override {
            return image->arrayLength();
        }
        TextureType type() const override {
            return image->type();
        }
        PixelFormat pixelFormat() const override {
            return image->pixelFormat();
        }

        std::shared_ptr<GraphicsDevice> device() const override;
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
