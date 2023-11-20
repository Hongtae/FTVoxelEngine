#pragma once
#include "Renderer.h"

class VolumeRenderer2 : public Renderer {
public:
    VolumeRenderer2();
    ~VolumeRenderer2();

    void initialize(std::shared_ptr<GraphicsDeviceContext>, std::shared_ptr<SwapChain>) override;
    void finalize() override;

    void prepareScene(const RenderPassDescriptor&, const ViewTransform& v, const ProjectionTransform& p) override;
    void render(const RenderPassDescriptor&, const Rect&) override;

    std::shared_ptr<ComputePipelineState> pipelineState;
    std::shared_ptr<Texture> renderTarget;
    std::shared_ptr<ShaderBindingSet> bindingSet;

    ViewTransform view;
    ProjectionTransform projection;
    Transform transform;
    Vector3 lightDir;

    std::shared_ptr<CommandQueue> queue;
    struct { uint32_t x, y, z; } threadgroupSize;

    void setVoxelModel(std::shared_ptr<VoxelModel> model);

private:    
    std::shared_ptr<VoxelModel> voxelModel;
    std::vector<std::shared_ptr<GPUBuffer>> voxelLayers;
    size_t layerDepth = 2;
    size_t maxDepthLevel = 6;
};
