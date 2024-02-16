#pragma once
#include <memory>
#include <FVCore.h>

using namespace FV;

class Renderer {
public:
    virtual ~Renderer() {}

    virtual void initialize(std::shared_ptr<GraphicsDeviceContext>, std::shared_ptr<SwapChain>, PixelFormat) = 0;
    virtual void finalize() = 0;

    virtual void update(float delta) {}
    virtual void render(const RenderPassDescriptor&, const Rect&) = 0;
    virtual void prepareScene(const RenderPassDescriptor&, const ViewTransform&, const ProjectionTransform&) {}
};

struct RenderPipeline {
    std::shared_ptr<RenderPipelineState> state;
    std::shared_ptr<ShaderBindingSet> bindingSet;
};

struct ComputePipeline {
    std::shared_ptr<ComputePipelineState> state;
    std::shared_ptr<ShaderBindingSet> bindingSet;
    struct { uint32_t x, y, z; } threadgroupSize;
};

std::optional<RenderPipeline> makeRenderPipeline(
    GraphicsDevice* device,
    std::filesystem::path vsPath,
    std::filesystem::path fsPath,
    std::vector<ShaderSpecialization> vsSp,
    std::vector<ShaderSpecialization> fsSp,
    const VertexDescriptor& vertexDescriptor,
    std::vector<RenderPipelineColorAttachmentDescriptor> colorAttachments,
    PixelFormat depthStencilAttachmentPixelFormat,
    std::vector<ShaderBinding> bindings);

std::optional<ComputePipeline> makeComputePipeline(
    GraphicsDevice* device,
    std::filesystem::path path,
    std::vector<ShaderBinding> bindings);
