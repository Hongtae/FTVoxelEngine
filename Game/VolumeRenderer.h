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

    ViewTransform view;
    ProjectionTransform projection;
    Transform transform;
    Vector3 lightDir;

    std::shared_ptr<CommandQueue> queue;

    void setModel(std::shared_ptr<VoxelModel> model);
    std::shared_ptr<VoxelModel> model() const { return voxelModel; }

private:
    ComputePipeline raycastVoxel;
    ComputePipeline clearBuffers;
    std::optional<RenderPipeline> imageBlit;
    std::shared_ptr<GPUBuffer> imageBlitVB;

    std::shared_ptr<Texture> outputImage;  // storage-image
    std::shared_ptr<Texture> depthImage;

    std::shared_ptr<VoxelModel> voxelModel;
    struct VoxelLayer {
        AABB aabb;
        std::shared_ptr<GPUBuffer> buffer;
    };
    std::vector<VoxelLayer> voxelLayers;
    const uint32_t maxDepthLevel = 9U;
    const uint32_t maxStartLevel = 2U;
};
