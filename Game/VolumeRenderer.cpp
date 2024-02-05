#include "ShaderReflection.h"
#include "VolumeRenderer.h"

extern std::filesystem::path appResourcesRoot;

VolumeRenderer::VolumeRenderer()
    : viewFrustum{ {}, {} }
    , transform{}
    , lightDir{ 0, 1, 0 } {
}

VolumeRenderer::~VolumeRenderer() {
}

void VolumeRenderer::initialize(std::shared_ptr<GraphicsDeviceContext> gc, std::shared_ptr<SwapChain> swapchain) {
    this->queue = swapchain->queue();
    FVASSERT_DEBUG(this->queue->flags() & CommandQueue::Compute);

    // load shader
    if (auto pso = makeComputePipeline(
        gc->device.get(),
        appResourcesRoot / "Shaders/voxel_depth_clear.comp.spv",
        {
            { 0, ShaderDescriptorType::StorageTexture, 1, nullptr }, // color (rgba8)
            { 1, ShaderDescriptorType::StorageTexture, 1, nullptr }, // depth (r32f)
        }); pso.has_value()) {
        clearBuffers = pso.value();
    } else {
        throw std::runtime_error("failed to load shader");
    }

    if (auto pso = makeComputePipeline(
        gc->device.get(),
        appResourcesRoot / "Shaders/voxel_depth_layer.comp.spv",
        {
            { 0, ShaderDescriptorType::StorageTexture, 1, nullptr }, // color (rgba8)
            { 1, ShaderDescriptorType::StorageTexture, 1, nullptr }, // depth (r32f)
            { 2, ShaderDescriptorType::StorageBuffer, 1, nullptr },  // voxel data
        }); pso.has_value()) {
        raycastVoxel = pso.value();
    } else {
        throw std::runtime_error("failed to load shader");
    }
}

void VolumeRenderer::finalize() {
    outputImage = nullptr;
    depthImage = nullptr;

    voxelModel = nullptr;
    voxelLayers.clear();

    raycastVoxel = {};
    clearBuffers = {};
    imageBlit.reset();
    imageBlitVB = nullptr;

    queue = nullptr;
}

void VolumeRenderer::setModel(std::shared_ptr<VoxelModel> model) {
    voxelModel = model;
    voxelLayers.clear();
}

bool stopUpdating = false;

