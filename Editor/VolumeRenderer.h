#pragma once
#include "Renderer.h"

class VolumeRenderer : public Renderer {
public:
    VolumeRenderer();
    ~VolumeRenderer();

    void initialize(std::shared_ptr<GraphicsDeviceContext>, std::shared_ptr<SwapChain>) override;
    void finalize() override;

    void prepareScene(const RenderPassDescriptor&, const ViewTransform& v, const ProjectionTransform& p) override;
    void render(const RenderPassDescriptor&, const Rect&) override;

    void setOctreeLayer(std::shared_ptr<AABBOctreeLayer> layer);
    const AABBOctreeLayer* layer() const { return aabbOctreeLayer.get(); }

    float bestFitDepth() const;

    std::shared_ptr<AABBOctree> aabbOctree;

    std::shared_ptr<ComputePipelineState> pipelineState;
    std::shared_ptr<Texture> texture;
    std::shared_ptr<ShaderBindingSet> bindingSet;

    ViewTransform view;
    ProjectionTransform projection;
    Transform transform;
    Vector3 lightDir;

    std::shared_ptr<CommandQueue> queue;
    struct { uint32_t x, y, z; } threadgroupSize;

private:
    std::shared_ptr<AABBOctreeLayer> aabbOctreeLayer;
    std::shared_ptr<GPUBuffer> aabbOctreeLayerBuffer;
};
