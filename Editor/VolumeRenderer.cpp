#include "VolumeRenderer.h"

VolumeRenderer::VolumeRenderer() {
}

VolumeRenderer::~VolumeRenderer() {
}

void VolumeRenderer::initialize(std::shared_ptr<CommandQueue>) {
}

void VolumeRenderer::finalize() {
    aabbOctree = nullptr;
}

void VolumeRenderer::update(float delta) {
}

void VolumeRenderer::render(const RenderPassDescriptor&, const Rect& frame, CommandQueue*) {
}
