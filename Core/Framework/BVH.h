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
            uint16_t aabbUnormMin[3];
            uint16_t aabbUnormMax[3];
            union {
                int32_t strideToNextSibling;
                int32_t payload;
            };
        };
#pragma pack(pop)

        AABB quantizedAABB;
        std::vector<Node> volumes;

        std::optional<Vector3> rayTest(const Vector3& rayOrigin, const Vector3& dir) const;
        uint32_t rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool(const Vector3&)> filter) const;
    };
}
