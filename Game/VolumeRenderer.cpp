#include "ShaderReflection.h"
#include "VolumeRenderer.h"

extern std::filesystem::path appResourcesRoot;

VolumeRenderer::VolumeRenderer()
    : view{}
    , projection{}
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

    if (voxelModel) {
        AABB aabb = voxelModel->aabb();
        FVASSERT_DEBUG(aabb.isNull() == false);

#pragma pack(push, 1)
        struct VolumeArrayHeader {
            Vector3 aabbMin;
            uint32_t _padding_offset12;
            Vector3 aabbMax;
            uint32_t _padding_offset28;
        };
#pragma pack(pop)

        voxelLayers.clear();
        auto depth = voxelModel->depth();
        uint32_t startDepth = 0;
        if (depth > maxDepthLevel) {
            startDepth = depth - maxDepthLevel;
            startDepth = std::min(maxStartLevel, startDepth);
        }
        uint32_t numLayers = 1U << startDepth;
        this->voxelLayers.reserve(numLayers);

        auto device = queue->device().get();
        int index = 0;
        auto numNodes = voxelModel->enumerateLevel(
            startDepth, [&](const AABB& aabb, uint32_t depth, const VoxelOctree& octree) {
                auto numNodes = octree.numDescendants();
                auto numLeafNodes = octree.numLeafNodes();
                auto maxLevels = octree.maxDepthLevels();
                Log::debug(std::format(
                    enUS_UTF8,
                    "node at depth:{} (max-depth:{}/{}), num-nodes:{:Ld}, num-leaf-nodes:{:Ld}",
                    depth, maxDepthLevel, maxLevels, numNodes, numLeafNodes));

                auto volumeData = octree.makeArray(aabb, maxDepthLevel);
                if (volumeData.data.empty() == false) {
                    size_t numNodes = volumeData.data.size();
                    const auto dataLength = sizeof(VolumeArray::Node) * numNodes;

                    VolumeArrayHeader header = {
                        aabb.min, 0,
                        aabb.max, 0,
                    };
                    size_t bufferLength = sizeof(header) + dataLength;
                    auto stgBuffer = device->makeBuffer(bufferLength,
                                                        GPUBuffer::StorageModeShared,
                                                        CPUCacheModeWriteCombined);
                    uint8_t* p = (uint8_t*)stgBuffer->contents();
                    memcpy(p, &header, sizeof(header));
                    p += sizeof(header);
                    memcpy(p, volumeData.data.data(), dataLength);
                    stgBuffer->flush();

                    auto buffer = device->makeBuffer(bufferLength,
                                                     GPUBuffer::StorageModePrivate,
                                                     CPUCacheModeDefault);
                    auto cbuffer = queue->makeCommandBuffer();
                    auto encoder = cbuffer->makeCopyCommandEncoder();
                    encoder->copy(stgBuffer, 0, buffer, 0, bufferLength);
                    encoder->endEncoding();
                    cbuffer->commit();

                    VoxelLayer layer = {
                        aabb,
                        buffer
                    };
                    this->voxelLayers.push_back(layer);
                    Log::debug(std::format(
                        enUS_UTF8,
                        "[{}] GPUBuffer {:Ld} bytes ({:Ld} nodes) has been created.",
                        index,
                        bufferLength,
                        numNodes));
                    index++;
                }
            });
        Log::debug(std::format("VoxelModel-Enumerate depth:{}, num-nodes:{}",
                               startDepth, numNodes));
    }
}

void VolumeRenderer::prepareScene(const RenderPassDescriptor& rp, const ViewTransform& v, const ProjectionTransform& p) {
    this->view = v;
    this->projection = p;

    auto renderTarget = rp.colorAttachments.front().renderTarget;

    uint32_t width = renderTarget->width();
    uint32_t height = renderTarget->height();

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
        this->projection = proj;
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

        auto imageBlitSampler = device->makeSamplerState({});
        FVASSERT_DEBUG(imageBlitSampler);
        imageBlit.value().bindingSet->setSamplerState(0, imageBlitSampler);
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
}

void VolumeRenderer::render(const RenderPassDescriptor& rp, const Rect& frame) {
    if (outputImage && depthImage) {

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
            auto mvp = nodeTM
                .concatenating(view.matrix4())
                .concatenating(projection.matrix);
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
            auto viewFrustum = ViewFrustum(view, projection);

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
