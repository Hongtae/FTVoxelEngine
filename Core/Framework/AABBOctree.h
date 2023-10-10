#pragma once
#include "../include.h"
#include <algorithm>
#include <optional>
#include <vector>
#include <functional>
#include "Vector3.h"
#include "AABB.h"

namespace FV {
    struct FVCORE_API AABBOctree {
#pragma pack(push, 1)
        struct Node {
            uint16_t aabbCenter[3];
            uint16_t aabbHalfExtent;
            union {
                int64_t strideToNextSibling;
                int64_t payload;
            };
            bool isLeaf() const { return payload < 0; }
        };
#pragma pack(pop)

        AABB quantizedAABB;
        std::vector<Node> volumes;

        enum RayHitResultOption {
            AnyHit,
            CloestHit,
            LongestHit,
        };

        struct RayHitResult {
            Vector3 hitPoint;
            int64_t payload;
        };
        std::optional<RayHitResult> rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option = CloestHit) const;
        uint32_t rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool(const RayHitResult&)> filter) const;
    };
}
