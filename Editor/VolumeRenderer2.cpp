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
    // load shader
    auto path = appResourcesRoot / "Shaders/voxel_depth_layer.comp.spv";

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
    pipelineState = device->makeComputePipelineState(desc, &reflection);
    if (pipelineState) {
        printPipelineReflection(reflection, Log::Level::Debug);
    }
    FVASSERT_DEBUG(pipelineState);

    // create render-target
    uint32_t width = 400;
    uint32_t height = 400;
    std::vector<uint32_t> initialColors(width * height, 0);
    renderTarget = Image(width, height, ImagePixelFormat::RGBA8, initialColors.data())
        .makeTexture(queue.get(), TextureUsageSampled | TextureUsageStorage | TextureUsageShaderRead | TextureUsageShaderWrite);

    FVASSERT_DEBUG(renderTarget);

    ShaderBindingSetLayout layout = {
        {
            { 0, ShaderDescriptorType::StorageBuffer, 1, nullptr },
            { 1, ShaderDescriptorType::StorageTexture, 1, nullptr },
        }
    };
    bindingSet = device->makeShaderBindingSet(layout);
    FVASSERT_DEBUG(bindingSet);
}

void VolumeRenderer2::finalize() {

    voxelModel = nullptr;
    voxelLayers.clear();

    renderTarget = nullptr;
    pipelineState = nullptr;
    queue = nullptr;
}

void VolumeRenderer2::setVoxelModel(std::shared_ptr<VoxelModel> model) {
    voxelModel = model;
    voxelLayers.clear();

    if (voxelModel) {

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
    if (renderTarget) {

    }
}
