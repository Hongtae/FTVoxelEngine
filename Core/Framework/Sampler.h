#pragma once
#include "../include.h"
#include "DepthStencil.h"

namespace FV
{
    struct SamplerDescriptor
    {
        enum MinMagFilter
        {
            MinMagFilterNearest,
            MinMagFilterLinear,
        };
        enum MipFilter
        {
            MipFilterNotMipmapped,
            MipFilterNearest,
            MipFilterLinear,
        };
        enum AddressMode
        {
            AddressModeClampToEdge,
            AddressModeRepeat,
            AddressModeMirrorRepeat,
            AddressModeClampToZero,
        };

        AddressMode addressModeU = AddressModeClampToEdge;
        AddressMode addressModeV = AddressModeClampToEdge;
        AddressMode addressModeW = AddressModeClampToEdge;

        MinMagFilter minFilter = MinMagFilterNearest;
        MinMagFilter magFilter = MinMagFilterNearest;
        MipFilter mipFilter = MipFilterNotMipmapped;

        float minLod = 0.0f;
        float maxLod = 3.402823466e+38F; // FLT_MAX

        uint32_t maxAnisotropy = 1; /// Values must be between 1 and 16

        bool normalizedCoordinates = true;

        /// comparison function used when sampling texels from a depth texture.
        CompareFunction compareFunction = CompareFunctionNever;
    };

    class SamplerState
    {
    public:
        virtual ~SamplerState() {}
    };
}
