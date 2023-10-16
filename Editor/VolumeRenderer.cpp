#include "Model.h"
#include "VolumeRenderer.h"

VolumeRenderer::VolumeRenderer() {
}

VolumeRenderer::~VolumeRenderer() {
}

void VolumeRenderer::initialize(std::shared_ptr<CommandQueue> queue) {
    extern std::filesystem::path appResourcesRoot;
    // load shader
    auto path = appResourcesRoot / "Shaders/bvh_aabb_raycast.comp.spv";

    auto device = queue->device();
    auto computeShader = loadShader(path, device.get());
    if (computeShader.has_value() == false)
        throw std::runtime_error("failed to load shader");
}

void VolumeRenderer::finalize() {
    aabbOctree = nullptr;
}

void VolumeRenderer::update(float delta) {
}

void VolumeRenderer::render(const RenderPassDescriptor&, const Rect& frame, CommandQueue*) {
}
