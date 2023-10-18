#pragma once
#include "../include.h"
#include <algorithm>
#include <optional>
#include <vector>
#include <functional>
#include "Vector3.h"
#include "AABB.h"
#include "Triangle.h"

namespace FV {

    struct FVCORE_API AABBOctreeLayer {
        using Payload = uint32_t;
        using Index = uint32_t;

        enum NodeFlagBit {
            FlagPayload = 1,
            FlagMaterial = 1 << 1,
        };

#pragma pack(push, 1)
        struct Node {
            uint16_t center[3];
            uint8_t depth;
            uint8_t flags;
            union {
                Index strideToNextSibling;
                Payload payload;
            };
            uint32_t _padding;
            bool isLeaf() const { return (flags & FlagPayload) != 0; }
        };
#pragma pack(pop)

        AABB aabb;
        std::vector<Node> data;

        enum RayHitResultOption {
            AnyHit,
            CloestHit,
            LongestHit,
        };

        struct RayHitResult {
            Vector3 hitPoint;
            Payload payload;
        };
        std::optional<RayHitResult> rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option = CloestHit) const;
        uint32_t rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool(const RayHitResult&)> filter) const;
    };

    struct FVCORE_API AABBOctree {
        using Payload = AABBOctreeLayer::Payload;
        AABB aabb;
        uint32_t maxDepth;
        uint64_t numDescendants;
        uint64_t numLeafNodes;

        using TriangleQuery = std::function<const Triangle& (uint64_t)>;
        // (triangle-indices, num-indices, aabb-center)
        using PayloadQuery = std::function<Payload (uint64_t*, size_t, const Vector3&)>;

        struct Node {
            Vector3 center;
            uint32_t depth; // aabb-extent-exponent
            Payload payload;
            std::vector<Node> subdivisions;
            AABB aabb() const {
                float halfExtent = 0.5f;
                for (uint32_t i = 0; i < depth; ++i) halfExtent *= 0.5f;

                return {
                    center - Vector3(halfExtent, halfExtent, halfExtent),
                    center + Vector3(halfExtent, halfExtent, halfExtent)
                };
            }
        };
        Node root;

        size_t _numberOfDescendants() const;
        size_t _numberOfLeafNodes() const;

        static std::shared_ptr<AABBOctree> makeTree(uint32_t maxDepth, uint64_t numTriangles, uint64_t baseIndex, TriangleQuery, PayloadQuery);
        std::shared_ptr<AABBOctreeLayer> makeLayer(uint32_t maxDepth) const;

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
        uint64_t rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool(const RayHitResult&)> filter) const;
    };
}
