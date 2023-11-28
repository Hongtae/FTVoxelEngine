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

    std::shared_ptr<Texture> renderTarget;

    ViewTransform view;
    ProjectionTransform projection;
    Transform transform;
    Vector3 lightDir;

    std::shared_ptr<CommandQueue> queue;

    void setVoxelModel(std::shared_ptr<VoxelModel> model);

private:
    struct PipelineState {
        std::shared_ptr<ComputePipelineState> pso;
        std::shared_ptr<ShaderBindingSet> bindingSet;
        struct { uint32_t x, y, z; } threadgroupSize;
    };
    PipelineState raycastVoxel;
    PipelineState clearBuffers;

    std::optional<PipelineState> loadPipeline(
        std::filesystem::path path,
        std::vector<ShaderBinding> bindings);

    std::shared_ptr<Texture> renderTargetR32F;

    std::shared_ptr<VoxelModel> voxelModel;
    struct VoxelLayer {
        AABB aabb;
        std::shared_ptr<GPUBuffer> buffer;
    };
    std::vector<VoxelLayer> voxelLayers;
    const uint32_t maxDepthLevel = 9;
};
