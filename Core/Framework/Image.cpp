#include <type_traits>
#include <memory>
#include <vector>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <algorithm>
#include "../Libs/dkwrapper/DKImage.h"
#include "Image.h"
#include "Logger.h"
#include "GraphicsDevice.h"

namespace
{
    std::vector<uint8_t> ifstreamVector(const std::filesystem::path& path)
    {
        std::ifstream fs(path, std::ifstream::binary | std::ifstream::in);
        if (fs.good())
        {
            return std::vector<uint8_t>((std::istreambuf_iterator<char>(fs)),
                                        std::istreambuf_iterator<char>());
        }
        return {};
    }

    using RawColorValue = struct { double r, g, b, a; };
    template <typename T> using enable_if_arithmetic_t = std::enable_if_t<std::is_arithmetic_v<T>>;
    // T = stored pixel component type, Q = quantization constant
    template <typename T, double Q = std::numeric_limits<T>::max(), typename = enable_if_arithmetic_t<T>>
    void writePixelR(uint8_t* data, size_t offset, const RawColorValue& value)
    {
        T color[] = { T(value.r * Q) };
        memcpy(&data[offset], color, sizeof(color));
    }
    template <typename T, double Q = std::numeric_limits<T>::max(), typename = enable_if_arithmetic_t<T>>
    void writePixelRG(uint8_t* data, size_t offset, const RawColorValue& value)
    {
        T color[] = { T(value.r * Q), T(value.g * Q) };
        memcpy(&data[offset], color, sizeof(color));
    }
    template <typename T, double Q = std::numeric_limits<T>::max(), typename = enable_if_arithmetic_t<T>>
    void writePixelRGB(uint8_t* data, size_t offset, const RawColorValue& value)
    {
        T color[] = { T(value.r * Q), T(value.g * Q), T(value.b * Q) };
        memcpy(&data[offset], color, sizeof(color));
    }
    template <typename T, double Q = std::numeric_limits<T>::max(), typename = enable_if_arithmetic_t<T>>
    void writePixelRGBA(uint8_t* data, size_t offset, const RawColorValue& value)
    {
        T color[] = { T(value.r * Q), T(value.g * Q), T(value.b * Q), T(value.a * Q) };
        memcpy(&data[offset], color, sizeof(color));
    }
    // T = stored pixel component type, N = normalization constant
    template <typename T, double N = 1.0 / double(std::numeric_limits<T>::max()), typename = enable_if_arithmetic_t<T>>
    RawColorValue readPixelR(const uint8_t* data, size_t offset)
    {
        const T* color = (const T*)&data[offset];
        return { color[0] * N, 0.0, 0.0, 1.0 };
    }
    template <typename T, double N = 1.0 / double(std::numeric_limits<T>::max()), typename = enable_if_arithmetic_t<T>>
    RawColorValue readPixelRG(const uint8_t* data, size_t offset)
    {
        const T* color = (const T*)&data[offset];
        return { color[0] * N, color[1] * N, 0.0, 1.0};
    }
    template <typename T, double N = 1.0 / double(std::numeric_limits<T>::max()), typename = enable_if_arithmetic_t<T>>
    RawColorValue readPixelRGB(const uint8_t* data, size_t offset)
    {
        const T* color = (const T*)&data[offset];
        return { color[0] * N, color[1] * N, color[2] * N, 1.0};
    }
    template <typename T, double N = 1.0 / double(std::numeric_limits<T>::max()), typename = enable_if_arithmetic_t<T>>
    RawColorValue readPixelRGBA(const uint8_t* data, size_t offset)
    {
        const T* color = (const T*)&data[offset];
        return { color[0] * N, color[1] * N, color[2] * N, color[3] * N };
    }
}

namespace FV
{
    struct Image::_DecodeContext
    {
        DKImageDecodeContext decoder;
    };
}

using namespace FV;

