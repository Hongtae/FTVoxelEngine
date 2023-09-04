#pragma once
#include "../include.h"
#include "RenderPass.h"
#include "RenderPipeline.h"
#include "CommandEncoder.h"
#include "GpuBuffer.h"
#include "ShaderBindingSet.h"

namespace FV {
    enum class VisibilityResultMode {
        Disabled,
        Boolean,
        Counting,
    };
    struct Viewport {
        float x;
        float y;
        float width;
        float height;
        float nearZ;
        float farZ;
    };
    struct ScissorRect {
        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
    };

    class RenderCommandEncoder : public CommandEncoder {
    public:
        virtual ~RenderCommandEncoder() {}

        virtual void setResources(uint32_t set, std::shared_ptr<ShaderBindingSet>) = 0;
        virtual void setViewport(const Viewport&) = 0;
        virtual void setScissorRect(const ScissorRect&) = 0;
        virtual void setRenderPipelineState(std::shared_ptr<RenderPipelineState> state) = 0;

        virtual void setVertexBuffer(std::shared_ptr<GPUBuffer> buffer, size_t offset, uint32_t index) = 0;
        virtual void setVertexBuffers(std::shared_ptr<GPUBuffer>* buffers, const size_t* offsets, uint32_t index, size_t count) = 0;

        virtual void setDepthStencilState(std::shared_ptr<DepthStencilState>) = 0;
        virtual void setDepthClipMode(DepthClipMode) = 0;
        virtual void setCullMode(CullMode) = 0;
        virtual void setFrontFacing(Winding) = 0;

        virtual void setBlendColor(float r, float g, float b, float a) = 0;
        virtual void setStencilReferenceValue(uint32_t) = 0;
        virtual void setStencilReferenceValues(uint32_t front, uint32_t back) = 0;
        virtual void setDepthBias(float depthBias, float slopeScale, float clamp) = 0;

        virtual void pushConstant(uint32_t stages, uint32_t offset, uint32_t size, const void*) = 0;

        virtual void draw(uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t baseInstance) = 0;
        virtual void drawIndexed(uint32_t indexCount, IndexType indexType, std::shared_ptr<GPUBuffer> indexBuffer, uint32_t indexBufferOffset, uint32_t instanceCount, uint32_t baseVertex, uint32_t baseInstance) = 0;
    };
}
