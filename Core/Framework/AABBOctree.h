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

        enum NodeFlagBit {
            FlagPayload = 1,
            FlagMaterial = 1 << 1,
        };

#pragma pack(push, 1)
        struct Node {
            uint16_t aabbCenter[3];
            uint8_t aabbHalfExtentExponent;
            uint8_t flags;
            union {
                uint64_t strideToNextSibling;
                uint64_t payload;
            };
            bool isLeaf() const { return (flags & FlagPayload) != 0; }
        };
#pragma pack(pop)

        AABB aabb;
        std::vector<Node> subdivisions;

        enum RayHitResultOption {
            AnyHit,
            CloestHit,
            LongestHit,
        };

        struct RayHitResult {
            Vector3 hitPoint;
            uint64_t payload;
        };
        std::optional<RayHitResult> rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option = CloestHit) const;
        uint32_t rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool(const RayHitResult&)> filter) const;
    };
}
