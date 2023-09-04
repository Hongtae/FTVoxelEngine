#pragma once
#include "../include.h"
#include "CommandEncoder.h"
#include "ComputePipeline.h"
#include "ShaderBindingSet.h"

namespace FV {
    class ComputeCommandEncoder : public CommandEncoder {
    public:
        virtual ~ComputeCommandEncoder() {}

        virtual void setResources(uint32_t set, std::shared_ptr<ShaderBindingSet>) = 0;
        virtual void setComputePipelineState(std::shared_ptr<ComputePipelineState>) = 0;

        virtual void pushConstant(uint32_t stages, uint32_t offset, uint32_t size, const void*) = 0;
        virtual void dispatch(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ) = 0;
    };
}
