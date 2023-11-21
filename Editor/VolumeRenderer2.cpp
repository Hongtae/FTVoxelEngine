#include "Model.h"
#include "ShaderReflection.h"
#include "VolumeRenderer2.h"

VolumeRenderer2::VolumeRenderer2()
    : view{}
    , projection{}
    , transform{}
    , lightDir{ 0, 1, 0 } {
}

VolumeRenderer2::~VolumeRenderer2() {
}

void VolumeRenderer2::initialize(std::shared_ptr<GraphicsDeviceContext> gc, std::shared_ptr<SwapChain>) {
    extern std::filesystem::path appResourcesRoot;

    this->queue = gc->commandQueue(CommandQueue::Compute | CommandQueue::Render);
    auto device = gc->device.get();

    // load shader
    if (auto pso = loadPipeline(
        appResourcesRoot / "Shaders/voxel_depth_clear.comp.spv",
        {
            { 0, ShaderDescriptorType::StorageTexture, 1, nullptr }, // color (rgba8)
            { 1, ShaderDescriptorType::StorageTexture, 1, nullptr }, // depth (r32f)
        }); pso.has_value()) {
        clearBuffers = pso.value();
    } else {
        throw std::runtime_error("failed to load shader");
    }

    if (auto pso = loadPipeline(
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


    uint32_t width = 400;
    uint32_t height = 400;

    uint32_t renderTargetUsage = TextureUsageCopyDestination |
        TextureUsageCopySource |
        TextureUsageSampled |
        TextureUsageStorage |
        TextureUsageShaderRead |
        TextureUsageShaderWrite;

    // create render-target (rgba8)
    renderTarget = device->makeTexture(
        TextureDescriptor{
            TextureType2D,
            PixelFormat::RGBA8Unorm,
            width,
            height,
            1, 1, 1, 1,
            renderTargetUsage
        });
    FVASSERT_DEBUG(renderTarget);

    // create render-target (r32f)
    renderTargetR32F = device->makeTexture(
        TextureDescriptor{
            TextureType2D,
            PixelFormat::R32Float,
            width,
            height,
            1, 1, 1, 1,
            renderTargetUsage
        });

    clearBuffers.bindingSet->setTexture(0, renderTarget);
    clearBuffers.bindingSet->setTexture(1, renderTargetR32F);

    raycastVoxel.bindingSet->setTexture(0, renderTarget);
    raycastVoxel.bindingSet->setTexture(1, renderTargetR32F);

    FVASSERT_DEBUG(renderTargetR32F);
}

void VolumeRenderer2::finalize() {
    renderTarget = nullptr;
    renderTargetR32F = nullptr;
    voxelModel = nullptr;
    voxelLayers.clear();

    raycastVoxel = {};
    clearBuffers = {};
    queue = nullptr;
}

std::optional<VolumeRenderer2::PipelineState> VolumeRenderer2::loadPipeline(
    std::filesystem::path path,
    std::vector<ShaderBinding> bindings) {

    if (queue == nullptr)
        return {};

    PipelineState pipeline = {};

    std::shared_ptr<ShaderFunction> fn;
    auto device = queue->device();
    if (Shader shader(path); shader.validate()) {
        Log::info(std::format("Shader description: \"{}\"", path.generic_u8string()));
        printShaderReflection(shader);
        if (auto module = device->makeShaderModule(shader); module) {
            auto names = module->functionNames();
            fn = module->makeFunction(names.front());
            auto groupSize = shader.threadgroupSize();
            pipeline.threadgroupSize = {
                groupSize.x, groupSize.y, groupSize.y
            };
        }
    }
    if (fn == nullptr)
        return {};
    
    ComputePipelineDescriptor desc = { fn };
    desc.disableOptimization = true;
    PipelineReflection reflection = {};
    pipeline.pso = device->makeComputePipelineState(desc, &reflection);
    if (pipeline.pso == nullptr)
        return {};

    printPipelineReflection(reflection, Log::Level::Debug);

    ShaderBindingSetLayout layout = { bindings };
    pipeline.bindingSet = device->makeShaderBindingSet(layout);
    if (pipeline.bindingSet == nullptr)
        return {};

    return pipeline;
}

void VolumeRenderer2::setVoxelModel(std::shared_ptr<VoxelModel> model) {
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
            startDepth = std::min(2U, startDepth);
        }
        uint32_t numLayers = 1U << startDepth;
        this->voxelLayers.reserve(numLayers);

        auto numNodes = voxelModel->enumerateLevel(
            startDepth, [&](const AABB& aabb, uint32_t depth, const VoxelOctree& octree) {
                auto numNodes = octree.numDescendants();
                auto numLeafNodes = octree.numLeafNodes();
                auto maxLevels = octree.maxDepthLevels();
                Log::debug(std::format("node at depth:{} (max-depth:{}), num-nodes:{}, num-leaf-nodes:{}",
                                       depth, maxLevels, numNodes, numLeafNodes));
            });
        Log::debug(std::format("VoxelModel-Enumerate depth:{}, num-nodes:{}",
                               startDepth, numNodes));
    }
}

