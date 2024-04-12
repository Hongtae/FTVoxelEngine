#include "Renderer.h"
#include "ShaderReflection.h"


std::shared_ptr<ShaderFunction> loadShader(GraphicsDevice* device,
                                           const ShaderPath& sp,
                                           decltype(ComputePipeline::threadgroupSize)* group = nullptr) {
    if (Shader shader(sp.path); shader.validate()) {
        Log::info("Shader description: \"{}\"", sp.path.generic_u8string());
        printShaderReflection(shader);
        if (auto module = device->makeShaderModule(shader)) {
            auto names = module->functionNames();
            std::shared_ptr<ShaderFunction> fn = {};
            if (sp.specializedConstants.empty())
                fn = module->makeFunction(names.front());
            else
                fn = module->makeSpecializedFunction(names.front(),
                                                     sp.specializedConstants);
            if (fn && group) {
                auto groupSize = shader.threadgroupSize();
                group->x = groupSize.x;
                group->y = groupSize.y;
                group->z = groupSize.z;
            }
            return fn;
        }
    }
    return nullptr;
};

std::optional<RenderPipeline> makeRenderPipeline(
    GraphicsDevice* device,
    ShaderPath vs,
    ShaderPath fs,
    const VertexDescriptor& vertexDescriptor,
    std::vector<RenderPipelineColorAttachmentDescriptor> colorAttachments,
    PixelFormat depthStencilAttachmentPixelFormat,
    std::vector<ShaderBinding> bindings) {

    auto pipelineDescriptor = RenderPipelineDescriptor{
        .vertexFunction = loadShader(device, vs),
        .fragmentFunction = loadShader(device, fs),
        .vertexDescriptor = vertexDescriptor,
        .colorAttachments = colorAttachments,
        .depthStencilAttachmentPixelFormat = depthStencilAttachmentPixelFormat,
        .primitiveTopology = PrimitiveType::Triangle,
    };
    if (pipelineDescriptor.vertexFunction == nullptr)
        return {};
    if (pipelineDescriptor.fragmentFunction == nullptr)
        return {};

    PipelineReflection reflection = {};
    auto state = device->makeRenderPipelineState(pipelineDescriptor, &reflection);
    if (state == nullptr) {
        Log::error("makeRenderPipelineState failed.");
        return {};
    }

    auto bindingSet = device->makeShaderBindingSet({ bindings });
    if (bindingSet == nullptr) {
        Log::error("makeShaderBindingSet failed.");
        return {};
    }
    return RenderPipeline{ state, bindingSet };
}

std::optional<ComputePipeline> makeComputePipeline(
    GraphicsDevice* device,
    ShaderPath shader,
    std::vector<ShaderBinding> bindings) {

    ComputePipeline pipeline = {};

    std::shared_ptr<ShaderFunction> fn = loadShader(device, shader, &pipeline.threadgroupSize);
    if (fn == nullptr)
        return {};

    ComputePipelineDescriptor desc = { fn };
    desc.disableOptimization = true;
    PipelineReflection reflection = {};
    pipeline.state = device->makeComputePipelineState(desc, &reflection);
    if (pipeline.state == nullptr)
        return {};

    printPipelineReflection(reflection, Log::Level::Debug);

    ShaderBindingSetLayout layout = { bindings };
    pipeline.bindingSet = device->makeShaderBindingSet(layout);
    if (pipeline.bindingSet == nullptr)
        return {};

    return pipeline;
}
