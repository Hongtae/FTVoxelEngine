#pragma once
#include "../include.h"
#include "PixelFormat.h"

namespace FV
{
    class Texture
    {
    public:
        enum Type
        {
            TypeUnknown = 0,
            Type1D,
            Type2D,
            Type3D,
            TypeCube,
        };
        enum Usage : uint32_t
        {
            UsageUnknown = 0U,
            UsageCopySource = 1U,
            UsageCopyDestination = 1U << 1,
            UsageSampled = 1U << 2,
            UsageStorage = 1U << 3,
            UsageShaderRead = 1U << 4,
            UsageShaderWrite = 1U << 5,
            UsageRenderTarget = 1U << 6,
            UsagePixelFormatView = 1U << 7,
        };

        virtual ~Texture() {}

        virtual uint32_t width() const = 0;
        virtual uint32_t height() const = 0;
        virtual uint32_t depth() const = 0;
        virtual uint32_t mipmapCount() const = 0;
        virtual uint32_t arrayLength() const = 0;

        virtual Type type() const = 0;
        virtual PixelFormat pixelFormat() const = 0;
    };

    struct TextureDescriptor
    {
        Texture::Type type;
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
