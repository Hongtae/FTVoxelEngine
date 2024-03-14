#pragma once
#include <memory>
#include <FVCore.h>

using namespace FV;

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void initialize(std::shared_ptr<GraphicsDeviceContext>, std::shared_ptr<SwapChain>) = 0;
    virtual void finalize() = 0;

    virtual void update(float delta) {}
    virtual void render(const RenderPassDescriptor&, const Rect&) = 0;
    virtual void prepareScene(const RenderPassDescriptor&, const ViewTransform&, const ProjectionTransform&) {}
};
