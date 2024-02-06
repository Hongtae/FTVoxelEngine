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

    ViewFrustum viewFrustum;
    Transform transform;
    Vector3 lightDir;
    float scale = 100.0f;

    std::shared_ptr<CommandQueue> queue;

    void setModel(std::shared_ptr<VoxelModel> model);
    std::shared_ptr<VoxelModel> model() const { return voxelModel; }

    struct {
        float renderScale = 0.5f;
        // distance from camera position
        float distanceToMaxDetail = 0.0f;
        float distanceToMinDetail = 100.0f;
        uint32_t minDetailLevel = 7U;
        uint32_t maxDetailLevel = 13U;
        bool linearFilter = false;
    } config;

private:
    ComputePipeline raycastVoxel;
    ComputePipeline clearBuffers;
    std::optional<RenderPipeline> imageBlit;
    std::shared_ptr<GPUBuffer> imageBlitVB;
    std::shared_ptr<SamplerState> blitSamplerLinear;
    std::shared_ptr<SamplerState> blitSamplerNearest;

    std::shared_ptr<Texture> outputImage;  // storage-image
    std::shared_ptr<Texture> depthImage;

    std::shared_ptr<VoxelModel> voxelModel;
    struct VoxelLayer {
        AABB aabb;
        std::shared_ptr<GPUBuffer> buffer;
    };
    std::vector<VoxelLayer> voxelLayers;

    struct VolumeDataCache {
        std::vector<VolumeArray::Node> data;
        uint32_t depth;
    };
    struct {
        std::unordered_map<const VoxelOctree*, VolumeDataCache> volumeMap;
        uint32_t layerDepth = VoxelOctree::maxDepth + 1;
        size_t maxNodeCount = 0;
    } cachedData;
};
