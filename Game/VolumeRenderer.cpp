#include <random>
#include "ShaderReflection.h"
#include "VolumeRenderer.h"

extern std::filesystem::path appResourcesRoot;

constexpr int SSAOKernelSize = 64;

VolumeRenderer::VolumeRenderer()
    : viewFrustum{ {}, {} }
    , transform{}
    , lightDir{ 0, 1, 0 } {
}

VolumeRenderer::~VolumeRenderer() {
}

void VolumeRenderer::initialize(std::shared_ptr<GraphicsDeviceContext> gc,
                                std::shared_ptr<SwapChain> swapchain,
                                PixelFormat depthFormat) {
    this->queue = swapchain->queue();
    FVASSERT_DEBUG(this->queue->flags() & CommandQueue::Compute);

    auto device = gc->device.get();

    // load shader
    if (auto pso = makeComputePipeline(
        device,
        { appResourcesRoot / "Shaders/voxel_depth_clear.comp.spv" },
        {
            { 0, ShaderDescriptorType::StorageTexture, 1, nullptr }, // color (rgba8)
            { 1, ShaderDescriptorType::StorageTexture, 1, nullptr }, // depth (r32f)
            { 2, ShaderDescriptorType::StorageTexture, 1, nullptr }, // normal (rgb10_a2)
        }); pso.has_value()) {
        clearBuffers = pso.value();
    } else {
        throw std::runtime_error("failed to load shader");
    }

    int writeRayIteration = 0;

    if (auto pso = makeComputePipeline(
        device,
        { appResourcesRoot / "Shaders/voxel_depth_layer.comp.spv", {
            // shader specialized constants
            { ShaderDataType::Int, &writeRayIteration, 0, sizeof(int)} }
        },
        {
            { 0, ShaderDescriptorType::StorageTexture, 1, nullptr }, // color (rgba8)
            { 1, ShaderDescriptorType::StorageTexture, 1, nullptr }, // depth (r32f)
            { 2, ShaderDescriptorType::StorageTexture, 1, nullptr }, // normal (rgb10_a2)
            { 3, ShaderDescriptorType::StorageBuffer, 1, nullptr },  // voxel data
        }); pso.has_value()) {
        raycastVoxel = pso.value();
    } else {
        throw std::runtime_error("failed to load shader");
    }

    writeRayIteration = 1;
    if (auto pso = makeComputePipeline(
        device,
        { appResourcesRoot / "Shaders/voxel_depth_layer.comp.spv", {
            // shader specialized constants
            {ShaderDataType::Int, &writeRayIteration, 0, sizeof(int)}}
        },
        {
            { 0, ShaderDescriptorType::StorageTexture, 1, nullptr }, // color (rgba8)
            { 1, ShaderDescriptorType::StorageTexture, 1, nullptr }, // depth (r32f)
            { 2, ShaderDescriptorType::StorageTexture, 1, nullptr }, // normal (rgb10_a2)
            { 3, ShaderDescriptorType::StorageBuffer, 1, nullptr },  // voxel data
        }); pso.has_value()) {
        raycastVisualizer = pso.value();
    } else {
        throw std::runtime_error("failed to load shader");
    }

    blitSamplerLinear = device->makeSamplerState(
        {
    .minFilter = SamplerMinMagFilter::Linear,
    .magFilter = SamplerMinMagFilter::Linear,
        });
    blitSamplerNearest = device->makeSamplerState(
        {
    .minFilter = SamplerMinMagFilter::Nearest,
    .magFilter = SamplerMinMagFilter::Nearest,
        });
    FVASSERT_DEBUG(blitSamplerLinear);
    FVASSERT_DEBUG(blitSamplerNearest);

    // ssao sample kernel
    std::random_device rd;
    std::default_random_engine re1(rd());
    std::uniform_real_distribution<float> rdist(0.0f, 1.0f);
    std::vector<Vector4> ssaoKernel(SSAOKernelSize);
    for (uint32_t i = 0; i < SSAOKernelSize; ++i) {
        auto sample = Vector3(rdist(re1) * 2.0 - 1.0,
                              rdist(re1) * 2.0 - 1.0,
                              rdist(re1));
        sample.normalize();
        sample *= rdist(re1);
        float scale = float(i) / float(SSAOKernelSize);
        scale = std::lerp(0.1f, 1.0f, scale * scale);
        ssaoKernel[i] = Vector4(sample * scale, 0.0f);
    }
    auto makeBuffer = [&](const void* data, size_t length) -> std::shared_ptr<GPUBuffer> {
        auto stgBuffer = device->makeBuffer(length,
                                            GPUBuffer::StorageModeShared,
                                            CPUCacheModeWriteCombined);
        FVASSERT(stgBuffer);
        if (auto p = stgBuffer->contents(); p) {
            memcpy(p, data, length);
            stgBuffer->flush();
        } else {
            Log::error("GPUBuffer map failed.");
            FVERROR_ABORT("GPUBuffer map failed.");
            return nullptr;
        }
        auto cbuffer = queue->makeCommandBuffer();
        auto buffer = device->makeBuffer(length,
                                         GPUBuffer::StorageModePrivate,
                                         CPUCacheModeDefault);
        FVASSERT(buffer);
        auto encoder = cbuffer->makeCopyCommandEncoder();
        FVASSERT(encoder);
        encoder->copy(stgBuffer, 0, buffer, 0, length);
        encoder->endEncoding();
        cbuffer->commit();
        return buffer;
    };
    FVASSERT_DEBUG(ssaoKernel.size() == SSAOKernelSize);
    this->ssaoKernel = makeBuffer(ssaoKernel.data(),
                                  sizeof(Vector4) * SSAOKernelSize);
    FVASSERT_DEBUG(this->ssaoKernel);
    // random noise
    constexpr auto ssaoNoiseDimension = 4;
    std::vector<Vector4> noiseValues(ssaoNoiseDimension * ssaoNoiseDimension);
    for (uint32_t i = 0; i < static_cast<uint32_t>(noiseValues.size()); i++) {
        noiseValues[i] = Vector4(rdist(re1) * 2.0f - 1.0f,
                                 rdist(re1) * 2.0f - 1.0f,
                                 0.0f, 0.0f);
    }
    this->ssaoRandomNoise = Image(ssaoNoiseDimension, ssaoNoiseDimension,
                                  ImagePixelFormat::RGBA32F,
                                  noiseValues.data()).makeTexture(queue.get());

    const int ssaoKernelSize = SSAOKernelSize;
    PixelFormat ssaoFormat = PixelFormat::R8Unorm;

    if (auto pso = makeRenderPipeline(
        device,
        {appResourcesRoot / "Shaders/screen.vert.spv"},
        { appResourcesRoot / "Shaders/ssao.frag.spv", {
                // shader specialized constants
                {ShaderDataType::Int, &ssaoKernelSize, 0, sizeof(int)},
            } 
        },
        {},             // VertexDescriptor        
        {
            { 0, ssaoFormat, BlendState::opaque },
        },              // RenderPipelineColorAttachmentDescriptor
        depthFormat,    // depthStencilAttachmentPixelFormat
        {
            // ShaderBinding
            { 0, ShaderDescriptorType::TextureSampler, 1, nullptr },
            { 1, ShaderDescriptorType::TextureSampler, 1, nullptr },
            { 2, ShaderDescriptorType::TextureSampler, 1, nullptr },
            { 3, ShaderDescriptorType::UniformBuffer, 1, nullptr },
        }); pso.has_value()) {
        ssao = pso.value();
        for (int i = 0; i < 3; ++i)
            ssao.bindingSet->setSamplerState(i, blitSamplerLinear);
        ssao.bindingSet->setTexture(2, ssaoRandomNoise);
        ssao.bindingSet->setBuffer(3, this->ssaoKernel, 0, this->ssaoKernel->length());
    } else {
        throw std::runtime_error("failed to load shader");
    }

    if (auto pso = makeRenderPipeline(
        device,
        { appResourcesRoot / "Shaders/screen.vert.spv"},
        { appResourcesRoot / "Shaders/blur.frag.spv"},
        {},             // VertexDescriptor        
        {
            { 0, ssaoFormat, BlendState::opaque },
        },              // RenderPipelineColorAttachmentDescriptor
        depthFormat,    // depthStencilAttachmentPixelFormat
        {
            // ShaderBinding
            { 0, ShaderDescriptorType::TextureSampler, 1, nullptr },
        }); pso.has_value()) {
        blur = pso.value();
        blur.bindingSet->setSamplerState(0, blitSamplerLinear);
    } else {
        throw std::runtime_error("failed to load shader");
    }
    if (auto pso = makeRenderPipeline(
        device,
        { appResourcesRoot / "Shaders/screen.vert.spv"},
        { appResourcesRoot / "Shaders/blur2.frag.spv"},
        {},             // VertexDescriptor        
        {
            { 0, ssaoFormat, BlendState::opaque },
        },              // RenderPipelineColorAttachmentDescriptor
        depthFormat,    // depthStencilAttachmentPixelFormat
        {
            // ShaderBinding
            { 0, ShaderDescriptorType::TextureSampler, 1, nullptr },
        }); pso.has_value()) {
        blur2 = pso.value();
        blur2.bindingSet->setSamplerState(0, blitSamplerLinear);
    } else {
        throw std::runtime_error("failed to load shader");
    }
    auto colorFormat = swapchain->pixelFormat();
    if (auto pso = makeRenderPipeline(
        device,
        { appResourcesRoot / "Shaders/screen.vert.spv"},
        { appResourcesRoot / "Shaders/composition.frag.spv"},
        {}, // VertexDescriptor
            {
                // RenderPipelineColorAttachmentDescriptor
                { 0, colorFormat, BlendState::alphaBlend },
            },
            depthFormat,    // depthStencilAttachmentPixelFormat
        {
            // ShaderBinding
            { 0, ShaderDescriptorType::TextureSampler, 1, nullptr }, // position;
            { 1, ShaderDescriptorType::TextureSampler, 1, nullptr }, // normal;
            { 2, ShaderDescriptorType::TextureSampler, 1, nullptr }, // albedo;
            { 3, ShaderDescriptorType::TextureSampler, 1, nullptr }, // ssao;
        }); pso.has_value()) {
        composition = pso.value();
        for (int i = 0; i < 4; ++i)
            composition.bindingSet->setSamplerState(i, blitSamplerLinear);
    } else {
        throw std::runtime_error("failed to load shader");
    }
}

