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
#include "Float16.h"

namespace {
    std::vector<uint8_t> ifstreamVector(const std::filesystem::path& path) {
        std::ifstream fs(path, std::ifstream::binary | std::ifstream::in);
        if (fs.good()) {
            return std::vector<uint8_t>((std::istreambuf_iterator<char>(fs)),
                                        std::istreambuf_iterator<char>());
        }
        return {};
    }

    using RawColorValue = struct { double r, g, b, a; };
    // T = stored pixel component type, Q = quantization constant
    template <typename T, double Q = std::numeric_limits<T>::max()> requires std::is_arithmetic_v<T>
    void writePixelR(uint8_t* data, size_t offset, const RawColorValue& value) {
        T color[] = { T(value.r * Q) };
        memcpy(&data[offset], color, sizeof(color));
    }
    template <typename T, double Q = std::numeric_limits<T>::max()> requires std::is_arithmetic_v<T>
    void writePixelRG(uint8_t* data, size_t offset, const RawColorValue& value) {
        T color[] = { T(value.r * Q), T(value.g * Q) };
        memcpy(&data[offset], color, sizeof(color));
    }
    template <typename T, double Q = std::numeric_limits<T>::max()> requires std::is_arithmetic_v<T>
    void writePixelRGB(uint8_t* data, size_t offset, const RawColorValue& value) {
        T color[] = { T(value.r * Q), T(value.g * Q), T(value.b * Q) };
        memcpy(&data[offset], color, sizeof(color));
    }
    template <typename T, double Q = std::numeric_limits<T>::max()> requires std::is_arithmetic_v<T>
    void writePixelRGBA(uint8_t* data, size_t offset, const RawColorValue& value) {
        T color[] = { T(value.r * Q), T(value.g * Q), T(value.b * Q), T(value.a * Q) };
        memcpy(&data[offset], color, sizeof(color));
    }

    // T = stored pixel component type, N = normalization constant
    template <typename T, double N = 1.0 / double(std::numeric_limits<T>::max())> requires std::is_arithmetic_v<T>
    RawColorValue readPixelR(const uint8_t* data, size_t offset) {
        const T* color = (const T*)&data[offset];
        return { color[0] * N, 0.0, 0.0, 1.0 };
    }
    template <typename T, double N = 1.0 / double(std::numeric_limits<T>::max())> requires std::is_arithmetic_v<T>
    RawColorValue readPixelRG(const uint8_t* data, size_t offset) {
        const T* color = (const T*)&data[offset];
        return { color[0] * N, color[1] * N, 0.0, 1.0 };
    }
    template <typename T, double N = 1.0 / double(std::numeric_limits<T>::max())> requires std::is_arithmetic_v<T>
    RawColorValue readPixelRGB(const uint8_t* data, size_t offset) {
        const T* color = (const T*)&data[offset];
        return { color[0] * N, color[1] * N, color[2] * N, 1.0 };
    }
    template <typename T, double N = 1.0 / double(std::numeric_limits<T>::max())> requires std::is_arithmetic_v<T>
    RawColorValue readPixelRGBA(const uint8_t* data, size_t offset) {
        const T* color = (const T*)&data[offset];
        return { color[0] * N, color[1] * N, color[2] * N, color[3] * N };
    }

    template <typename T> requires std::integral<T>
    inline double fixedToDouble(T value) {
        if (std::numeric_limits<T>::is_signed)
            return std::max(double(value) / double(std::numeric_limits<T>::max() - 1), -1.0);
        return double(value) / double(std::numeric_limits<T>::max() - 1);
    }

    template <uint32_t E, uint32_t M>
    inline double ufloatToDouble(uint32_t exponent, uint32_t mantissa) {
        constexpr uint32_t expUpper = (1 << E) - 1;
        constexpr uint32_t expLower = expUpper >> 1;
        constexpr double manUpper = double(1 << M);

        uint32_t m = mantissa & ((1 << M) - 1);
        uint32_t e = exponent & ((1 << E) - 1);

        if (e == 0) {
            if (m == 0) return 0.0;
            return (1.0 / double(1 << (expLower - 1))) * (double(m) / manUpper);
        }
        if (e < expUpper) {
            if (e > expLower) {
                return double(1 << (e - expLower)) * (1.0 + double(m) / manUpper);
            }
            return (1.0 / double(1 << (expLower - e))) * (1.0 + double(m) / manUpper);
        }
        if (m == 0)
            return std::numeric_limits<double>::infinity();
        return std::numeric_limits<double>::quiet_NaN();
    }

