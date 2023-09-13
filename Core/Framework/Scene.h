#pragma once
#include "../include.h"
#include <string>
#include <optional>
#include <vector>
#include <limits>
#include "ViewProjection.h"
#include "Quaternion.h"
#include "Vector3.h"
#include "Matrix4.h"
#include "AffineTransform3.h"
#include "Mesh.h"
#include "Transform.h"
#include "RenderCommandEncoder.h"
#include "AABB.h"

namespace FV {
    struct SceneState {
        ViewTransform view;
        ProjectionTransform projection;
        Matrix4 model;
    };

    struct FVCORE_API SceneNode {
        std::string name;
        std::optional<Mesh> mesh;
        AABB aabb;

        Vector3 scale = Vector3(1, 1, 1);
        Transform transform = Transform::identity;

        std::vector<SceneNode> children;

        void draw(RenderCommandEncoder*, const SceneState&) const;
        void updateAABB();
    };

    class FVCORE_API Scene {
    public:
        Scene();
        ~Scene();

        std::vector<SceneNode> nodes;
    };
}
