#pragma once
#include "../include.h"
#include "CommandQueue.h"
#include "ShaderModule.h"
#include "Shader.h"
#include "RenderPipeline.h"
#include "ComputePipeline.h"
#include "DepthStencil.h"
#include "PipelineReflection.h"
#include "GPUBuffer.h"
#include "Texture.h"
#include "Sampler.h"
#include "GPUResource.h"
#include "ShaderBindingSet.h"

namespace FV
{
    class GraphicsDevice
    {
    public:
        virtual ~GraphicsDevice() {}

        virtual std::string deviceName() const = 0;

        virtual std::shared_ptr<CommandQueue> makeCommandQueue(uint32_t queueFlags) = 0;
        virtual std::shared_ptr<ShaderModule> makeShaderModule(const Shader&) = 0;
        virtual std::shared_ptr<ShaderBindingSet> makeShaderBindingSet(const ShaderBindingSetLayout&) = 0;

        virtual std::shared_ptr<RenderPipelineState> makeRenderPipeline(const RenderPipelineDescriptor&, PipelineReflection* reflection) = 0;
        virtual std::shared_ptr<ComputePipelineState> makeComputePipeline(const ComputePipelineDescriptor&, PipelineReflection* reflection) = 0;

        virtual std::shared_ptr<DepthStencilState> makeDepthStencilState(const DepthStencilDescriptor&) = 0;

        virtual std::shared_ptr<GPUBuffer> makeBuffer(size_t, GPUBuffer::StorageMode, CPUCacheMode) = 0;
        virtual std::shared_ptr<Texture> makeTexture(const TextureDescriptor&) = 0;
        virtual std::shared_ptr<Texture> makeTransientRenderTarget(TextureType, PixelFormat, uint32_t, uint32_t, uint32_t) = 0;

        virtual std::shared_ptr<SamplerState> makeSamplerState(const SamplerDescriptor&) = 0;
        virtual std::shared_ptr<GPUEvent> makeEvent() = 0;
        virtual std::shared_ptr<GPUSemaphore> makeSemaphore() = 0;
    };
}
