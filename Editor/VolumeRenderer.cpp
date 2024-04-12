#include "Model.h"
#include "ShaderReflection.h"
#include "VolumeRenderer.h"

VolumeRenderer::VolumeRenderer()
    : view{}
    , projection{}
    , transform{}
    , lightDir{ 0, 1, 0 } {
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
        Log::info("Shader description: \"{}\"", path.generic_u8string());
        printShaderReflection(shader);
        if (auto module = device->makeShaderModule(shader)) {
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
    pipelineState = device->makeComputePipelineState(desc, &reflection);
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
    aabbOctreeLayer = nullptr;
    aabbOctreeLayerBuffer = nullptr;

    texture = nullptr;
    pipelineState = nullptr;
    queue = nullptr;
}

void VolumeRenderer::setOctreeLayer(std::shared_ptr<AABBOctreeLayer> layer) {
    aabbOctreeLayerBuffer = nullptr;
    aabbOctreeLayer = layer;
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

void VolumeRenderer::prepareScene(const RenderPassDescriptor&, const ViewTransform& v, const ProjectionTransform& p) {
    this->view = v;
    this->projection = p;

    if (texture) {
        uint32_t width = texture->width();
        uint32_t height = texture->height();

        auto proj = p;
        if (proj.matrix._34 != 0.0f) {
            auto f = p.matrix._22;
            auto aspect = float(width) / float(height);
            proj.matrix._11 = f / aspect;
        }
        this->projection = proj;
    }
}

float VolumeRenderer::bestFitDepth() const {
    if (aabbOctreeLayer && texture) {
        auto aabb = aabbOctreeLayer->aabb;
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
        uint32_t width = texture->width();
        uint32_t height = texture->height();

        auto nodeTM = transform.matrix4();
        auto mvp = nodeTM.concatenating(view.matrix4()).concatenating(projection.matrix);

        for (auto& v : aabbCornerVertices) { v.apply(mvp, 1.0);}

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
            return std::log2(effectivePixels);
        }
    }
    return 0.0f;
}

void VolumeRenderer::render(const RenderPassDescriptor&, const Rect& frame) {
    if (aabbOctreeLayerBuffer && texture) {

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
        Color ambientColor = { 0.7, 0.7, 0.7, 1};

        uint32_t width = texture->width();
        uint32_t height = texture->height();

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

        auto cbuffer = queue->makeCommandBuffer();
        auto encoder = cbuffer->makeComputeCommandEncoder();
        encoder->setComputePipelineState(pipelineState);
        encoder->setResource(0, bindingSet);
        encoder->pushConstant(uint32_t(ShaderStage::Compute), 0, sizeof(pcdata), &pcdata);
        encoder->dispatch(width / threadgroupSize.x,
                          height / threadgroupSize.y,
                          1);
        encoder->endEncoding();
        cbuffer->commit();
    }
}
