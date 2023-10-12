#pragma once
#include "Renderer.h"

class MeshRenderer : public Renderer {
public:
    MeshRenderer();
    ~MeshRenderer();

    void initialize(std::shared_ptr<CommandQueue>) override;
    void finalize() override;

    void update(float delta) override;
    void render(Rect, CommandQueue*) override;
};
