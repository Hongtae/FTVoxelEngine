#include <limits>
#include <cmath>
#include "Scene.h"
#include "Mesh.h"
#include "Material.h"

using namespace FV;

void SceneNode::draw(RenderCommandEncoder* encoder, const SceneState& state) const {
    if (mesh) {
        if (fabs(scale.x * scale.y * scale.z) > std::numeric_limits<float>::epsilon()) {

        }
    }
}

Matrix4 SceneNode::transformMatrix() const {
    return AffineTransform3::identity
        .scaled(this->scale)
        .matrix4()
        .concatenating(this->transform.matrix4());
}

AABB SceneNode::aabb() const {
    AABB aabb = {};
    if (mesh) {
        aabb = mesh.value().aabb;
    }
    
    for (auto& child : children) {
        auto aabb2 = child.aabb();
        aabb.combine(aabb2);
    }
    aabb.apply(transformMatrix());
    return aabb;
}

Scene::Scene() {
}

Scene::~Scene() {
}
