#pragma once
#include "Renderer.h"

enum class VisualMode: int {
    Raycast = 0,
    SSAO,
    Albedo,
    Composition,
};

class VolumeRenderer : public Renderer {
public:
    VolumeRenderer();
    ~VolumeRenderer();

    void initialize(std::shared_ptr<GraphicsDeviceContext>,
                    std::shared_ptr<SwapChain>,
                    PixelFormat) override;
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
        float distanceToMinDetail = 40.0f;
        uint32_t minDetailLevel = 7U;
        uint32_t maxDetailLevel = 12U;
        float ssaoRadius = 0.3f;
        float ssaoBias = 0.025f;
        bool ssaoBlur = true;
        bool ssaoBlur2p = false;
        float ssaoBlur2pRadius = 0.5f;
        bool linearFilter = false;
        VisualMode mode = VisualMode::Composition;
    } config;

private:
    ComputePipeline raycastVoxel;
    ComputePipeline raycastVisualizer;
    ComputePipeline clearBuffers;
    RenderPipeline ssao;
    RenderPipeline blur;
    RenderPipeline blur2;
    RenderPipeline composition;

    std::shared_ptr<GPUBuffer> ssaoKernel;
    std::shared_ptr<Texture> ssaoRandomNoise;

    std::shared_ptr<SamplerState> blitSamplerLinear;
    std::shared_ptr<SamplerState> blitSamplerNearest;

    std::shared_ptr<Texture> positionOutput;
    std::shared_ptr<Texture> albedoOutput;
    std::shared_ptr<Texture> normalOutput;
    std::shared_ptr<Texture> ssaoOutput;
    std::shared_ptr<Texture> blurOutput;

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
