#pragma once
#include "../../Texture.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "Image.h"

namespace FV::Vulkan {
    class GraphicsDevice;
    class ImageView : public FV::Texture {
    public:
        ImageView(std::shared_ptr<Image>, VkImageView, const VkImageViewCreateInfo&);
        ImageView(std::shared_ptr<GraphicsDevice>, VkImageView);
        ~ImageView();

        VkImageView imageView;
        VkSemaphore waitSemaphore;
        VkSemaphore signalSemaphore;

        std::shared_ptr<Image> image;
        std::shared_ptr<GraphicsDevice> gdevice;

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

        std::shared_ptr<FV::GraphicsDevice> device() const override;
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
