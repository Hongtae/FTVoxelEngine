#include "Model.h"
#include "ShaderReflection.h"
#include "VolumeRenderer.h"

VolumeRenderer::VolumeRenderer()
    : view{}
    , projection{}
    , transform{} {
}

VolumeRenderer::~VolumeRenderer() {
}

void VolumeRenderer::initialize(std::shared_ptr<GraphicsDeviceContext> gc, std::shared_ptr<SwapChain>) {
    extern std::filesystem::path appResourcesRoot;
    // load shader
    auto path = appResourcesRoot / "Shaders/bvh_aabb_raycast.comp.spv";

    std::shared_ptr<ShaderFunction> fn;
    auto device = gc->device;
    if (Shader shader(path); shader.validate()) {
        Log::info(std::format("Shader description: \"{}\"", path.generic_u8string()));
        printShaderReflection(shader);
        if (auto module = device->makeShaderModule(shader); module) {
            auto names = module->functionNames();
            fn = module->makeFunction(names.front());
            auto groupSize = shader.threadgroupSize();
            this->threadgroupSize = {
                groupSize.x, groupSize.y, groupSize.y
            };
        }
    }
    if (fn == nullptr)
        throw std::runtime_error("failed to load shader");

    queue = gc->commandQueue(CommandQueue::Compute | CommandQueue::Render);

    ComputePipelineDescriptor desc = { fn };
    desc.disableOptimization = true;
    PipelineReflection reflection = {};
    pipelineState = device->makeComputePipeline(desc, &reflection);
    if (pipelineState) {
        printPipelineReflection(reflection, Log::Level::Debug);
    }
    FVASSERT_DEBUG(pipelineState);

    // create render-target
    uint32_t width = 400;
    uint32_t height = 400;
    std::vector<uint32_t> initialColors(width * height, 0);
    texture = Image(width, height, ImagePixelFormat::RGBA8, initialColors.data())
        .makeTexture(queue.get(), TextureUsageSampled | TextureUsageStorage | TextureUsageShaderRead | TextureUsageShaderWrite);

    FVASSERT_DEBUG(texture);

    ShaderBindingSetLayout layout = {
        {
            { 0, ShaderDescriptorType::StorageBuffer, 1, nullptr },
            { 1, ShaderDescriptorType::StorageTexture, 1, nullptr },
        }
    };
    bindingSet = device->makeShaderBindingSet(layout);
    FVASSERT_DEBUG(bindingSet);
}

void VolumeRenderer::finalize() {
    aabbOctree = nullptr;
    aabbOctreeLayerBuffer = nullptr;

    texture = nullptr;
    pipelineState = nullptr;
    queue = nullptr;
}

static std::shared_ptr<AABBOctreeLayer> testLayer;

void VolumeRenderer::setOctreeLayer(std::shared_ptr<AABBOctreeLayer> layer) {
    aabbOctreeLayerBuffer = nullptr;
    testLayer = layer;
    if (layer) {
        FVASSERT_DEBUG(layer->aabb.isNull() == false);

#pragma pack(push, 1)
        struct AABBArrayHeader {
            Vector3 aabbMin;
            uint32_t _padding_offset12;
            Vector3 aabbMax;
            uint32_t aabbArrayCount;
        };
#pragma pack(pop)

        auto arrayCount = layer->data.size();

        AABBArrayHeader header = {
            layer->aabb.min, 
            0,
            layer->aabb.max,
            arrayCount
        };
        const size_t nodeSize = sizeof(AABBOctreeLayer::Node);
        size_t bufferSize = sizeof(AABBArrayHeader) + nodeSize * arrayCount;

        FVASSERT_DEBUG(sizeof(header) == 32);

        auto device = queue->device();
        auto stgBuffer = device->makeBuffer(bufferSize, GPUBuffer::StorageModeShared, CPUCacheModeWriteCombined);
        uint8_t* p = reinterpret_cast<uint8_t*>(stgBuffer->contents());
        memcpy(p, &header, sizeof(header));
        p += sizeof(header);
        memcpy(p, layer->data.data(), nodeSize * arrayCount);
        stgBuffer->flush();

        aabbOctreeLayerBuffer = device->makeBuffer(bufferSize, GPUBuffer::StorageModePrivate, CPUCacheModeDefault);
        auto cbuffer = queue->makeCommandBuffer();
        auto encoder = cbuffer->makeCopyCommandEncoder();
        encoder->copy(stgBuffer, 0, aabbOctreeLayerBuffer, 0, bufferSize);
        encoder->endEncoding();
        cbuffer->commit();

        this->bindingSet->setBuffer(0, aabbOctreeLayerBuffer, 0, bufferSize);
        this->bindingSet->setTexture(1, texture);
    }
}

void VolumeRenderer::render(const RenderPassDescriptor&, const Rect& frame) {
    if (aabbOctreeLayerBuffer) {

#pragma pack(push, 1)
        struct PushConstantData {
            Matrix4 transform;
            uint32_t width;
            uint32_t height;
            float depth;
        };
#pragma pack(pop)

        uint32_t width = texture->width();
        uint32_t height = texture->height();

        auto proj = projection;
        if (proj.matrix._34 != 0.0f) {
            auto f = projection.matrix._22;
            auto aspect = float(width) / float(height);
            proj.matrix._11 = f / aspect;
        }
        PushConstantData pcdata = {
            view.matrix4().concatenating(proj.matrix).inverted(),
            width, height
        };
        pcdata.depth = [&] {
            auto start4 = Vector4(0, 0, 0, 1).applying(pcdata.transform);
            auto end4 = Vector4(0, 0, 1, 1).applying(pcdata.transform);
            auto start = Vector3(start4.x, start4.y, start4.z) / start4.w;
            auto end = Vector3(end4.x, end4.y, end4.z) / end4.w;
            return (end - start).magnitude();
        }();

        auto cbuffer = queue->makeCommandBuffer();
        auto encoder = cbuffer->makeComputeCommandEncoder();
        encoder->setComputePipelineState(pipelineState);
        encoder->setResources(0, bindingSet);
        encoder->pushConstant(uint32_t(ShaderStage::Compute), 0, sizeof(pcdata), &pcdata);
        encoder->dispatch(width / threadgroupSize.x,
                          height / threadgroupSize.y,
                          1);
        encoder->endEncoding();
        cbuffer->commit();
    }
}
