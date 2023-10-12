#pragma once
#include "Model.h"
#include "Renderer.h"

class MeshRenderer : public Renderer {
public:
    MeshRenderer();
    ~MeshRenderer();

    void initialize(std::shared_ptr<CommandQueue>) override;
    void finalize() override;

    void update(float delta) override;
    void render(const RenderPassDescriptor&, const Rect&, CommandQueue*) override;

    Model* loadModel(std::filesystem::path path,
                     CommandQueue* queue,
                     PixelFormat colorFormat,
                     PixelFormat depthFormat);

    ViewTransform view;
    ProjectionTransform projection;
    Transform transform;
    std::shared_ptr<DepthStencilState> depthStencilState;

    AABB aabb;
    MaterialShaderMap shader;
    std::shared_ptr<Model> model;
    std::shared_ptr<CommandQueue> queue;
};
