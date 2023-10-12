#pragma once
#include "Renderer.h"

class VolumeRenderer : public Renderer {
public:
    VolumeRenderer();
    ~VolumeRenderer();

    void initialize(std::shared_ptr<CommandQueue>) override;
    void finalize() override;

    void update(float delta) override;
    void render(Rect, CommandQueue*) override;
};