    template <uint32_t E, uint32_t M>
    inline double ufloatToDouble(uint32_t value) {
        return ufloatToDouble<E, M>(value >> M, value);
    }
}

namespace FV {
    struct Image::_DecodeContext {
        DKImageDecodeContext decoder;
    };

    using ReadFunction = RawColorValue(*)(const uint8_t*, size_t);
    ReadFunction getReadFunction(ImagePixelFormat pixelFormat) {
        ReadFunction readPixel = nullptr;
        switch (pixelFormat) {
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
        return readPixel;
    }

    using WriteFunction = void (*)(uint8_t*, size_t, const RawColorValue&);
    WriteFunction getWriteFunction(ImagePixelFormat pixelFormat) {
        WriteFunction writePixel = nullptr;
        switch (pixelFormat) {
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
        case ImagePixelFormat::RGBA32F: writePixel = writePixelRGBA<float, 1.0>; break;
        }
        return writePixel;
    }
}

using namespace FV;

Image::Image(uint32_t w, uint32_t h, ImagePixelFormat format, const void* p)
    : width(w), height(h), pixelFormat(format) {
    if (format != ImagePixelFormat::Invalid) {
        size_t dataSize = bytesPerPixel() * width * height;

        if (dataSize > 0) {
            if (p) {
                const uint8_t* ptr = reinterpret_cast<const uint8_t*>(p);
                data = std::vector<uint8_t>(&ptr[0], &ptr[dataSize]);
            } else {
                data = std::vector<uint8_t>(dataSize, uint8_t(0));
            }
        }
    }
}

Image::Image(const std::vector<uint8_t>& encodedData)
    : Image(data.data(), data.size()) {
}

Image::Image(const std::filesystem::path& path)
    : Image(ifstreamVector(path)) {
}

Image::Image(const void* encoded, size_t length)
    : Image(_DecodeContext{ DKImageDecodeFromMemory(encoded, length) }) {
}

Image::Image(_DecodeContext ctxt)
    : width(ctxt.decoder.width)
    , height(ctxt.decoder.height)
    , pixelFormat((ImagePixelFormat)ctxt.decoder.pixelFormat) {
    auto& decoder = ctxt.decoder;
    if (decoder.error != DKImageDecodeError_Success) {
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

Image::~Image() {
}

uint32_t Image::bytesPerPixel() const {
    return DKImagePixelFormatBytesPerPixel(static_cast<DKImagePixelFormat>(pixelFormat));
}

bool Image::canEncode(ImageFormat imageFormat) const {
    auto supportFormat = DKImagePixelFormatEncodingSupported(static_cast<DKImageFormat>(imageFormat),
                                                             static_cast<DKImagePixelFormat>(pixelFormat));
    return static_cast<ImagePixelFormat>(supportFormat) == this->pixelFormat
        && this->pixelFormat != ImagePixelFormat::Invalid;
}

bool Image::encode(ImageFormat imageFormat, std::function<void(const void*, size_t)> fn) const {
    uint32_t byteCount = bytesPerPixel() * width * height;
    FVASSERT_DEBUG(byteCount == data.size());

    auto context = DKImageEncodeFromMemory(static_cast<DKImageFormat>(imageFormat), width, height,
                                           static_cast<DKImagePixelFormat>(pixelFormat),
                                           data.data(), data.size());
    bool result = false;
    if (context.error == DKImageEncodeError_Success) {
        fn(context.encodedData, context.encodedDataLength);
        result = true;
    }
    DKImageReleaseEncodeContext(&context);
    return result;
}

std::shared_ptr<Image> Image::resample(ImagePixelFormat format) const {
    return resample(width, height, format, ImageInterpolation::Nearest);
}

std::shared_ptr<Image> Image::resample(uint32_t width, uint32_t height,
                                       ImagePixelFormat format,
                                       ImageInterpolation interp) const {
    if (width == 0 || height == 0 || format == ImagePixelFormat::Invalid)
        return nullptr;
    if (width == this->width && height == this->height && format == this->pixelFormat) {
        if (auto p = const_cast<Image*>(this)->shared_from_this(); p)
            return p;
        // copy
        return std::make_shared<Image>(width, height, format, data.data());
    }

    if (getWriteFunction(format) == nullptr) {
        Log::error("Invalid output format!");
        return nullptr;
    }
    if (getReadFunction(this->pixelFormat) == nullptr) {
        Log::error("Invalid input format!");
        return nullptr;
    }

    uint32_t bpp = DKImagePixelFormatBytesPerPixel((DKImagePixelFormat)format);
    uint32_t rowStride = bpp * width;
    uint32_t bufferLength = rowStride * height;

    auto image = std::make_shared<Image>(width, height, format, nullptr);
    FVASSERT_DEBUG(bufferLength == image->data.size());

    if (this->width == width && this->height == height) {
        for (uint32_t ny = 0; ny < height; ++ny) {
            for (uint32_t nx = 0; nx < width; ++nx) {
                auto color = readPixel(nx, ny);
                image->writePixel(nx, ny, color);
            }
        }
    } else {
        const float scaleX = float(this->width) / float(width);
        const float scaleY = float(this->height) / float(height);

        for (uint32_t ny = 0; ny < height; ++ny) {
            for (uint32_t nx = 0; nx < width; ++nx) {
                // convert source location
                auto x = (float(nx) + 0.5f) * scaleX - 0.5f;
                auto y = (float(ny) + 0.5f) * scaleY - 0.5f;

                float x1 = x - scaleX * 0.5f;
                float x2 = x + scaleX * 0.5f;
                float y1 = y - scaleX * 0.5f;
                float y2 = y + scaleY * 0.5f;

                auto color = _interpolate(x1, x2, y1, y2, interp);
                image->writePixel(nx, ny, color);
            }
        }
    }
    return image;
}

Image::Pixel Image::readPixel(uint32_t x, uint32_t y) const {
    ReadFunction fn = getReadFunction(this->pixelFormat);
    if (fn == nullptr) {
        Log::error("Invalid pixel format!");
        return {};
    }

    uint32_t bpp = bytesPerPixel();
    x = std::clamp(x, 0U, this->width - 1);
    y = std::clamp(y, 0U, this->height - 1);
    size_t offset = size_t(y * this->width + x) * bpp;
    const uint8_t* sourceData = this->data.data();
    auto value = fn(sourceData, offset);
    return { value.r, value.g, value.b, value.a };
}

void Image::writePixel(uint32_t x, uint32_t y, const Pixel& value) {
    WriteFunction fn = getWriteFunction(this->pixelFormat);
    if (fn == nullptr) {
        Log::error("Invalid pixel format!");
        return;
    }

    uint32_t bpp = bytesPerPixel();
    x = std::clamp(x, 0U, this->width - 1);
    y = std::clamp(y, 0U, this->height - 1);
    size_t offset = size_t(y * this->width + x) * bpp;
    uint8_t* buffer = this->data.data();

    RawColorValue color = {
        std::clamp(value.r, 0.0, 1.0), 
        std::clamp(value.g, 0.0, 1.0),
        std::clamp(value.b, 0.0, 1.0),
        std::clamp(value.a, 0.0, 1.0) 
    };
    fn(buffer, offset, color);
}

Image::Pixel Image::interpolate(const Rect& rect, ImageInterpolation interp) const {
    if (rect.isNull()) 
        return { 0 };
    if (rect.isInfinite()) {
        return _interpolate(0, float(width), 0, float(height), interp);
    }
    return _interpolate(rect.minX(), rect.maxX(), rect.minY(), rect.maxY(), interp);
}

Image::Pixel Image::_interpolate(float _x1, float _x2, float _y1, float _y2, ImageInterpolation interp) const {
    
    ReadFunction readPixel = getReadFunction(this->pixelFormat);
    if (readPixel == nullptr) {
        Log::error("Invalid pixel format!");
        return {};
    }

    uint32_t bpp = bytesPerPixel();
    const uint8_t* data = this->data.data();

    auto getPixel = [&](float x, float y)->RawColorValue {
        int nx = std::clamp(int(x), 0, int(this->width) - 1);
        int ny = std::clamp(int(y), 0, int(this->height) - 1);
        size_t offset = size_t(ny * this->width + nx) * bpp;
        return readPixel(data, offset);
    };

    auto interpKernel = [&getPixel](double (*kernel)(float), float x, float y) -> RawColorValue {
        float fx = floor(x);
        float fy = floor(y);
        float px[4] = { fx - 1.0f, fx, fx + 1.0f, fx + 2.0f };
        float py[4] = { fy - 1.0f, fy, fy + 1.0f, fy + 2.0f };
        double kx[4] = { kernel(px[0] - x), kernel(px[1] - x), kernel(px[2] - x), kernel(px[3] - x) };
        double ky[4] = { kernel(py[0] - y), kernel(py[1] - y), kernel(py[2] - y), kernel(py[3] - y) };

        RawColorValue color = { 0, 0, 0, 0 };
        for (int yIndex = 0; yIndex < 4; ++yIndex) {
            for (int xIndex = 0; xIndex < 4; ++xIndex) {
                double k = kx[xIndex] * ky[yIndex];
                auto c = getPixel(px[xIndex], py[yIndex]);
                color = { color.r + c.r * k, color.g + c.g * k, color.b + c.b * k, color.a + c.a * k };
            }
        }
        return color;
    };

    std::function<RawColorValue(float, float)> interpolatePoint;
    switch (interp) {
    case ImageInterpolation::Nearest:
        interpolatePoint = [&](float x, float y)->RawColorValue {
            return getPixel(std::round(x), std::round(y));
        };
        break;
    case ImageInterpolation::Bilinear:
        interpolatePoint = [&](float x, float y)->RawColorValue {
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
        interpolatePoint = [&](float x, float y)->RawColorValue {
            auto kernelCubic = [](float t)->double {
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
        interpolatePoint = [&](float x, float y)->RawColorValue {
            auto kernelSpline = [](float _t)->double {
                constexpr auto f = double(1) / double(6);
                auto t = double(_t);
                if (t < -2.0) { return 0.0; }
                if (t < -1.0) { return (2.0 + t) * (2.0 + t) * (2.0 + t) * f; }
                if (t < 0.0) { return (4.0 + t * t * (-6.0 - 3.0 * t)) * f; }
                if (t < 1.0) { return (4.0 + t * t * (-6.0 + 3.0 * t)) * f; }
                if (t < 2.0) { return (2.0 - t) * (2.0 - t) * (2.0 - t) * f; }
                return 0.0;
            };
            return interpKernel(kernelSpline, x, y);
        };
        break;
    case ImageInterpolation::Gaussian:
        interpolatePoint = [&](float x, float y)->RawColorValue {
            auto kernelGaussian = [](float t)->double {
                return exp(-2.0 * double(t * t)) * 0.79788456080287;
            };
            return interpKernel(kernelGaussian, x, y);
        };
        break;
    case ImageInterpolation::Quadratic:
        interpolatePoint = [&](float x, float y)->RawColorValue {
            auto kernelQuadratic = [](float t)->double {
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

    float scaleX = abs(_x1 - _x2);
    float scaleY = abs(_y1 - _y2);

    auto interpolateBox = [&](float x, float y)->RawColorValue {
        auto x1 = x - scaleX * 0.5f;
        auto x2 = x + scaleX * 0.5f;
        auto y1 = y - scaleY * 0.5f;
        auto y2 = y + scaleY * 0.5f;

        RawColorValue color = { 0, 0, 0, 0 };
        for (auto ny = std::lround(y1), ny2 = std::lround(y2); ny <= ny2; ++ny) {
            for (auto nx = std::lround(x1), nx2 = std::lround(x2); nx <= nx2; ++nx) {
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
    if (scaleX < 1.0f && scaleY < 1.0f) {
        sample = interpolatePoint;
    } else {
        sample = interpolateBox;
    }

    float x = std::min(_x1, _x2);
    float y = std::min(_y1, _y2);
    auto color = sample(x, y);
    return { color.r, color.g, color.b, color.a };
}

std::shared_ptr<Texture> Image::makeTexture(CommandQueue* queue, uint32_t usage) const {
    if (queue == nullptr)
        return nullptr;

    PixelFormat textureFormat = PixelFormat::Invalid;
    ImagePixelFormat imageFormat = this->pixelFormat;

    switch (this->pixelFormat) {
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
        TextureDescriptor {
            TextureType2D,
            textureFormat,
            width,
            height,
            1, 1, 1, 1,
            TextureUsageCopyDestination | TextureUsageCopySource | usage
        });
    if (texture == nullptr)
        return nullptr;

    // create buffer for staging
    auto stgBuffer = device->makeBuffer(data.size(),
                                        GPUBuffer::StorageModeShared,
                                        CPUCacheModeWriteCombined);
    if (stgBuffer == nullptr) {
        Log::error("Failed to make buffer object.");
        return nullptr;
    }

    auto p = stgBuffer->contents();
    if (p == nullptr) {
        Log::error("Buffer memory mapping failed.");
        return nullptr;
    }

    memcpy(p, data.data(), data.size());
    stgBuffer->flush();

    auto commandBuffer = queue->makeCommandBuffer();
    if (commandBuffer == nullptr) {
        Log::error("Failed to make command buffer.");
        return nullptr;
    }

    auto encoder = commandBuffer->makeCopyCommandEncoder();
    if (encoder == nullptr) {
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

std::shared_ptr<Image> Image::fromTextureBuffer(std::shared_ptr<GPUBuffer> buffer,
                                                uint32_t width, uint32_t height,
                                                PixelFormat pixelFormat) {
    if (buffer == nullptr) {
        Log::error("Texture buffer should not be null");
        return nullptr;
    }
    if (width < 1 || height < 1) {
        Log::error("Invalid texture dimensions");
        return nullptr;
    }

    ImagePixelFormat imageFormat = ImagePixelFormat::Invalid;
    std::function<RawColorValue(const void*)> getPixel = nullptr;
    switch (pixelFormat) {
    case PixelFormat::R8Unorm:
    case PixelFormat::R8Uint:
        imageFormat = ImagePixelFormat::R8;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(*(uint8_t*)data), 0, 0, 1 };
        };
        break;
    case PixelFormat::R8Snorm:
    case PixelFormat::R8Sint:
        imageFormat = ImagePixelFormat::R8;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(*(int8_t*)data), 0, 0, 1 };
        };
        break;
    case PixelFormat::R16Unorm:
    case PixelFormat::R16Uint:
        imageFormat = ImagePixelFormat::R16;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(*(uint16_t*)data), 0, 0, 1 };
        };
        break;
    case PixelFormat::R16Snorm:
    case PixelFormat::R16Sint:
        imageFormat = ImagePixelFormat::R16;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(*(int16_t*)data), 0, 0, 1 };
        };
        break;
    case PixelFormat::R16Float:
        imageFormat = ImagePixelFormat::R16;
        getPixel = [](const void* data)->RawColorValue {
            return { *(Float16*)data, 0, 0, 1 };
        };
        break;
    case PixelFormat::RG8Unorm:
    case PixelFormat::RG8Uint:
        imageFormat = ImagePixelFormat::RG8;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((uint8_t*)data)[0]),
                     fixedToDouble(((uint8_t*)data)[1]),
                     0, 1 };
        };
        break;
    case PixelFormat::RG8Snorm:
    case PixelFormat::RG8Sint:
        imageFormat = ImagePixelFormat::RG8;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((int8_t*)data)[0]),
                     fixedToDouble(((int8_t*)data)[1]),
                     0, 1 };
        };
        break;
    case PixelFormat::R32Uint:
        imageFormat = ImagePixelFormat::R32;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(*(uint32_t*)data), 0, 0, 1 };
        };
        break;
    case PixelFormat::R32Sint:
        imageFormat = ImagePixelFormat::R32;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(*(int32_t*)data), 0, 0, 1 };
        };
        break;
    case PixelFormat::R32Float:
        imageFormat = ImagePixelFormat::R32F;
        getPixel = [](const void* data)->RawColorValue {
            return { *(float*)data, 0, 0, 1 };
        };
        break;
    case PixelFormat::RG16Unorm:
    case PixelFormat::RG16Uint:
        imageFormat = ImagePixelFormat::RG16;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((uint16_t*)data)[0]),
                     fixedToDouble(((uint16_t*)data)[1]),
                     0, 1 };
        };
        break;
    case PixelFormat::RG16Snorm:
    case PixelFormat::RG16Sint:
        imageFormat = ImagePixelFormat::RG16;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((int16_t*)data)[0]),
                     fixedToDouble(((int16_t*)data)[1]),
                     0, 1 };
        };
        break;
    case PixelFormat::RG16Float:
        imageFormat = ImagePixelFormat::RG16;
        getPixel = [](const void* data)->RawColorValue {
            return { ((Float16*)data)[0], ((Float16*)data)[1], 0, 1};
        };
        break;
    case PixelFormat::RGBA8Unorm:
    case PixelFormat::RGBA8Unorm_srgb:
    case PixelFormat::RGBA8Uint:
        imageFormat = ImagePixelFormat::RGBA8;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((uint8_t*)data)[0]),
                     fixedToDouble(((uint8_t*)data)[1]),
                     fixedToDouble(((uint8_t*)data)[2]),
                     fixedToDouble(((uint8_t*)data)[3]) };
        };
        break;
    case PixelFormat::RGBA8Snorm:
    case PixelFormat::RGBA8Sint:
        imageFormat = ImagePixelFormat::RGBA8;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((int8_t*)data)[0]),
                     fixedToDouble(((int8_t*)data)[1]),
                     fixedToDouble(((int8_t*)data)[2]),
                     fixedToDouble(((int8_t*)data)[3]) };
        };
        break;
    case PixelFormat::BGRA8Unorm:
    case PixelFormat::BGRA8Unorm_srgb:
        imageFormat = ImagePixelFormat::RGBA8;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((uint8_t*)data)[2]),
                     fixedToDouble(((uint8_t*)data)[1]),
                     fixedToDouble(((uint8_t*)data)[0]),
                     fixedToDouble(((uint8_t*)data)[3]) };
        };
        break;
    case PixelFormat::RGB10A2Unorm:
    case PixelFormat::RGB10A2Uint:
        imageFormat = ImagePixelFormat::RGBA16;
        getPixel = [](const void* data)->RawColorValue {
            uint32_t value = *(uint32_t*)data;
            return {
                double(value & 1023) / 1023.0,
                double((value >> 10) & 1023) / 1023.0,
                double((value >> 20) & 1023) / 1023.0,
                double((value >> 30) & 3) / 3.0,
            };
        };
        break;
    case PixelFormat::RG11B10Float:
        imageFormat = ImagePixelFormat::RGB16;
        getPixel = [](const void* data)->RawColorValue {
            uint32_t value = *(uint32_t*)data;
            return {
                ufloatToDouble<5, 6>(value >> 21),
                ufloatToDouble<5, 6>(value >> 10),
                ufloatToDouble<5, 5>(value),
                1.0 
            };
        };
        break;
    case PixelFormat::RGB9E5Float:
        imageFormat = ImagePixelFormat::RGB16;
        getPixel = [](const void* data)->RawColorValue {
            uint32_t value = *(uint32_t*)data;
            uint32_t exp = value & 31;
            return {
                ufloatToDouble<5, 9>(exp, value >> 23),
                ufloatToDouble<5, 9>(exp, value >> 14),
                ufloatToDouble<5, 9>(exp, value >> 5),
                1.0
            };
        };
        break;
    case PixelFormat::BGR10A2Unorm:
        imageFormat = ImagePixelFormat::RGBA16;
        getPixel = [](const void* data)->RawColorValue {
            uint32_t value = *(uint32_t*)data;
            return {
                double((value >> 20) & 1023) / 1023.0,
                double((value >> 10) & 1023) / 1023.0,
                double(value & 1023) / 1023.0,
                double((value >> 30) & 3) / 3.0,
            };
        };
        break;
    case PixelFormat::RG32Uint:
        imageFormat = ImagePixelFormat::RG32;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((uint32_t*)data)[0]),
                     fixedToDouble(((uint32_t*)data)[1]),
                     0, 1 };
        };
        break;
    case PixelFormat::RG32Sint:
        imageFormat = ImagePixelFormat::RG32;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((int32_t*)data)[0]),
                     fixedToDouble(((int32_t*)data)[1]),
                     0, 1 };
        };
        break;
    case PixelFormat::RG32Float:
        imageFormat = ImagePixelFormat::RG32F;
        getPixel = [](const void* data)->RawColorValue {
            return { ((float*)data)[0],
                     ((float*)data)[1],
                     0, 1 };
        };
        break;
    case PixelFormat::RGBA16Unorm:
    case PixelFormat::RGBA16Uint:
        imageFormat = ImagePixelFormat::RGBA16;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((uint16_t*)data)[0]),
                     fixedToDouble(((uint16_t*)data)[1]),
                     fixedToDouble(((uint16_t*)data)[2]),
                     fixedToDouble(((uint16_t*)data)[3]) };
        };
        break;
    case PixelFormat::RGBA16Snorm:
    case PixelFormat::RGBA16Sint:
        imageFormat = ImagePixelFormat::RGBA16;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((int16_t*)data)[0]),
                     fixedToDouble(((int16_t*)data)[1]),
                     fixedToDouble(((int16_t*)data)[2]),
                     fixedToDouble(((int16_t*)data)[3]) };
        };
        break;
    case PixelFormat::RGBA16Float:
        imageFormat = ImagePixelFormat::RGBA16;
        getPixel = [](const void* data)->RawColorValue {
            return { ((Float16*)data)[0],
                     ((Float16*)data)[1],
                     ((Float16*)data)[2],
                     ((Float16*)data)[3] };
        };
        break;
    case PixelFormat::RGBA32Uint:
        imageFormat = ImagePixelFormat::RGBA32;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((uint32_t*)data)[0]),
                     fixedToDouble(((uint32_t*)data)[1]),
                     fixedToDouble(((uint32_t*)data)[2]),
                     fixedToDouble(((uint32_t*)data)[3]) };
        };
        break;
    case PixelFormat::RGBA32Sint:
        imageFormat = ImagePixelFormat::RGBA32;
        getPixel = [](const void* data)->RawColorValue {
            return { fixedToDouble(((int32_t*)data)[0]),
                     fixedToDouble(((int32_t*)data)[1]),
                     fixedToDouble(((int32_t*)data)[2]),
                     fixedToDouble(((int32_t*)data)[3]) };
        };
        break;
    case PixelFormat::RGBA32Float:
        imageFormat = ImagePixelFormat::RGBA32F;
        getPixel = [](const void* data)->RawColorValue {
            return { ((float*)data)[0],
                     ((float*)data)[1],
                     ((float*)data)[2],
                     ((float*)data)[3] };
        };
        break;
    default:
        break;
    }

    if (getPixel == nullptr) {
        Log::error("Unsupported texture format! ({:d})", (int)pixelFormat);
        return nullptr;
    }
    FVASSERT_DEBUG(imageFormat != ImagePixelFormat::Invalid);

    uint32_t bpp = pixelFormatBytesPerPixel(pixelFormat);
    auto bufferLength = width * height * bpp;

    if (buffer->length() >= bufferLength) {
        uint8_t* p = (uint8_t*)buffer->contents();
        if (p) {
            auto image = std::make_shared<Image>(width, height, imageFormat);
            for (uint32_t y = 0; y < height; ++y) {
                for (uint32_t x = 0; x < width; ++x) {
                    auto c = getPixel(p);
                    p = p + bpp;

                    image->writePixel(x, y, { c.r, c.g, c.b, c.a });
                }
            }
            return image;
        } else {
            Log::error("Buffer is not accessible!");
        }
    }
    return nullptr;
}