void VolumeRenderer::prepareScene(const RenderPassDescriptor& rp, const ViewTransform& v, const ProjectionTransform& p) {
    ViewTransform view = v;
    ProjectionTransform projection = p;

    auto renderTarget = rp.colorAttachments.front().renderTarget;

    uint32_t width = renderTarget->width();
    uint32_t height = renderTarget->height();

    width = width * config.renderScale;
    height = height * config.renderScale;

    bool resetImages = true;
    if (outputImage) {
        if (outputImage->width() == width && outputImage->height() == height)
            resetImages = false;
    }

    auto device = queue->device();

    if (resetImages) {
        uint32_t renderTargetUsage = TextureUsageCopyDestination |
            TextureUsageCopySource |
            TextureUsageSampled |
            TextureUsageStorage |
            TextureUsageShaderRead |
            TextureUsageShaderWrite;

        // create render-target (rgba8)
        outputImage = device->makeTexture(
            TextureDescriptor{
                TextureType2D,
                PixelFormat::RGBA8Unorm,
                width,
                height,
                1, 1, 1, 1,
                renderTargetUsage
            });
        FVASSERT_DEBUG(outputImage);

        // create render-target (r32f)
        depthImage = device->makeTexture(
            TextureDescriptor{
                TextureType2D,
                PixelFormat::R32Float,
                width,
                height,
                1, 1, 1, 1,
                renderTargetUsage
            });
        FVASSERT_DEBUG(depthImage);

        clearBuffers.bindingSet->setTexture(0, outputImage);
        clearBuffers.bindingSet->setTexture(1, depthImage);

        raycastVoxel.bindingSet->setTexture(0, outputImage);
        raycastVoxel.bindingSet->setTexture(1, depthImage);
        Log::debug("VolumeRenderer: resetImages ({}x{})", width, height);
    }

    if (outputImage) {
        uint32_t width = outputImage->width();
        uint32_t height = outputImage->height();

        auto proj = p;
        if (proj.matrix._34 != 0.0f) {
            auto f = p.matrix._22;
            auto aspect = float(width) / float(height);
            proj.matrix._11 = f / aspect;
        }
        projection = proj;
    }

#pragma pack(push, 4)
    struct ImageBlitVertex {
        Vector2 pos;
        Vector2 tex;
        Vector4 color;
    };
#pragma pack(pop)

    if (imageBlit.has_value() == false) {
        auto colorFormat = renderTarget->pixelFormat();
        auto depthFormat = rp.depthStencilAttachment.renderTarget ?
            rp.depthStencilAttachment.renderTarget->pixelFormat() :
            PixelFormat::Invalid;

        imageBlit = makeRenderPipeline(
            device.get(),
            appResourcesRoot / "Shaders/blit.vert.spv",
            appResourcesRoot / "Shaders/blit.frag.spv",
            // VertexDescriptor
            {
                .attributes = {
                    { VertexFormat::Float2, 0, 0, 0 },
                    { VertexFormat::Float2, offsetof(ImageBlitVertex, tex), 0, 1},
                    { VertexFormat::Float4, offsetof(ImageBlitVertex, color), 0, 2 },
                },
                .layouts = {
                    { VertexStepRate::Vertex, sizeof(ImageBlitVertex), 0 }
                },
            },
            // RenderPipelineColorAttachmentDescriptor
            {
                { 0, colorFormat, BlendState::alphaBlend },
            },
            // depthStencilAttachmentPixelFormat
            depthFormat,
            // ShaderBinding
            {
                { 0, ShaderDescriptorType::TextureSampler, 1, nullptr },
            });

        FVASSERT_DEBUG(imageBlit);

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

        if (config.linearFilter)
            imageBlit.value().bindingSet->setSamplerState(0, blitSamplerLinear);
        else
            imageBlit.value().bindingSet->setSamplerState(0, blitSamplerNearest);

        imageBlit.value().bindingSet->setTexture(0, outputImage);
    }
    if (imageBlitVB == nullptr)
    {
        const ImageBlitVertex vertices[6] = {
            { Vector2(-1, -1), Vector2(0, 1), Vector4(1, 1, 1, 1) },
            { Vector2(-1,  1), Vector2(0, 0), Vector4(1, 1, 1, 1) },
            { Vector2(1, -1), Vector2(1, 1), Vector4(1, 1, 1, 1) },

            { Vector2(1, -1), Vector2(1, 1), Vector4(1, 1, 1, 1) },
            { Vector2(-1,  1), Vector2(0, 0), Vector4(1, 1, 1, 1) },
            { Vector2(1,  1), Vector2(1, 0), Vector4(1, 1, 1, 1) },
        };
        auto stgBuffer = device->makeBuffer(sizeof(vertices),
                                            GPUBuffer::StorageModeShared,
                                            CPUCacheModeWriteCombined);
        FVASSERT_DEBUG(stgBuffer);
        if (stgBuffer) {
            auto p = stgBuffer->contents();
            memcpy(p, vertices, sizeof(vertices));
            stgBuffer->flush();
        }
        auto buffer = device->makeBuffer(stgBuffer->length(),
                                         GPUBuffer::StorageModePrivate,
                                         CPUCacheModeDefault);

        auto cbuffer = queue->makeCommandBuffer();
        auto encoder = cbuffer->makeCopyCommandEncoder();
        encoder->copy(stgBuffer, 0, buffer, 0, stgBuffer->length());
        encoder->endEncoding();
        cbuffer->commit();

        imageBlitVB = buffer;
    }

    if (resetImages)
        imageBlit.value().bindingSet->setTexture(0, outputImage);

    this->viewFrustum = { view, projection };

    if (stopUpdating)
        return;

    // calculate volume depth
    voxelLayers.clear();
    if (voxelModel) {

        auto nodeTM = transform.matrix4();
        auto mvp = nodeTM.concatenating(view.matrix4()).concatenating(projection.matrix);

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

        AABB aabb = voxelModel->aabb();
        if (viewFrustum.isAABBInside(aabb)) {
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
                auto distMaxDetailSq = config.distanceToMaxDetail * config.distanceToMaxDetail;
                auto distMinDetailSq = config.distanceToMinDetail * config.distanceToMinDetail;
                distMinDetailSq = std::max(distMinDetailSq, distMaxDetailSq + 0.001f);

                volumeData = root->makeArray(
                    aabb, maxDetailLevel,
                    [&](const AABB& aabb, uint32_t depth, uint32_t& maxDepth) {
                        if (depth == startLevel) {
                            _debug_numIterations++;
                            if (viewFrustum.isAABBInside(aabb)) {
                                auto c = aabb.center().applying(transform);
                                float distanceFromViewSq = (c - viewPosition).magnitudeSquared();
                                float bestFit = calculateDepthLevel(aabb, width, height);
                                if (auto k = distanceFromViewSq - distMaxDetailSq; k > 0.0f) {
                                    k = k / (distMinDetailSq - distMaxDetailSq);
                                    k = 1.0f - std::clamp(k, 0.0f, 1.0f);
                                    bestFit = bestFit * k;
                                }
                                maxDepth = std::min({ maxDepth, uint32_t(bestFit) + depth, maxDetailLevel });
                            } else {
                                maxDepth = 0;
                                _debug_numCulling++;
                            }
                        }
                    });
            } else {
                volumeData = root->makeArray(aabb, std::min(maxDetailLevel, bestFitDepth));
            }

            if (volumeData.data.empty() == false) {
                size_t numNodes = volumeData.data.size();
                const auto dataLength = sizeof(VolumeArray::Node) * numNodes;

                VolumeArray::Header header = { aabb.min, 0, aabb.max, 0 };

                size_t bufferLength = sizeof(header) + dataLength;
                auto buffer = device->makeBuffer(bufferLength,
                                                 GPUBuffer::StorageModeShared,
                                                 CPUCacheModeWriteCombined);
                uint8_t* p = (uint8_t*)buffer->contents();
                memcpy(p, &header, sizeof(header));
                p += sizeof(header);
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
    if (!stopRendering && outputImage && depthImage) {
        uint32_t width = outputImage->width();
        uint32_t height = outputImage->height();
        FVASSERT_DEBUG(depthImage->width() == width);
        FVASSERT_DEBUG(depthImage->height() == height);

        // clear
        auto cbuffer = queue->makeCommandBuffer();
        if (auto encoder = cbuffer->makeComputeCommandEncoder(); encoder) {
            encoder->setComputePipelineState(clearBuffers.state);
            encoder->setResource(0, clearBuffers.bindingSet);
            encoder->dispatch(width / clearBuffers.threadgroupSize.x,
                              height / clearBuffers.threadgroupSize.y,
                              1);
            encoder->endEncoding();
        }

        if (voxelLayers.empty() == false) {

#pragma pack(push, 1)
            struct PushConstantData {
                Matrix4 inversedM;
                Matrix4 inversedMVP;
                Matrix4 mvp;
                Color ambientColor;
                Color lightColor;
                Vector3 lightDir;
                uint32_t width;
                uint32_t height;
            };
#pragma pack(pop)

            Color lightColor = { 1, 1, 1, 0.2 };
            Color ambientColor = { 0.7, 0.7, 0.7, 1 };

            auto nodeTM = transform.matrix4();
            auto mvp = nodeTM.concatenating(viewFrustum.matrix());

            PushConstantData pcdata = {
                nodeTM.inverted(),
                mvp.inverted(),
                mvp,
                ambientColor,
                lightColor,
                lightDir,
                width, height
            };

            auto encoder = cbuffer->makeComputeCommandEncoder();
            encoder->setComputePipelineState(raycastVoxel.state);
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
                if (viewFrustum.isAABBInside(layer.aabb) == false)
                    continue;

                raycastVoxel.bindingSet->setBuffer(2, layer.buffer, 0, layer.buffer->length());
                encoder->setResource(0, raycastVoxel.bindingSet);
                if (true) {
                    // calling setResource (vkCmdBindDescriptorSets) seems to
                    // corrupt existing bound Push-Constant data, which appears
                    // to be a bug in the driver.
                    // For the moment, rebind the Push Constant data.
                    encoder->pushConstant(uint32_t(ShaderStage::Compute), 0, sizeof(pcdata), &pcdata);
                }
                encoder->dispatch(width / raycastVoxel.threadgroupSize.x,
                                  height / raycastVoxel.threadgroupSize.y,
                                  1);
                drawLayers++;
            }
            encoder->endEncoding();

            if (drawLayers > 0) {
                // blit outputImage to back-buffer.

                if (config.linearFilter)
                    imageBlit.value().bindingSet->setSamplerState(0, blitSamplerLinear);
                else
                    imageBlit.value().bindingSet->setSamplerState(0, blitSamplerNearest);

                auto encoder = cbuffer->makeRenderCommandEncoder(rp);
                encoder->setRenderPipelineState(imageBlit.value().state);
                encoder->setResource(0, imageBlit.value().bindingSet);
                encoder->setVertexBuffer(imageBlitVB, 0, 0);
                encoder->setCullMode(CullMode::None);
                encoder->setFrontFacing(Winding::Clockwise);
                encoder->draw(0, 6, 1, 0);
                encoder->endEncoding();
            }
        }
        cbuffer->commit();
    }
}
