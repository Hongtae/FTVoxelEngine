#pragma once
#include "../include.h"
#include "ShaderFunction.h"

namespace FV {
    struct ComputePipelineDescriptor {
        std::shared_ptr<ShaderFunction> computeFunction;
        bool deferCompile = false;
        bool disableOptimization = false;
    };

    class GraphicsDevice;
    class ComputePipelineState {
    public:
        virtual ~ComputePipelineState() = default;
        virtual std::shared_ptr<GraphicsDevice> device() const = 0;
    };
}