void VolumeRenderer::finalize() {
    positionOutput = nullptr;
    albedoOutput = nullptr;
    normalOutput = nullptr;

    voxelModel = nullptr;
    voxelLayers.clear();

    raycastVoxel = {};
    raycastVisualizer = {};
    clearBuffers = {};
    ssao = {};
    blur = {};
    composition = {};

    queue = nullptr;
}

void VolumeRenderer::setModel(std::shared_ptr<VoxelModel> model) {
    voxelModel = model;
    voxelLayers.clear();
}

bool stopUpdating = false;
bool stopCaching = false;

void VolumeRenderer::prepareScene(const RenderPassDescriptor& rp,
                                  const ViewTransform& v,
                                  const ProjectionTransform& p) {
    ViewTransform view = v;
    ProjectionTransform projection = p;

    auto renderTarget = rp.colorAttachments.front().renderTarget;

    uint32_t width = renderTarget->width();
    uint32_t height = renderTarget->height();

    width = width * config.renderScale;
    height = height * config.renderScale;

    bool resetRaycastAttachments = true;
    if (positionOutput) {
        if (positionOutput->width() == width && positionOutput->height() == height)
            resetRaycastAttachments = false;
    }

    auto device = queue->device();

    if (resetRaycastAttachments) {
        uint32_t textureUsage = TextureUsageCopyDestination |
            TextureUsageCopySource |
            TextureUsageSampled |
            TextureUsageStorage |
            TextureUsageShaderRead |
            TextureUsageShaderWrite;

        // create position-depth render-target (rgba32f)
        positionOutput = device->makeTexture(
            TextureDescriptor{
                TextureType2D,
                PixelFormat::RGBA32Float,
                width,
                height,
                1, 1, 1, 1,
                textureUsage
            });
        FVASSERT_DEBUG(positionOutput);

        // create albedo render-target (rgba8)
        albedoOutput = device->makeTexture(
            TextureDescriptor{
                TextureType2D,
                PixelFormat::RGBA8Unorm,
                width,
                height,
                1, 1, 1, 1,
                textureUsage
            });
        FVASSERT_DEBUG(albedoOutput);

        // create normal render-target (rgba8)
        normalOutput = device->makeTexture(
            TextureDescriptor{
                TextureType2D,
                PixelFormat::RGBA8Unorm,
                width,
                height,
                1, 1, 1, 1,
                textureUsage
            });
        FVASSERT_DEBUG(normalOutput);

        for (auto& pipeline : { clearBuffers, raycastVoxel, raycastVisualizer }) {
            pipeline.bindingSet->setTexture(0, positionOutput);
            pipeline.bindingSet->setTexture(1, albedoOutput);
            pipeline.bindingSet->setTexture(2, normalOutput);
        }

        Log::debug("VolumeRenderer: resetImages ({}x{})", width, height);

        ssao.bindingSet->setTexture(0, positionOutput);
        ssao.bindingSet->setTexture(1, normalOutput);

        composition.bindingSet->setTexture(0, positionOutput);
        composition.bindingSet->setTexture(1, normalOutput);
        composition.bindingSet->setTexture(2, albedoOutput);
    }

    //width = renderTarget->width();
    //height = renderTarget->height();

    bool resetSSAOAttachments = true;
    if (ssaoOutput) {
        if (ssaoOutput->width() == width && ssaoOutput->height() == height)
            resetSSAOAttachments = false;
    }
    if (resetSSAOAttachments) {
        uint32_t textureUsage = TextureUsageRenderTarget |
            TextureUsageSampled |
            TextureUsageShaderRead;

        ssaoOutput = device->makeTexture(
            TextureDescriptor{
                TextureType2D,
                PixelFormat::R8Unorm,
                width,
                height,
                1, 1, 1, 1,
                textureUsage
            });
        FVASSERT_DEBUG(ssaoOutput);

        blurOutput = device->makeTexture(
            TextureDescriptor{
                TextureType2D,
                PixelFormat::R8Unorm,
                width,
                height,
                1, 1, 1, 1,
                textureUsage
            });
        FVASSERT_DEBUG(blurOutput);

        blur.bindingSet->setTexture(0, ssaoOutput);
        composition.bindingSet->setTexture(3, ssaoOutput);
    }

    if (albedoOutput) {
        uint32_t width = albedoOutput->width();
        uint32_t height = albedoOutput->height();

        auto proj = p;
        if (proj.matrix._34 != 0.0f) {
            auto f = p.matrix._22;
            auto aspect = float(width) / float(height);
            proj.matrix._11 = f / aspect;
        }
        projection = proj;
    }
    this->viewFrustum = { view, projection };

    if (stopUpdating)
        return;

    // calculate volume depth
    voxelLayers.clear();
    if (voxelModel) {

        auto modelTransform = AffineTransform3();
        modelTransform.scale({ scale, scale, scale });
        modelTransform.concatenate(AffineTransform3{ transform.orientation.matrix3(), transform.position });

        auto modelView = ViewTransform{
            modelTransform.matrix3,
            modelTransform.translation }.concatenate(view);
        ViewFrustum mvpFrustum = { modelView, projection };

        auto mv = modelView.matrix4();
        auto mvp = mvpFrustum.matrix();

        auto viewPosition = view.position();

        auto calculateDepthLevel = [&](const AABB& aabb, uint32_t width, uint32_t height) {
            Vector3 aabbCornerVertices[8] = {
                {aabb.min.x, aabb.min.y, aabb.min.z},
                {aabb.max.x, aabb.min.y, aabb.min.z},
                {aabb.min.x, aabb.max.y, aabb.min.z},
                {aabb.max.x, aabb.max.y, aabb.min.z},
                {aabb.min.x, aabb.min.y, aabb.max.z},
                {aabb.max.x, aabb.min.y, aabb.max.z},
                {aabb.min.x, aabb.max.y, aabb.max.z},
                {aabb.max.x, aabb.max.y, aabb.max.z},
            };

            for (auto& v : aabbCornerVertices) { v.apply(mvp, 1.0); }

            struct { float minX, maxX, minY, maxY; } ndcMinMaxPoint = {
                aabbCornerVertices[0].x,
                aabbCornerVertices[0].x,
                aabbCornerVertices[0].y,
                aabbCornerVertices[0].y,
            };
            for (int i = 1; i < 8; ++i) {
                const auto& v = aabbCornerVertices[i];
                ndcMinMaxPoint.minX = std::min(ndcMinMaxPoint.minX, v.x);
                ndcMinMaxPoint.maxX = std::max(ndcMinMaxPoint.maxX, v.x);
                ndcMinMaxPoint.minY = std::min(ndcMinMaxPoint.minY, v.y);
                ndcMinMaxPoint.maxY = std::max(ndcMinMaxPoint.maxY, v.y);
            }
            auto pixelsX = (ndcMinMaxPoint.maxX - ndcMinMaxPoint.minX) * float(width - 1) * 0.5f;
            auto pixelsY = (ndcMinMaxPoint.maxY - ndcMinMaxPoint.minY) * float(height - 1) * 0.5f;
            auto effectivePixels = std::max(pixelsX, pixelsY);
            if (effectivePixels > 1.0) {
                return std::min(std::log2(effectivePixels), 125.0f);
            }
            return 0.0f;
        };

        bool printDebugInfo = false;
        auto timestamp = std::chrono::high_resolution_clock::now();
        if (!printDebugInfo) {
            static std::chrono::time_point<std::chrono::high_resolution_clock> _timestamp = {};
            std::chrono::duration<double> d = timestamp - _timestamp;
            if (d.count() > 2.0) {
                _timestamp = timestamp;
                printDebugInfo = true;
            }
        }

        AABB aabb = { Vector3::zero, {1, 1, 1} };
        if (mvpFrustum.isAABBInside(aabb)) {
            const VoxelOctree* root = voxelModel->root();

            uint32_t minDetailLevel = config.minDetailLevel;
            uint32_t maxDetailLevel = std::max(config.maxDetailLevel, minDetailLevel);

            uint32_t depthLevel = voxelModel->depth();
            uint32_t bestFitDepth = (uint32_t)calculateDepthLevel(aabb, width, height);
            uint32_t startLevel = 0;
            if (depthLevel > minDetailLevel)
                startLevel = minDetailLevel;

            uint32_t _debug_numIterations = 0;
            uint32_t _debug_numCulling = 0;

            VolumeArray volumeData = {};
            if (startLevel > 0) {
                auto distMaxDetail = config.distanceToMaxDetail;
                auto distMinDetail = config.distanceToMinDetail;
                distMinDetail = std::max(distMinDetail, distMaxDetail + 0.001f);

                struct DepthResolveRecursion {
                    uint32_t startLevel;
                    std::function<uint32_t(const Vector3&, uint32_t)>& bestFit;
                    const VoxelOctree::VolumePriorityCallback& getPriority;
                    std::function<void (const Vector3&, uint32_t, const VoxelOctree*, uint32_t, std::vector<VolumeArray::Node>&)>& generator;
                    void operator() (const Vector3& center,
                                     uint32_t depth,
                                     float priority,
                                     const VoxelOctree* node,
                                     std::vector<VolumeArray::Node>& vector) const {
                        if (depth == startLevel) {
                            uint32_t maxDepth = bestFit(center, depth);
                            if (stopCaching || maxDepth < depth) {
                                node->makeSubarray(center, depth, maxDepth, vector);
                            } else {
                                generator(center, depth, node, maxDepth, vector);
                            }
                        } else {
                            node->makeSubarray(center, depth, *this, getPriority, vector);
                        }
                    }
                };

                std::function bestFit = [&]
                (const Vector3& pos, uint32_t depth) -> uint32_t {
                    _debug_numIterations++;
                    float hext = VoxelOctree::halfExtent(depth);
                    AABB aabb = {
                        pos - Vector3(hext, hext, hext),
                        pos + Vector3(hext, hext, hext)
                    };
                    if (mvpFrustum.isAABBInside(aabb)) {
                        auto p = pos.applying(modelView.transform());
                        float distanceFromView = fabs(p.z);
                        float bestFit = calculateDepthLevel(aabb, width, height);
                        if (auto k = distanceFromView - distMaxDetail; k > 0.0f) {
                            k = k / (distMinDetail - distMaxDetail);
                            k = 1.0f - std::clamp(k, 0.0f, 1.0f);
                            bestFit = bestFit * k;
                        }
                        return std::min(uint32_t(bestFit) + depth, maxDetailLevel);
                    }
                    _debug_numCulling++;
                    return 0;
                };

                VoxelOctree::VolumePriorityCallback getPriority = [&]
                (const Vector3& pos, uint32_t depth) -> float {
                    auto p = pos.applying(modelView.transform());
                    return p.z;
                };

                std::function generator = [&](const Vector3& center,
                                              uint32_t depth,
                                              const VoxelOctree* node,
                                              uint32_t maxDepth,
                                              std::vector<VolumeArray::Node>& vector) {
                    bool build = true;
                    if (auto iter = cachedData.volumeMap.find(node);
                        iter != cachedData.volumeMap.end()) {
                        VolumeDataCache& cache = iter->second;
                        if (cache.depth == maxDepth) {
                            build = false;
                        }
                    } else {
                        cachedData.volumeMap[node] = { {}, maxDepth };
                    }
                    VolumeDataCache& cache = cachedData.volumeMap.at(node);
                    if (build) {
                        cache.data.clear();
                        cache.depth = maxDepth;
                        node->makeSubarray(center, depth, maxDepth, cache.data);
                    }
                    vector.insert(vector.end(), cache.data.begin(), cache.data.end());
                };

                if (startLevel != cachedData.layerDepth) {
                    Log::info(enUS_UTF8,
                              "Volume cache clear with new depth-level:{}, (previous depth:{}), peak-count:{:Ld}",
                              startLevel,
                              cachedData.layerDepth,
                              cachedData.maxNodeCount);
                    cachedData.volumeMap.clear();
                    cachedData.layerDepth = startLevel;
                    cachedData.maxNodeCount = 0;
                }

                volumeData = root->makeArray([&](const Vector3& center,
                                                 uint32_t depth,
                                                 float priority,
                                                 const VoxelOctree* node,
                                                 std::vector<VolumeArray::Node>& vector) {
                    vector.reserve(cachedData.maxNodeCount);
                    DepthResolveRecursion{
                        startLevel, bestFit, getPriority, generator
                    }(center, depth, priority, node, vector);
                });
                cachedData.maxNodeCount = std::max(volumeData.data.size(), cachedData.maxNodeCount);
            } else {
                volumeData = root->makeArray(std::min(maxDetailLevel, bestFitDepth));
            }

            if (volumeData.data.empty() == false) {
                size_t numNodes = volumeData.data.size();
                const auto dataLength = sizeof(VolumeArray::Node) * numNodes;
                auto buffer = device->makeBuffer(dataLength,
                                                 GPUBuffer::StorageModeShared,
                                                 CPUCacheModeWriteCombined);
                uint8_t* p = (uint8_t*)buffer->contents();
                memcpy(p, volumeData.data.data(), dataLength);
                buffer->flush();

                VoxelLayer layer = {
                    aabb,
                    buffer
                };
                this->voxelLayers.push_back(layer);
            }

            auto t = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> d = t - timestamp;

            if (printDebugInfo) {
                Log::debug(enUS_UTF8,
                           "VoxelModel nodes:{:Ld} (start:{}, depth:{}, bestfit:{}). iteration:{} cull:{}, elapsed: {:.4f}",
                           volumeData.data.size(),
                           startLevel,
                           depthLevel,
                           bestFitDepth,
                           _debug_numIterations,
                           _debug_numCulling,
                           d.count());
            }
        } else {
            if (printDebugInfo) {
                Log::debug("AABB is not visible!");
            }
        }
    }
}