Image::Image(uint32_t w, uint32_t h, ImagePixelFormat format, const void* p)
    : width(w), height(h), pixelFormat(format)
{
    if (format != ImagePixelFormat::Invalid)
    {
        size_t dataSize = bytesPerPixel() * width * height;

        if (dataSize > 0)
        {
            if (p)
            {
                const uint8_t* ptr = reinterpret_cast<const uint8_t*>(p);
                data = std::vector<uint8_t>(&ptr[0], &ptr[dataSize]);
            }
            else
            {
                data = std::vector<uint8_t>(dataSize, uint8_t(0));
            }
        }
    }
}

Image::Image(const std::vector<uint8_t>& encodedData)
    : Image(data.data(), data.size())
{
}

Image::Image(const std::filesystem::path& path)
    : Image(ifstreamVector(path))
{
}

Image::Image(const void* encoded, size_t length)
    : Image(_DecodeContext{ DKImageDecodeFromMemory(encoded, length) })
{
}

Image::Image(_DecodeContext ctxt)
    : width(ctxt.decoder.width)
    , height(ctxt.decoder.height)
    , pixelFormat((ImagePixelFormat)ctxt.decoder.pixelFormat)
{
    auto& decoder = ctxt.decoder;
    if (decoder.error != DKImageDecodeError_Success)
    {
        auto mesg = std::format("Image decode error: {}",
                                decoder.errorDescription);

        DKImageReleaseDecodeContext(&decoder);

        Log::error(mesg);
        throw std::runtime_error(mesg);
    }
    size_t s = bytesPerPixel() * width * height;
    size_t length = decoder.decodedDataLength;
    FVASSERT(s == length);

    const uint8_t* data = reinterpret_cast<const uint8_t*>(decoder.decodedData);
    this->data = std::vector<uint8_t>(&data[0], &data[length]);

    DKImageReleaseDecodeContext(&decoder);
}

Image::~Image()
{
}

uint32_t Image::bytesPerPixel() const
{
   return DKImagePixelFormatBytesPerPixel(static_cast<DKImagePixelFormat>(pixelFormat));
}

bool Image::canEncode(ImageFormat imageFormat) const
{
    auto supportFormat = DKImagePixelFormatEncodingSupported(static_cast<DKImageFormat>(imageFormat),
                                                             static_cast<DKImagePixelFormat>(pixelFormat));
    return static_cast<ImagePixelFormat>(supportFormat) == this->pixelFormat
        && this->pixelFormat != ImagePixelFormat::Invalid;
}

bool Image::encode(ImageFormat imageFormat, std::function<void(void*, size_t)> fn) const
{
    uint32_t byteCount = bytesPerPixel() * width * height;
    FVASSERT_DEBUG(byteCount == data.size());

    auto context = DKImageEncodeFromMemory(static_cast<DKImageFormat>(imageFormat), width, height,
                                           static_cast<DKImagePixelFormat>(pixelFormat),
                                           data.data(), data.size());
    bool result = false;
    if (context.error == DKImageEncodeError_Success)
    {
        result = true;
    }
    DKImageReleaseEncodeContext(&context);
    return result;
}

std::shared_ptr<Image> Image::resample(ImagePixelFormat format) const
{
    return resample(width, height, format, ImageInterpolation::Nearest);
}

