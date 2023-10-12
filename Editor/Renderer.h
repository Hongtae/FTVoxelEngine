#pragma once
#include <memory>
#include <FVCore.h>

using namespace FV;

class Renderer {
public:
    virtual ~Renderer() {}

    virtual void initialize(std::shared_ptr<CommandQueue>) = 0;
    virtual void finalize() = 0;

    virtual void update(float delta) {}
    virtual void render(const RenderPassDescriptor&, const Rect&, CommandQueue*) = 0;
    virtual void prepareScene(const RenderPassDescriptor&) {}
};
