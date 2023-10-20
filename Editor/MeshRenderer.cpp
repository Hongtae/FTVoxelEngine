#include "MeshRenderer.h"
#include "ShaderReflection.h"

MeshRenderer::MeshRenderer()
    : view{}
    , projection{}
    , transform{}
    , aabb{}
    , shader{}
    , lightDir{ 1, 0, 0 } {
}

MeshRenderer::~MeshRenderer() {
}

void MeshRenderer::initialize(std::shared_ptr<GraphicsDeviceContext>, std::shared_ptr<SwapChain> swapchain) {
    extern std::filesystem::path appResourcesRoot;
    // load shader
    auto vsPath = appResourcesRoot / "Shaders/sample.vert.spv";
    auto fsPath = appResourcesRoot / "Shaders/sample.frag.spv";

    this->queue = swapchain->queue();
    auto device = queue->device();
    auto vertexShader = loadShader(vsPath, device.get());
    if (vertexShader.has_value() == false)
        throw std::runtime_error("failed to load shader");
    auto fragmentShader = loadShader(fsPath, device.get());
    if (fragmentShader.has_value() == false)
        throw std::runtime_error("failed to load shader");

    struct PushConstantBuffer {
        Matrix4 transform;
        Vector3 lightDir;
        Vector3 color;
    };

    // setup shader binding
    this->shader.resourceSemantics = {
        { ShaderBindingLocation{ 0, 1, 0}, MaterialSemantic::BaseColorTexture },
        { ShaderBindingLocation::pushConstant(0), ShaderUniformSemantic::ModelViewProjectionMatrix },
        //{ ShaderBindingLocation::pushConstant(64), MaterialSemantic::UserDefined },
        //{ ShaderBindingLocation::pushConstant(80), MaterialSemantic::UserDefined },
        //{ ShaderBindingLocation::pushConstant(96), MaterialSemantic::UserDefined },
    };

    this->shader.inputAttributeSemantics = {
        { 0, VertexAttributeSemantic::Position },
        { 1, VertexAttributeSemantic::Normal },
        { 2, VertexAttributeSemantic::TextureCoordinates },
    };
    this->shader.functions = { vertexShader.value(), fragmentShader.value() };

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

    SceneState sceneState = { view, projection, transform.matrix4()};

    auto buffer = queue->makeCommandBuffer();
    auto encoder = buffer->makeRenderCommandEncoder(rp);
    encoder->setDepthStencilState(depthStencilState);

    if (model && model->scenes.empty() == false) {
        //transform.rotate(Quaternion(Vector3(0, 1, 0), std::numbers::pi * delta * 0.4));

        Vector3 lightColor = { 1, 1, 1 };
        Vector3 ambientColor = { 0.3, 0.3, 0.3 };

        auto& scene = model->scenes.at(model->defaultSceneIndex);
        for (auto& node : scene.nodes)
            ForEachNode{ node }(
                [&](auto& node) {
                    if (node.mesh.has_value()) {
                        auto& mesh = node.mesh.value();
                        if (auto material = mesh.material.get(); material) {
                            material->setProperty(ShaderBindingLocation::pushConstant(64), lightDir);
                            material->setProperty(ShaderBindingLocation::pushConstant(80), lightColor);
                            material->setProperty(ShaderBindingLocation::pushConstant(96), ambientColor);
                        }
                        mesh.updateShadingProperties(&sceneState);
                        mesh.encodeRenderCommand(encoder.get(), 1, 0);
                    }
                });
    }
    encoder->endEncoding();
    buffer->commit();
}

Model* MeshRenderer::loadModel(std::filesystem::path path,
                               PixelFormat colorFormat,
                               PixelFormat depthFormat) {
    auto model = ::loadModel(path, queue.get());
    if (model) {
        auto device = queue->device().get();

        // set user-defined property values (lighting)
        Vector3 lightColor = { 1, 1, 1 };
        Vector3 ambientColor = { 0.3, 0.3, 0.3 };

        for (auto& scene : model->scenes) {
            for (auto& node : scene.nodes) {
                ForEachNode{ node }(
                    [&](auto& node) {
                        if (node.mesh.has_value()) {
                            auto& mesh = node.mesh.value();
                            if (auto material = mesh.material.get(); material) {
                                material->shader = this->shader;
                                material->attachments.front().format = colorFormat;
                                material->depthFormat = depthFormat;
                                material->setProperty(ShaderBindingLocation::pushConstant(64), lightDir);
                                material->setProperty(ShaderBindingLocation::pushConstant(80), lightColor);
                                material->setProperty(ShaderBindingLocation::pushConstant(96), ambientColor);
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
                node.updateAABB();
            }
        }

        AABB aabb = {};
        if (model && model->scenes.empty() == false) {
            auto& scene = model->scenes.at(model->defaultSceneIndex);
            for (auto& node : scene.nodes) {
                aabb.combine(node.aabb);
            }
        }
        this->aabb = aabb;
        this->model = model;
        return this->model.get();
    }
    return nullptr;
}
