#pragma once
#include "../include.h"
#include "PixelFormat.h"

namespace FV {
    class GraphicsDevice;

    enum TextureType {
        TextureTypeUnknown = 0,
        TextureType1D,
        TextureType2D,
        TextureType3D,
        TextureTypeCube,
    };

    enum TextureUsage {
        TextureUsageUnknown = 0U,
        TextureUsageCopySource = 1U,
        TextureUsageCopyDestination = 1U << 1,
        TextureUsageSampled = 1U << 2,
        TextureUsageStorage = 1U << 3,
        TextureUsageShaderRead = 1U << 4,
        TextureUsageShaderWrite = 1U << 5,
        TextureUsageRenderTarget = 1U << 6,
        TextureUsagePixelFormatView = 1U << 7,
    };

    class Texture {
    public:
        virtual ~Texture() {}

        virtual uint32_t width() const = 0;
        virtual uint32_t height() const = 0;
        virtual uint32_t depth() const = 0;
        virtual uint32_t mipmapCount() const = 0;
        virtual uint32_t arrayLength() const = 0;

        virtual TextureType type() const = 0;
        virtual PixelFormat pixelFormat() const = 0;

        virtual std::shared_ptr<GraphicsDevice> device() const = 0;
    };

    struct TextureDescriptor {
        TextureType textureType;
        PixelFormat pixelFormat;

        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t mipmapLevels;
        uint32_t sampleCount;
        uint32_t arrayLength;
        uint32_t usage;
    };
}
