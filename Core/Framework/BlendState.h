#pragma once
#include "../include.h"

namespace FV {
    enum class BlendFactor {
        Zero,
        One,
        SourceColor,
        OneMinusSourceColor,
        SourceAlpha,
        OneMinusSourceAlpha,
        DestinationColor,
        OneMinusDestinationColor,
        DestinationAlpha,
        OneMinusDestinationAlpha,
        SourceAlphaSaturated,
        BlendColor,
        OneMinusBlendColor,
        BlendAlpha,
        OneMinusBlendAlpha,
    };

    enum class BlendOperation {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max,
    };

    enum ColorWriteMask : uint8_t {
        ColorWriteMaskNone = 0,
        ColorWriteMaskRed = 0x1 << 3,
        ColorWriteMaskGreen = 0x1 << 2,
        ColorWriteMaskBlue = 0x1 << 1,
        ColorWriteMaskAlpha = 0x1 << 0,
        ColorWriteMaskAll = 0xf
    };

    struct FVCORE_API BlendState {
        bool enabled = false;

        BlendFactor sourceRGBBlendFactor = BlendFactor::One;
        BlendFactor sourceAlphaBlendFactor = BlendFactor::One;

        BlendFactor destinationRGBBlendFactor = BlendFactor::Zero;
        BlendFactor destinationAlphaBlendFactor = BlendFactor::Zero;

        BlendOperation rgbBlendOperation = BlendOperation::Add;
        BlendOperation alphaBlendOperation = BlendOperation::Add;

        ColorWriteMask writeMask = ColorWriteMaskAll;

        // preset
        static const BlendState	opaque;
        static const BlendState	alphaBlend;
        static const BlendState	multiply;
        static const BlendState	screen;
        static const BlendState	darken;
        static const BlendState	lighten;
        static const BlendState	linearBurn;
        static const BlendState	linearDodge;
    };
}