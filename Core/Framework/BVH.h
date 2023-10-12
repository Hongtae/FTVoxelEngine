#pragma once
#include "../include.h"
#include <algorithm>
#include <optional>
#include <vector>
#include <functional>
#include "Vector3.h"
#include "AABB.h"

namespace FV {
    struct FVCORE_API BVH {
#pragma pack(push, 1)
        struct Node {
            uint16_t aabbMin[3];
            uint16_t aabbMax[3];
            union {
                int32_t strideToNextSibling;
                int32_t payload;
            };
        };
#pragma pack(pop)

        AABB aabb;
        std::vector<Node> volumes;

        enum RayHitResultOption {
            AnyHit,
            CloestHit,
            LongestHit,
        };

        std::optional<Vector3> rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option = CloestHit) const;
        uint32_t rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool(const Vector3&)> filter) const;
    };
}