std::shared_ptr<Image> Image::resample(uint32_t width, uint32_t height, ImagePixelFormat format, ImageInterpolation interpolation) const
{
    if (width == 0 || height == 0 || format == ImagePixelFormat::Invalid)
        return nullptr;
    if (width == this->width && height == this->height && format == this->pixelFormat)
    {
        if (auto p = const_cast<Image*>(this)->shared_from_this(); p)
            return p;
        // copy
        return std::make_shared<Image>(width, height, format, data.data());
    }

    void (*writePixel)(uint8_t*, size_t, const RawColorValue&) = nullptr;
    RawColorValue (*readPixel)(const uint8_t*, size_t) = nullptr;

    switch (format)
    {
    case ImagePixelFormat::R8:      writePixel = writePixelR<uint8_t>;      break;
    case ImagePixelFormat::RG8:     writePixel = writePixelRG<uint8_t>;     break;
    case ImagePixelFormat::RGB8:    writePixel = writePixelRGB<uint8_t>;    break;
    case ImagePixelFormat::RGBA8:   writePixel = writePixelRGBA<uint8_t>;   break;
    case ImagePixelFormat::R16:     writePixel = writePixelR<uint16_t>;     break;
    case ImagePixelFormat::RG16:    writePixel = writePixelRG<uint16_t>;    break;
    case ImagePixelFormat::RGB16:   writePixel = writePixelRGB<uint16_t>;   break;
    case ImagePixelFormat::RGBA16:  writePixel = writePixelRGBA<uint16_t>;  break;
    case ImagePixelFormat::R32:     writePixel = writePixelR<uint32_t>;     break;
    case ImagePixelFormat::RG32:    writePixel = writePixelRG<uint32_t>;    break;
    case ImagePixelFormat::RGB32:   writePixel = writePixelRGB<uint32_t>;   break;
    case ImagePixelFormat::RGBA32:  writePixel = writePixelRGBA<uint32_t>;  break;
    case ImagePixelFormat::R32F:    writePixel = writePixelR<float, 1.0>;   break;
    case ImagePixelFormat::RG32F:   writePixel = writePixelRG<float, 1.0>;  break;
    case ImagePixelFormat::RGB32F:  writePixel = writePixelRGB<float, 1.0>; break;
    case ImagePixelFormat::RGBA32F: writePixel = writePixelRGBA<float, 1.0>;break;
    }

    switch (this->pixelFormat)
    {
    case ImagePixelFormat::R8:      readPixel = readPixelR<uint8_t>;        break;
    case ImagePixelFormat::RG8:     readPixel = readPixelRG<uint8_t>;       break;
    case ImagePixelFormat::RGB8:    readPixel = readPixelRGB<uint8_t>;      break;
    case ImagePixelFormat::RGBA8:   readPixel = readPixelRGBA<uint8_t>;     break;
    case ImagePixelFormat::R16:     readPixel = readPixelR<uint16_t>;       break;
    case ImagePixelFormat::RG16:    readPixel = readPixelRG<uint16_t>;      break;
    case ImagePixelFormat::RGB16:   readPixel = readPixelRGB<uint16_t>;     break;
    case ImagePixelFormat::RGBA16:  readPixel = readPixelRGBA<uint16_t>;    break;
    case ImagePixelFormat::R32:     readPixel = readPixelR<uint32_t>;       break;
    case ImagePixelFormat::RG32:    readPixel = readPixelRG<uint32_t>;      break;
    case ImagePixelFormat::RGB32:   readPixel = readPixelRGB<uint32_t>;     break;
    case ImagePixelFormat::RGBA32:  readPixel = readPixelRGBA<uint32_t>;    break;
    case ImagePixelFormat::R32F:    readPixel = readPixelR<float, 1.0>;     break;
    case ImagePixelFormat::RG32F:   readPixel = readPixelRG<float, 1.0>;    break;
    case ImagePixelFormat::RGB32F:  readPixel = readPixelRGB<float, 1.0>;   break;
    case ImagePixelFormat::RGBA32F: readPixel = readPixelRGBA<float, 1.0>;  break;
    }

    if (writePixel == nullptr)
    {
        Log::error("Invalid output format!");
        return nullptr;
    }
    if (readPixel == nullptr)
    {
        Log::error("Invalid input format!");
        return nullptr;
    }

    uint32_t bpp = DKImagePixelFormatBytesPerPixel((DKImagePixelFormat)format);
    uint32_t rowStride = bpp * width;
    uint32_t bufferLength = rowStride * height;
    auto image = std::make_shared<Image>(width, height, format, nullptr);
    FVASSERT_DEBUG(bufferLength == image->data.size());
    uint8_t* buffer = image->data.data();

    const float scaleX = float(this->width) / float(width);
    const float scaleY = float(this->height) / float(height);
    const uint32_t sourceBpp = this->bytesPerPixel();
    const uint8_t* sourceData = this->data.data();

    auto getPixel = [&](float x, float y)->RawColorValue
    {
        int nx = std::clamp(int(x), 0, int(this->width) - 1);
        int ny = std::clamp(int(y), 0, int(this->height) - 1);
        size_t offset = size_t(ny * this->width + nx) * sourceBpp;
        return readPixel(sourceData, offset);
    };

    auto interpKernel = [&getPixel](double (*kernel)(float), float x, float y) -> RawColorValue
    {
        float fx = floor(x);
        float fy = floor(y);
        float px[4] = { fx - 1.0f, fx, fx + 1.0f, fx + 2.0f };
        float py[4] = { fy - 1.0f, fy, fy + 1.0f, fy + 2.0f };
        double kx[4] = { kernel(px[0] - x), kernel(px[1] - x), kernel(px[2] - x), kernel(px[3] - x) };
        double ky[4] = { kernel(py[0] - y), kernel(py[1] - y), kernel(py[2] - y), kernel(py[3] - y) };

        RawColorValue color = { 0, 0, 0, 0 };
        for (int yIndex = 0; yIndex < 4; ++yIndex)
        {
            for (int xIndex = 0; xIndex < 4; ++xIndex)
            {
                double k = kx[xIndex] * ky[yIndex];
                auto c = getPixel(px[xIndex], py[yIndex]);
                color = { color.r + c.r * k, color.g + c.g * k, color.b + c.b * k, color.a + c.a * k };
            }
        }
        return color;
    };

    std::function<RawColorValue(float, float)> interpolatePoint;
    switch (interpolation)
    {
    case ImageInterpolation::Nearest:
        interpolatePoint = [&](float x, float y)->RawColorValue
        {
            return getPixel(std::round(x), std::round(y));
        };
        break;
    case ImageInterpolation::Bilinear:
        interpolatePoint = [&](float x, float y)->RawColorValue
        {
            auto x1 = floor(x);
            auto x2 = floor(x + 1);
            auto y1 = floor(y);
            auto y2 = floor(y + 1);
            auto t1 = double(x - x1);
            auto t2 = double(y - y1);
            auto d = t1 * t2;
            auto b = t1 - d;
            auto c = t2 - d;
            auto a = 1.0 - t1 - c;
            auto p1 = getPixel(x1, y1);
            auto p2 = getPixel(x2, y1);
            auto p3 = getPixel(x1, y2);
            auto p4 = getPixel(x2, y2);
            return { p1.r * a + p2.r * b + p3.r * c + p4.r * d,
                    p1.g * a + p2.g * b + p3.g * c + p4.g * d,
                    p1.b * a + p2.b * b + p3.b * c + p4.b * d,
                    p1.a * a + p2.a * b + p3.a * c + p4.a * d };
        };
        break;
    case ImageInterpolation::Bicubic:
        interpolatePoint = [&](float x, float y)->RawColorValue
        {
            auto kernelCubic = [](float t)->double
            {
                auto t1 = abs(t);
                auto t2 = t1 * t1;
                if (t1 < 1) { return double(1 - 2 * t2 + t2 * t1); }
                if (t1 < 2) { return double(4 - 8 * t1 + 5 * t2 - t2 * t1); }
                return 0.0;
            };
            return interpKernel(kernelCubic, x, y);
        };
        break;
    case ImageInterpolation::Spline: 
        interpolatePoint = [&](float x, float y)->RawColorValue
        {
            auto kernelSpline = [](float _t)->double
            {
                auto t = double(_t);
                if (t < -2.0) { return 0.0; }
                if (t < -1.0) { return (2.0 + t) * (2.0 + t) * (2.0 + t) * 0.16666666666666666667; }
                if (t < 0.0) { return (4.0 + t * t * (-6.0 - 3.0 * t)) * 0.16666666666666666667; }
                if (t < 1.0) { return (4.0 + t * t * (-6.0 + 3.0 * t)) * 0.16666666666666666667; }
                if (t < 2.0) { return (2.0 - t) * (2.0 - t) * (2.0 - t) * 0.16666666666666666667; }
                return 0.0;
            };
            return interpKernel(kernelSpline, x, y);
        };
        break;
    case ImageInterpolation::Gaussian:
        interpolatePoint = [&](float x, float y)->RawColorValue
        {
            auto kernelGaussian = [](float t)->double
            {
                return exp(-2.0 * double(t * t)) * 0.79788456080287;
            };
            return interpKernel(kernelGaussian, x, y);
        };
        break;
    case ImageInterpolation::Quadratic: 
        interpolatePoint = [&](float x, float y)->RawColorValue
        {
            auto kernelQuadratic = [](float t)->double
            {
                if (t < -1.5) { return 0; }
                if (t < -0.5) { return double(0.5 * (t + 1.5) * (t + 1.5)); }
                if (t < 0.5) { return double(0.75 - t * t); }
                if (t < 1.5) { return double(0.5 * (t - 1.5) * (t - 1.5)); }
                return 0.0;
            };
            return interpKernel(kernelQuadratic, x, y);
        };
        break;
    }

    auto interpolateBox = [&](float x, float y)->RawColorValue
    {
        auto x1 = x - scaleX * 0.5f;
        auto x2 = x + scaleX * 0.5f;
        auto y1 = y - scaleY * 0.5f;
        auto y2 = y + scaleY * 0.5f;

        RawColorValue color = { 0, 0, 0, 0 };
        for (int ny = (int)std::round(y1), ny2 = (int)std::round(y2); ny <= ny2; ++ny)
        {
            for (int nx = (int)std::round(x1), nx2 = (int)std::round(x2); nx <= nx2; ++nx)
            {
                auto xMin = std::max(float(nx) - 0.5f, x1);
                auto xMax = std::min(float(nx) + 0.5f, x2);
                auto yMin = std::max(float(ny) - 0.5f, y1);
                auto yMax = std::min(float(ny) + 0.5f, y2);

                auto k = double((xMax - xMin) * (yMax - yMin));
                auto c = interpolatePoint((xMin + xMax) * 0.5f, (yMin + yMax) * 0.5f);
                color = { color.r + c.r * k, color.g + c.g * k, color.b + c.b * k, color.a + c.a * k };
            }
        }

        auto area = 1.0 / double((x2 - x1) * (y2 - y1));
        return { color.r * area, color.g * area, color.b * area, color.a * area };
    };
    std::function<RawColorValue(float, float)> sample;
    if (this->width != width || this->height != height)
    {
        if (scaleX <= 1.0f && scaleY <= 1.0f)
            sample = interpolatePoint;
        else
            sample = interpolateBox;
    }
    else
    {
        sample = [&](float x, float y)
        {
            return getPixel(x, y);
        };
    }

    for (uint32_t ny = 0; ny < height; ++ny)
    {
        for (uint32_t nx = 0; nx < width; ++nx)
        {
            // convert source location
            auto x = (float(nx) + 0.5f) * scaleX - 0.5f;
            auto y = (float(ny) + 0.5f) * scaleY - 0.5f;

            auto color = sample(x, y);
            color.r = std::clamp(color.r, 0.0, 1.0);
            color.g = std::clamp(color.g, 0.0, 1.0);
            color.b = std::clamp(color.b, 0.0, 1.0);
            color.a = std::clamp(color.a, 0.0, 1.0);
            size_t offset = ny * rowStride + nx * bpp;
            writePixel(buffer, offset, color);
        }
    }
    return image;
}

