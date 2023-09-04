#pragma once
#include "../include.h"

namespace FV {
    enum CompareFunction {
        CompareFunctionNever,
        CompareFunctionLess,
        CompareFunctionEqual,
        CompareFunctionLessEqual,
        CompareFunctionGreater,
        CompareFunctionNotEqual,
        CompareFunctionGreaterEqual,
        CompareFunctionAlways,
    };

    enum StencilOperation {
        StencilOperationKeep,
        StencilOperationZero,
        StencilOperationReplace,
        StencilOperationIncrementClamp,
        StencilOperationDecrementClamp,
        StencilOperationInvert,
        StencilOperationIncrementWrap,
        StencilOperationDecrementWrap,
    };

    struct StencilDescriptor {
        CompareFunction stencilCompareFunction = CompareFunctionAlways;
        StencilOperation stencilFailureOperation = StencilOperationKeep;
        StencilOperation depthFailOperation = StencilOperationKeep;
        StencilOperation depthStencilPassOperation = StencilOperationKeep;
        uint32_t readMask = 0xffffffff;
        uint32_t writeMask = 0xffffffff;
    };

    struct DepthStencilDescriptor {
        CompareFunction depthCompareFunction = CompareFunctionAlways;
        StencilDescriptor frontFaceStencil;
        StencilDescriptor backFaceStencil;
        bool depthWriteEnabled = false;
    };

    class GraphicsDevice;
    class DepthStencilState {
    public:
        virtual ~DepthStencilState() {}
        virtual std::shared_ptr<GraphicsDevice> device() const = 0;
    };
}