void VolumeRenderer2::prepareScene(const RenderPassDescriptor&, const ViewTransform& v, const ProjectionTransform& p) {
    this->view = v;
    this->projection = p;

    if (renderTarget) {
        uint32_t width = renderTarget->width();
        uint32_t height = renderTarget->height();

        auto proj = p;
        if (proj.matrix._34 != 0.0f) {
            auto f = p.matrix._22;
            auto aspect = float(width) / float(height);
            proj.matrix._11 = f / aspect;
        }
        this->projection = proj;
    }
}

void VolumeRenderer2::render(const RenderPassDescriptor&, const Rect& frame) {
    if (renderTarget && renderTargetR32F) {

        uint32_t width = renderTarget->width();
        uint32_t height = renderTarget->height();
        FVASSERT_DEBUG(renderTargetR32F->width() == width);
        FVASSERT_DEBUG(renderTargetR32F->height() == height);

        // clear
        auto cbuffer = queue->makeCommandBuffer();
        if (auto encoder = cbuffer->makeComputeCommandEncoder(); encoder) {
            encoder->setComputePipelineState(clearBuffers.pso);
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
                Color ambientColor;
                Color lightColor;
                Vector3 lightDir;
                uint32_t width;
                uint32_t height;
                float depth;
            };
#pragma pack(pop)

            Color lightColor = { 1, 1, 1, 0.2 };
            Color ambientColor = { 0.7, 0.7, 0.7, 1 };

            auto nodeTM = transform.matrix4();
            PushConstantData pcdata = {
                nodeTM.inverted(),
                nodeTM
                    .concatenating(view.matrix4())
                    .concatenating(projection.matrix)
                    .inverted(),
                ambientColor,
                lightColor,
                lightDir,
                width, height
            };
            pcdata.depth = [&] {
                auto start = Vector3(0, 0, 0).applying(pcdata.inversedMVP, 1.0f);
                auto end = Vector3(0, 0, 1).applying(pcdata.inversedMVP, 1.0f);
                return (end - start).magnitude();
            }();

            auto encoder = cbuffer->makeComputeCommandEncoder();
            encoder->setComputePipelineState(raycastVoxel.pso);
            encoder->pushConstant(uint32_t(ShaderStage::Compute), 0, sizeof(pcdata), &pcdata);

            for (auto& layer : voxelLayers) {
                raycastVoxel.bindingSet->setBuffer(2, layer.buffer, 0, layer.buffer->length());
                encoder->setResource(0, raycastVoxel.bindingSet);
                encoder->dispatch(width / raycastVoxel.threadgroupSize.x,
                                  height / raycastVoxel.threadgroupSize.y,
                                  1);
            }
            encoder->endEncoding();
        }
        cbuffer->commit();
    }
}

