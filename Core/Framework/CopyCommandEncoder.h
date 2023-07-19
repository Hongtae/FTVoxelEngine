#pragma once
#include "../include.h"
#include "CommandEncoder.h"
#include "Texture.h"
#include "GPUBuffer.h"

namespace FV
{
    struct TextureSize
    {
        uint32_t width, height, depth;
    };

    struct TextureOrigin
    {
        uint32_t layer;
        uint32_t level;
        uint32_t x, y, z;       ///< pixel offset
    };

    struct BufferImageOrigin
    {
        size_t bufferOffset;    ///< buffer offset (bytes)
        uint32_t imageWidth;    ///< buffer image's width (pixels)
        uint32_t imageHeight;   ///< buffer image's height (pixels)
    };

    class CopyCommandEncoder : public CommandEncoder
    {
    public:
        virtual ~CopyCommandEncoder() {}

        virtual void copy(std::shared_ptr<GPUBuffer> src,
                          size_t srcOffset,
                          std::shared_ptr<GPUBuffer> dst,
                          size_t dstOffset,
                          size_t size) = 0;

        virtual void copy(std::shared_ptr<GPUBuffer> src,
                          const BufferImageOrigin& srcOffset,
                          std::shared_ptr<Texture> dst,
                          const TextureOrigin& dstOffset,
                          const TextureSize& size) = 0;

        virtual void copy(std::shared_ptr<Texture> src,
                          const TextureOrigin& srcOffset,
                          std::shared_ptr<GPUBuffer> dst,
                          const BufferImageOrigin& dstOffset,
                          const TextureSize& size) = 0;

        virtual void copy(std::shared_ptr<Texture> src,
                          const TextureOrigin& srcOffset,
                          std::shared_ptr<Texture> dst,
                          const TextureOrigin& dstOffset,
                          const TextureSize& size) = 0;

        virtual void fill(std::shared_ptr<GPUBuffer> buffer,
                          size_t offset,
                          size_t length,
                          uint8_t value) = 0;
    };
}
