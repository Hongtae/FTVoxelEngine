#include "../Libs/dkwrapper/DKImage.h"
#include "Image.h"

using namespace FV;

Image::Image()
{
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
    FVASSERT_DEBUG(byteCount == this->data.size());

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

std::shared_ptr<Image> Image::resample(uint32_t width, uint32_t height, ImagePixelFormat format, ImageInterpolation interpolation) const
{
    return nullptr;
}
