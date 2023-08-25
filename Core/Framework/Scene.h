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

namespace FV
{
    struct SceneState
    {
        ViewFrustum view;
        Matrix4 model;
    };

    struct AABB
    {
        Vector3 min = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        };
        Vector3 max = {
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max()
        };
        bool isValid() const 
        {
            return max.x > min.x && max.y > min.y && max.z > min.z;
        }
        bool isPointInside(const Vector3& pt) const
        {
            return pt.x >= min.x && pt.x <= max.x &&
                pt.y >= min.y && pt.y <= max.y &&
                pt.z >= min.z && pt.z <= max.z;
        }
    };

    struct FVCORE_API SceneNode
    {
        std::string name;
        std::optional<Mesh> mesh;

        Vector3 scale = Vector3(1, 1, 1);
        Transform transform = Transform::identity;

        std::vector<SceneNode> children;

        void draw(RenderCommandEncoder*, const SceneState&) const;
    };

    class FVCORE_API Scene
    {
    public:
        Scene();
        ~Scene();

        std::vector<SceneNode> nodes;
    };
}
