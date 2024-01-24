#include "MeshRenderer.h"
#include "ShaderReflection.h"

MeshRenderer::MeshRenderer()
    : view{}
    , projection{}
    , transform{}
    , aabb{}
    , shader{}
    , lightDir{ 0, 1, 0 } {
}

MeshRenderer::~MeshRenderer() {
}

void MeshRenderer::initialize(std::shared_ptr<GraphicsDeviceContext>, std::shared_ptr<SwapChain> swapchain) {

    this->queue = swapchain->queue();
    auto device = queue->device();

    auto loadShader = [device](std::filesystem::path path)->std::optional<MaterialShaderMap::Function> {
        if (Shader shader(path); shader.validate()) {
            Log::info("Shader description: \"{}\"", path.generic_u8string());
            printShaderReflection(shader);
            if (auto module = device->makeShaderModule(shader); module) {
                auto names = module->functionNames();
                auto fn = module->makeFunction(names.front());
                return MaterialShaderMap::Function { fn, shader.descriptors() };
            }
        }
        throw std::runtime_error("failed to load shader");
        return {};
    };

    extern std::filesystem::path appResourcesRoot;
    // load shader

    this->shader.functions = {
        loadShader(appResourcesRoot / "Shaders/sample.vert.spv").value(),
        loadShader(appResourcesRoot / "Shaders/sample.frag.spv").value()
    };

    this->shaderNoTex.functions = {
        loadShader(appResourcesRoot / "Shaders/sample_notex.vert.spv").value(),
        loadShader(appResourcesRoot / "Shaders/sample_notex.frag.spv").value()
    };

    // setup shader binding
    this->shader.resourceSemantics = {
        { ShaderBindingLocation{ 0, 0, 0}, MaterialSemantic::BaseColor },
        { ShaderBindingLocation{ 0, 0, 16}, MaterialSemantic::Metallic },
        { ShaderBindingLocation{ 0, 0, 20}, MaterialSemantic::Roughness },
        { ShaderBindingLocation{ 0, 1, 0}, MaterialSemantic::BaseColorTexture },
        { ShaderBindingLocation::pushConstant(0), ShaderUniformSemantic::ModelMatrix },
        { ShaderBindingLocation::pushConstant(64), ShaderUniformSemantic::ViewProjectionMatrix },
    };
    this->shaderNoTex.resourceSemantics = {
        { ShaderBindingLocation{ 0, 0, 0}, MaterialSemantic::BaseColor },
        { ShaderBindingLocation{ 0, 0, 16}, MaterialSemantic::Metallic },
        { ShaderBindingLocation{ 0, 0, 20}, MaterialSemantic::Roughness },
        { ShaderBindingLocation::pushConstant(0), ShaderUniformSemantic::ModelMatrix },
        { ShaderBindingLocation::pushConstant(64), ShaderUniformSemantic::ViewProjectionMatrix },
    };

    this->shader.inputAttributeSemantics = {
        { 0, VertexAttributeSemantic::Position },
        { 1, VertexAttributeSemantic::Normal },
        { 2, VertexAttributeSemantic::TextureCoordinates },
    };
    this->shaderNoTex.inputAttributeSemantics = {
        { 0, VertexAttributeSemantic::Position },
        { 1, VertexAttributeSemantic::Normal },
        { 2, VertexAttributeSemantic::Color },
    };

    this->depthStencilState = device->makeDepthStencilState(
        {
            CompareFunctionLessEqual,
            StencilDescriptor{},
            StencilDescriptor{},
            true
        });
}

void MeshRenderer::finalize() {
    this->shader = {};
    this->model = nullptr;
    this->queue = nullptr;
    this->depthStencilState = nullptr;
}

void MeshRenderer::update(float delta) {
}

void MeshRenderer::render(const RenderPassDescriptor& rp, const Rect& frame) {
    if (model && model->scenes.empty() == false) {
        SceneState sceneState = { view, projection, transform.matrix4() };

        auto buffer = queue->makeCommandBuffer();
        auto encoder = buffer->makeRenderCommandEncoder(rp);
        encoder->setDepthStencilState(depthStencilState);

        //transform.rotate(Quaternion(Vector3(0, 1, 0), std::numbers::pi * delta * 0.4));

        Vector3 lightColor = { 1, 1, 1 };
        Vector3 ambientColor = { 0.7, 0.7, 0.7 };

        auto& scene = model->scenes.at(model->defaultSceneIndex);
        for (auto& node : scene.nodes)
            ForEachNode{ node }(
                [&](auto& node, const Transform& trans) {
                    if (node.mesh.has_value()) {
                        auto& mesh = node.mesh.value();
                        if (auto material = mesh.material.get(); material) {
                            material->setProperty(ShaderBindingLocation::pushConstant(128), lightDir);
                            material->setProperty(ShaderBindingLocation::pushConstant(144), lightColor);
                            material->setProperty(ShaderBindingLocation::pushConstant(160), ambientColor);
                        }
                        auto sceneStateLocal = sceneState;
                        sceneStateLocal.model = AffineTransform3::identity
                            .scaled(node.scale).matrix4()
                            .concatenating(trans.matrix4());

                        mesh.updateShadingProperties(&sceneStateLocal);
                        mesh.encodeRenderCommand(encoder.get(), 1, 0);
                    }
                });
        encoder->endEncoding();
        buffer->commit();
    }
}

Model* MeshRenderer::loadModel(std::filesystem::path path,
                               PixelFormat colorFormat,
                               PixelFormat depthFormat) {
    auto model = ::loadModel(path, queue.get());
    if (model) {
        auto device = queue->device().get();

        for (auto& scene : model->scenes) {
            for (auto& node : scene.nodes) {
                ForEachNode{ node }(
                    [&](auto& node) {
                        if (node.mesh.has_value()) {
                            auto& mesh = node.mesh.value();
                            if (auto material = mesh.material.get(); material) {
                                if (material->properties.contains(MaterialSemantic::BaseColorTexture)) {
                                    material->shader = this->shader;
                                } else {
                                    material->shader = this->shaderNoTex;
                                }
                                material->attachments.front().format = colorFormat;
                                material->depthFormat = depthFormat;
                            }

                            PipelineReflection reflection = {};
                            if (mesh.buildPipelineState(device, &reflection)) {
                                printPipelineReflection(reflection, Log::Level::Debug);
                                mesh.initResources(device, Mesh::BufferUsagePolicy::SingleBuffer);
                            } else {
                                Log::error("Failed to make pipeline descriptor");
                            }
                        }
                    });
            }
        }

        AABB aabb = {};
        if (model && model->scenes.empty() == false) {
            if (model->defaultSceneIndex < 0)
                model->defaultSceneIndex = 0;
            auto& scene = model->scenes.at(model->defaultSceneIndex);
            for (auto& node : scene.nodes) {
                aabb.combine(node.aabb());
            }
        }
        this->aabb = aabb;
        this->model = model;
        return this->model.get();
    }
    return nullptr;
}