bool stopRendering = false;

void VolumeRenderer::render(const RenderPassDescriptor& rp, const Rect& frame) {
    if (!stopRendering && positionOutput && albedoOutput && normalOutput) {
        uint32_t width = albedoOutput->width();
        uint32_t height = albedoOutput->height();
        FVASSERT_DEBUG(positionOutput->width() == width);
        FVASSERT_DEBUG(positionOutput->height() == height);
        FVASSERT_DEBUG(normalOutput->width() == width);
        FVASSERT_DEBUG(normalOutput->height() == height);

        auto cbuffer = queue->makeCommandBuffer();
#if 0
        // clear
        if (auto encoder = cbuffer->makeComputeCommandEncoder(); encoder) {
            encoder->setComputePipelineState(clearBuffers.state);
            encoder->setResource(0, clearBuffers.bindingSet);
            encoder->dispatch(width / clearBuffers.threadgroupSize.x,
                              height / clearBuffers.threadgroupSize.y,
                              1);
            encoder->endEncoding();
        }
#endif
        if (voxelLayers.empty() == false) {

#pragma pack(push, 1)
            struct PushConstantData {
                Matrix4 inversedMVP;
                Matrix4 mvp;
                Matrix4 mv;
                float znear;
                float zfar;
                uint16_t width;
                uint16_t height;
            };
#pragma pack(pop)

            auto& view = viewFrustum.view;
            auto& projection = viewFrustum.projection;

            auto invProj = projection.matrix.inverted();
            auto zfar = Vector3(0, 0, 1).applying(invProj, 1.0).magnitude();
            auto znear = Vector3(0, 0, 0).applying(invProj, 1.0).magnitude();

            auto modelTransform = AffineTransform3();
            modelTransform.scale({ scale, scale, scale });
            modelTransform.concatenate(AffineTransform3{ transform.orientation.matrix3(), transform.position });

            auto modelView = ViewTransform{
                modelTransform.matrix3,
                modelTransform.translation }.concatenate(view);
            ViewFrustum mvpFrustum = { modelView, projection };

            auto nodeTM = modelTransform.matrix4();
            auto mvp = mvpFrustum.matrix();
            auto mv = modelView.matrix4();

            PushConstantData pcdata = {
                mvp.inverted(),
                mvp,
                mv,
                znear, zfar,
                width, height
            };

            ComputePipeline* pipeline = &raycastVoxel;
            if (config.mode == VisualMode::Raycast || config.mode == VisualMode::LOD)
                pipeline = &raycastVisualizer;

            auto encoder = cbuffer->makeComputeCommandEncoder();
            encoder->setComputePipelineState(pipeline->state);
            encoder->pushConstant(uint32_t(ShaderStage::Compute), 0, sizeof(pcdata), &pcdata);

            auto sortLayers = [](
                const std::vector<VoxelLayer>& layers,
                const Matrix4& mat,
                bool ascending = true) -> std::vector<VoxelLayer> {
                if (layers.size() > 1) {
                    struct ZOrderVoxelLayer {
                        const VoxelLayer* layer;
                        float z;
                    };
                    std::vector<ZOrderVoxelLayer> zlayers;
                    zlayers.reserve(layers.size());
                    for (auto& layer : layers) {
                        auto center = Vector4(layer.aabb.center(), 1.0f);
                        auto z = Vector4::dot(center, mat.column3());
                        auto w = Vector4::dot(center, mat.column4());
                        ZOrderVoxelLayer zl = {
                            &layer,
                            z / w
                        };
                        zlayers.push_back(zl);
                    }
                    if (ascending) {
                        std::sort(zlayers.begin(), zlayers.end(),
                                  [](auto& a, auto& b) {
                            return a.z < b.z;
                        });
                    } else {
                        std::sort(zlayers.begin(), zlayers.end(),
                                  [](auto& a, auto& b) {
                            return a.z > b.z;
                        });
                    }
                    std::vector<VoxelLayer> layersCopy;
                    layersCopy.reserve(zlayers.size());
                    for (auto& zlayer : zlayers)
                        layersCopy.push_back(*zlayer.layer);
                    return layersCopy;
                }
                return layers;
            };

            std::vector<VoxelLayer> layersCopy = sortLayers(voxelLayers, mvp);

            int drawLayers = 0;
            for (auto& layer : layersCopy) {
                if (mvpFrustum.isAABBInside(layer.aabb) == false)
                    continue;

                pipeline->bindingSet->setBuffer(3, layer.buffer, 0, layer.buffer->length());
                encoder->setResource(0, pipeline->bindingSet);
                if (true) {
                    // calling setResource (vkCmdBindDescriptorSets) seems to
                    // corrupt existing bound Push-Constant data, which appears
                    // to be a bug in the driver.
                    // For the moment, rebind the Push Constant data.
                    encoder->pushConstant(uint32_t(ShaderStage::Compute), 0, sizeof(pcdata), &pcdata);
                }
                encoder->dispatch(width / pipeline->threadgroupSize.x,
                                  height / pipeline->threadgroupSize.y,
                                  1);
                drawLayers++;
            }
            encoder->endEncoding();

            if (drawLayers > 0) {
                struct {
                    int drawMode;
                } compositionPC;
                compositionPC.drawMode = (int)config.mode;

                // blit outputImage to back-buffer.
                if (config.mode == VisualMode::SSAO || config.mode == VisualMode::Composition) {
                    RenderPassDescriptor desc = {
                        .colorAttachments = {
                            RenderPassColorAttachmentDescriptor {
                                {
                                .renderTarget = this->ssaoOutput,
                                .loadAction = RenderPassLoadAction::LoadActionClear,
                                .storeAction = RenderPassStoreAction::StoreActionStore
                                },
                                Color(0,0,0,1),
                            },
                        },
                        .depthStencilAttachment = {}
                    };
                    struct {
                        Matrix4 projection;
                        float ssaoRadius;
                        float ssaoBias;
                    } ssaoPCData = {
                        projection.matrix,
                        config.ssaoRadius,
                        config.ssaoBias
                    };

                    auto encoder = cbuffer->makeRenderCommandEncoder(desc);
                    encoder->setRenderPipelineState(ssao.state);
                    encoder->setResource(0, ssao.bindingSet);
                    encoder->pushConstant(uint32_t(ShaderStage::Fragment), 0, sizeof(ssaoPCData), &ssaoPCData);
                    encoder->draw(0, 3, 1, 0);
                    encoder->endEncoding();
                    if (config.ssaoBlur) {
                        if (config.ssaoBlur2p) {
                            struct {
                                Vector2 dir;
                            } blur2Param;
                            auto radius = config.ssaoBlur2pRadius;
                            // pass-1
                            blur2Param.dir = { radius, 0 };
                            blur2.bindingSet->setTexture(0, this->ssaoOutput);
                            desc.colorAttachments.front().renderTarget = this->blurOutput;
                            auto encoder = cbuffer->makeRenderCommandEncoder(desc);
                            encoder->setRenderPipelineState(blur2.state);
                            encoder->setResource(0, blur2.bindingSet);
                            encoder->pushConstant(uint32_t(ShaderStage::Fragment), 0, sizeof(blur2Param), &blur2Param);
                            encoder->draw(0, 3, 1, 0);
                            encoder->endEncoding();

                            // pass-2
                            blur2Param.dir = { 0, radius };
                            blur2.bindingSet->setTexture(0, this->blurOutput);
                            desc.colorAttachments.front().renderTarget = this->ssaoOutput;
                            encoder = cbuffer->makeRenderCommandEncoder(desc);
                            encoder->setRenderPipelineState(blur2.state);
                            encoder->setResource(0, blur2.bindingSet);
                            encoder->pushConstant(uint32_t(ShaderStage::Fragment), 0, sizeof(blur2Param), &blur2Param);
                            encoder->draw(0, 3, 1, 0);
                            encoder->endEncoding();

                        } else {
                            desc.colorAttachments.front().renderTarget = this->blurOutput;
                            auto encoder = cbuffer->makeRenderCommandEncoder(desc);
                            encoder->setRenderPipelineState(blur.state);
                            encoder->setResource(0, blur.bindingSet);
                            encoder->draw(0, 3, 1, 0);
                            encoder->endEncoding();
                        }
                    }
                }

                if (config.linearFilter) {
                    composition.bindingSet->setSamplerState(2, blitSamplerLinear);
                }
                else {
                    composition.bindingSet->setSamplerState(2, blitSamplerNearest);
                }

                if (config.ssaoBlur && !config.ssaoBlur2p) {
                    composition.bindingSet->setTexture(3, blurOutput);
                } else {
                    composition.bindingSet->setTexture(3, ssaoOutput);
                }

                auto encoder = cbuffer->makeRenderCommandEncoder(rp);
                encoder->setRenderPipelineState(composition.state);
                encoder->setResource(0, composition.bindingSet);
                encoder->pushConstant(uint32_t(ShaderStage::Fragment), 0, sizeof(compositionPC), &compositionPC);
                encoder->draw(0, 3, 1, 0);
                encoder->endEncoding();
            }
        }
        cbuffer->commit();
    }
}
