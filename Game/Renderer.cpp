#include "Renderer.h"
#include "ShaderReflection.h"


std::optional<RenderPipeline> makeRenderPipeline(
    GraphicsDevice* device,
    std::filesystem::path vsPath,
    std::filesystem::path fsPath,
    std::vector<ShaderSpecialization> vsSp,
    std::vector<ShaderSpecialization> fsSp,
    const VertexDescriptor& vertexDescriptor,
    std::vector<RenderPipelineColorAttachmentDescriptor> colorAttachments,
    PixelFormat depthStencilAttachmentPixelFormat,
    std::vector<ShaderBinding> bindings) {

    auto loadShader = [device](std::filesystem::path path,
                               std::vector<ShaderSpecialization>& sp) -> std::shared_ptr<ShaderFunction> {
        if (Shader shader(path); shader.validate()) {
            Log::info("Shader description: \"{}\"", path.generic_u8string());
            printShaderReflection(shader);
            if (auto module = device->makeShaderModule(shader); module) {
                auto names = module->functionNames();
                if (sp.empty())
                    return module->makeFunction(names.front());
                else
                    return module->makeSpecializedFunction(names.front(), sp);
            }
        }
        return nullptr;
    };
    auto pipelineDescriptor = RenderPipelineDescriptor{
        .vertexFunction = loadShader(vsPath, vsSp),
        .fragmentFunction = loadShader(fsPath, fsSp),
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
    std::filesystem::path path,
    std::vector<ShaderBinding> bindings) {

    ComputePipeline pipeline = {};

    std::shared_ptr<ShaderFunction> fn;
    if (Shader shader(path); shader.validate()) {
        Log::info("Shader description: \"{}\"", path.generic_u8string());
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
