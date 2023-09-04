#pragma once
#include "../include.h"
#include "DepthStencil.h"

namespace FV {
    enum class SamplerMinMagFilter {
        Nearest,
        Linear
    };

    enum class SamplerMipFilter {
        NotMipmapped,
        Nearest,
        Linear
    };

    enum class SamplerAddressMode {
        ClampToEdge,
        Repeat,
        MirrorRepeat,
        ClampToZero
    };

    struct SamplerDescriptor {
        using MinMagFilter = SamplerMinMagFilter;
        using MipFilter = SamplerMipFilter;
        using AddressMode = SamplerAddressMode;

        AddressMode addressModeU = AddressMode::ClampToEdge;
        AddressMode addressModeV = AddressMode::ClampToEdge;
        AddressMode addressModeW = AddressMode::ClampToEdge;

        MinMagFilter minFilter = MinMagFilter::Nearest;
        MinMagFilter magFilter = MinMagFilter::Nearest;
        MipFilter mipFilter = MipFilter::NotMipmapped;

        float lodMinClamp = 0.0f;
        float lodMaxClamp = 3.402823466e+38F; // FLT_MAX

        uint32_t maxAnisotropy = 1; /// Values must be between 1 and 16

        bool normalizedCoordinates = true;

        /// comparison function used when sampling texels from a depth texture.
        CompareFunction compareFunction = CompareFunctionNever;
    };

    class GraphicsDevice;
    class SamplerState {
    public:
        virtual ~SamplerState() {}
        virtual std::shared_ptr<GraphicsDevice> device() const = 0;
    };
}
