#pragma once

namespace FV {
    enum class PixelFormat {
        Invalid,

        // 8 bit formats
        R8Unorm,
        R8Snorm,
        R8Uint,
        R8Sint,

        // 16 bit formats
        R16Unorm,
        R16Snorm,
        R16Uint,
        R16Sint,
        R16Float,

        RG8Unorm,
        RG8Snorm,
        RG8Uint,
        RG8Sint,

        // 32 bit formats
        R32Uint,
        R32Sint,
        R32Float,

        RG16Unorm,
        RG16Snorm,
        RG16Uint,
        RG16Sint,
        RG16Float,

        RGBA8Unorm,
        RGBA8Unorm_srgb,
        RGBA8Snorm,
        RGBA8Uint,
        RGBA8Sint,

        BGRA8Unorm,
        BGRA8Unorm_srgb,

        // packed 32 bit formats
        RGB10A2Unorm,
        RGB10A2Uint,

        RG11B10Float,
        RGB9E5Float,

        // 64 bit formats
        RG32Uint,
        RG32Sint,
        RG32Float,

        RGBA16Unorm,
        RGBA16Snorm,
        RGBA16Uint,
        RGBA16Sint,
        RGBA16Float,

        // 128 bit formats
        RGBA32Uint,
        RGBA32Sint,
        RGBA32Float,

        // Depth
        Depth16Unorm,   // 16-bit normalized uint
        Depth32Float,   // 32-bit float

        // Stencil
        Stencil8,       // 8 bit uint stencil

        // Depth Stencil
        Depth24Unorm_stencil8, // 24-bit normalized uint depth, 8-bit uint stencil
        Depth32Float_stencil8, // 32-bit float depth, 8-bit uint stencil, 24-bit unused.
    };

    constexpr bool isColorFormat(PixelFormat f) {
        switch (f) {
        case PixelFormat::Invalid:
        case PixelFormat::Depth16Unorm:
        case PixelFormat::Depth32Float:
        case PixelFormat::Stencil8:
        case PixelFormat::Depth24Unorm_stencil8:
        case PixelFormat::Depth32Float_stencil8:
            return false;
        }
        return true;
    }

    constexpr bool isDepthFormat(PixelFormat f) {
        switch (f) {
        case PixelFormat::Depth16Unorm:
        case PixelFormat::Depth32Float:
        case PixelFormat::Depth24Unorm_stencil8:
        case PixelFormat::Depth32Float_stencil8:
            return true;
        }
        return false;
    }

    constexpr bool isStencilFormat(PixelFormat f) {
        switch (f) {
        case PixelFormat::Stencil8:
        case PixelFormat::Depth24Unorm_stencil8:
        case PixelFormat::Depth32Float_stencil8:
            return true;
        }
        return false;
    }

    constexpr uint32_t pixelFormatBytesPerPixel(PixelFormat f) {
        switch (f) {
            // 8 bit formats
        case PixelFormat::R8Unorm:
        case PixelFormat::R8Snorm:
        case PixelFormat::R8Uint:
        case PixelFormat::R8Sint:
            return 1;
            // 16 bit formats
        case PixelFormat::R16Unorm:
        case PixelFormat::R16Snorm:
        case PixelFormat::R16Uint:
        case PixelFormat::R16Sint:
        case PixelFormat::R16Float:
        case PixelFormat::RG8Unorm:
        case PixelFormat::RG8Snorm:
        case PixelFormat::RG8Uint:
        case PixelFormat::RG8Sint:
            return 2;
            // 32 bit formats
        case PixelFormat::R32Uint:
        case PixelFormat::R32Sint:
        case PixelFormat::R32Float:
        case PixelFormat::RG16Unorm:
        case PixelFormat::RG16Snorm:
        case PixelFormat::RG16Uint:
        case PixelFormat::RG16Sint:
        case PixelFormat::RG16Float:
        case PixelFormat::RGBA8Unorm:
        case PixelFormat::RGBA8Unorm_srgb:
        case PixelFormat::RGBA8Snorm:
        case PixelFormat::RGBA8Uint:
        case PixelFormat::RGBA8Sint:
        case PixelFormat::BGRA8Unorm:
        case PixelFormat::BGRA8Unorm_srgb:
            return 4;
            // packed 32 bit formats
        case PixelFormat::RGB10A2Unorm:
        case PixelFormat::RGB10A2Uint:
        case PixelFormat::RG11B10Float:
        case PixelFormat::RGB9E5Float:
            return 4;
            // 64 bit formats
        case PixelFormat::RG32Uint:
        case PixelFormat::RG32Sint:
        case PixelFormat::RG32Float:
        case PixelFormat::RGBA16Unorm:
        case PixelFormat::RGBA16Snorm:
        case PixelFormat::RGBA16Uint:
        case PixelFormat::RGBA16Sint:
        case PixelFormat::RGBA16Float:
            return 8;
            // 128 bit formats
        case PixelFormat::RGBA32Uint:
        case PixelFormat::RGBA32Sint:
        case PixelFormat::RGBA32Float:
            return 16;
            // Depth
        case PixelFormat::Depth16Unorm:
            return 2;
        case PixelFormat::Depth32Float:
            return 4;
            // Stencil (Uint)
        case PixelFormat::Stencil8:
            return 1;
            // Depth Stencil
        case PixelFormat::Depth24Unorm_stencil8:
            return 4;
        case PixelFormat::Depth32Float_stencil8: // 32-depth: 8-stencil: 24-unused.
            return 8;
        }
        return 0; // unsupported
    }
}
