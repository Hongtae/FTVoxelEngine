#pragma once
#include "Model.h"
#include "Renderer.h"

class MeshRenderer : public Renderer {
public:
    MeshRenderer();
    ~MeshRenderer();

    void initialize(std::shared_ptr<GraphicsDeviceContext>, std::shared_ptr<SwapChain>) override;
    void finalize() override;

    void update(float delta) override;
    void render(const RenderPassDescriptor&, const Rect&) override;

    void prepareScene(const RenderPassDescriptor&, const ViewTransform& v, const ProjectionTransform& p) override {
        this->view = v;
        this->projection = p;
    }

    Model* loadModel(std::filesystem::path path,
                     PixelFormat colorFormat,
                     PixelFormat depthFormat);

    ViewTransform view;
    ProjectionTransform projection;
    Transform transform;
    Vector3 lightDir;

    std::shared_ptr<DepthStencilState> depthStencilState;

    AABB aabb;
    MaterialShaderMap shader;
    MaterialShaderMap shaderNoTex;
    std::shared_ptr<Model> model;
    std::shared_ptr<CommandQueue> queue;
};
