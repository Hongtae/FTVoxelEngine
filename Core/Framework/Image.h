#pragma once
#include "../include.h"
#include <vector>
#include <filesystem>
#include "Texture.h"
#include "CommandQueue.h"
#include "Rect.h"

namespace FV {
    enum class ImagePixelFormat {
        Invalid = 0,
        R8,         //  1 byte  per pixel, uint8
        RG8,        //  2 bytes per pixel, uint8
        RGB8,       //  3 bytes per pixel, uint8
        RGBA8,      //  4 bytes per pixel, uint8
        R16,        //  2 bytes per pixel, uint16
        RG16,       //  4 bytes per pixel, uint16
        RGB16,      //  6 bytes per pixel, uint16
        RGBA16,     //  8 bytes per pixel, uint16
        R32,        //  4 bytes per pixel, uint32
        RG32,       //  8 bytes per pixel, uint32
        RGB32,      // 12 bytes per pixel, uint32
        RGBA32,     // 16 bytes per pixel, uint32
        R32F,       //  4 bytes per pixel, float32
        RG32F,      //  8 bytes per pixel, float32
        RGB32F,     // 12 bytes per pixel, float32
        RGBA32F,    // 16 bytes per pixel, float32
    };

    enum class ImageFormat {
        Unknown = 0,
        PNG,
        JPEG,
        BMP,
    };

    enum class ImageInterpolation {
        Nearest,
        Bilinear,
        Bicubic,
        Spline,
        Gaussian,
        Quadratic,
    };

    class FVCORE_API Image : public std::enable_shared_from_this<Image> {
    public:
        Image(uint32_t width, uint32_t height, ImagePixelFormat, const void* data = nullptr);
        Image(const void* encoded, size_t);
        Image(const std::vector<uint8_t>& encodedData);
        Image(const std::filesystem::path& path);
        ~Image();

        const uint32_t width;
        const uint32_t height;
        const ImagePixelFormat pixelFormat;

        uint32_t bytesPerPixel() const;

        bool canEncode(ImageFormat format) const;
        bool encode(ImageFormat format, std::function<void(const void*, size_t)>) const;

        std::shared_ptr<Image> resample(ImagePixelFormat) const;
        std::shared_ptr<Image> resample(uint32_t width, uint32_t height, ImagePixelFormat format, ImageInterpolation interpolation) const;

        std::shared_ptr<Texture> makeTexture(CommandQueue*, uint32_t usage = TextureUsageSampled) const;
        static std::shared_ptr<Image> fromTextureBuffer(std::shared_ptr<GPUBuffer>, uint32_t width, uint32_t height, PixelFormat);

        struct Pixel { double r, g, b, a; };
        Pixel readPixel(uint32_t x, uint32_t y) const;
        void writePixel(uint32_t x, uint32_t y, const Pixel&);
        Pixel interpolate(const Rect& rect, ImageInterpolation) const;

    private:
        Pixel _interpolate(float x1, float x2, float y1, float y2, ImageInterpolation) const;
        std::vector<uint8_t> data;

        struct _DecodeContext;
        Image(_DecodeContext);
    };
}