std::shared_ptr<Texture> Image::makeTexture(std::shared_ptr<CommandQueue> queue) const
{
    PixelFormat textureFormat = PixelFormat::Invalid;
    ImagePixelFormat imageFormat = this->pixelFormat;

    switch (this->pixelFormat)
    {
    case ImagePixelFormat::R8:
        textureFormat = PixelFormat::R8Unorm;
        break;
    case ImagePixelFormat::RG8:
        textureFormat = PixelFormat::RG8Unorm;
        break;
    case ImagePixelFormat::RGB8:
    case ImagePixelFormat::RGBA8:
        textureFormat = PixelFormat::RGBA8Unorm;
        imageFormat = ImagePixelFormat::RGBA8;
        break;
    case ImagePixelFormat::R16:
        textureFormat = PixelFormat::R16Unorm;
        break;
    case ImagePixelFormat::RG16:
        textureFormat = PixelFormat::RG16Unorm;
        break;
    case ImagePixelFormat::RGB16:
    case ImagePixelFormat::RGBA16:
        textureFormat = PixelFormat::RGBA16Unorm;
        imageFormat = ImagePixelFormat::RGBA16;
        break;
    case ImagePixelFormat::R32:
        textureFormat = PixelFormat::R32Uint;
        break;
    case ImagePixelFormat::RG32:
        textureFormat = PixelFormat::RG32Uint;
        break;
    case ImagePixelFormat::RGB32:
    case ImagePixelFormat::RGBA32:
        textureFormat = PixelFormat::RGBA32Uint;
        imageFormat = ImagePixelFormat::RGBA32;
        break;
    case ImagePixelFormat::R32F:
        textureFormat = PixelFormat::R32Float;
        break;
    case ImagePixelFormat::RG32F:
        textureFormat = PixelFormat::RG32Float;
        break;
    case ImagePixelFormat::RGB32F:
    case ImagePixelFormat::RGBA32F:
        textureFormat = PixelFormat::RGBA32Float;
        imageFormat = ImagePixelFormat::RGBA32F;
        break;
    default:
        textureFormat = PixelFormat::Invalid;
        break;
    }

    if (textureFormat == PixelFormat::Invalid) {
        Log::error("Invalid pixel format");
        return nullptr;
    }
    if (imageFormat != this->pixelFormat) {
        if (auto image = resample(imageFormat); image)
            return image->makeTexture(queue);
        return nullptr;
    }

    auto device = queue->device();

    // create texture
    auto texture = device->makeTexture(
        TextureDescriptor
        {
            TextureType2D,
            textureFormat,
            width,
            height,
            1, 1, 1, 1,
            TextureUsageCopyDestination & TextureUsageSampled
        });
    if (texture == nullptr)
        return nullptr;

    // create buffer for staging
    auto stgBuffer = device->makeBuffer(data.size(),
                                        GPUBuffer::StorageModeShared,
                                        CPUCacheModeWriteCombined);
    if (stgBuffer == nullptr)
    {
        Log::error("Failed to make buffer object.");
        return nullptr;
    }

    auto p = stgBuffer->contents();
    if (p == nullptr)
    {
        Log::error("Buffer memory mapping failed.");
        return nullptr;
    }

    memcpy(p, data.data(), data.size());
    stgBuffer->flush();

    auto commandBuffer = queue->makeCommandBuffer();
    if (commandBuffer == nullptr)
    {
        Log::error("Failed to make command buffer.");
        return nullptr;
    }

    auto encoder = commandBuffer->makeCopyCommandEncoder();
    if (encoder == nullptr)
    {
        Log::error("Failed to make copy command encoder.");
        return nullptr;
    }

    encoder->copy(stgBuffer,
                  BufferImageOrigin{ 0, width, height },
                  texture,
                  TextureOrigin{ 0 },
                  TextureSize{ width, height, 1 });

    encoder->endEncoding();
    commandBuffer->commit();
    return texture;
}
