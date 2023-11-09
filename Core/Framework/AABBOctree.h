#pragma once
#include "../include.h"
#include <vector>
#include "Vector3.h"
#include "AABB.h"
#include "Triangle.h"
#include "Color.h"

namespace FV {
    struct FVCORE_API AABBOctreeLayer {
        using Payload = uint64_t;
        using Index = uint32_t;

        struct Material {
            Color::RGBA8 color;
            int materialIndex;
        };

        enum NodeFlagBit {
            FlagMaterial = 1,
            FlagPayload = 1 << 1,
        };

#pragma pack(push, 1)
        struct Node {
            uint16_t center[3];
            uint8_t depth;
            uint8_t flags;
            union {
                Index strideToNextSibling;
                Color::RGBA8 color;
                Material material;
                Payload payload;
            };
            bool isLeaf() const { return flags != 0; }
        };
#pragma pack(pop)
        static_assert((sizeof(Node) % 16) == 0); // must be 16 bytes alignment

        AABB aabb;
        std::vector<Node> data;

        enum RayHitResultOption {
            AnyHit,
            CloestHit,
            LongestHit,
        };

        struct RayHitResult {
            Vector3 hitPoint;
            const Node* node;
        };
        std::optional<RayHitResult> rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option = CloestHit) const;
        uint32_t rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool(const RayHitResult&)> filter) const;
    };

    struct FVCORE_API AABBOctree {
        using Material = AABBOctreeLayer::Material;

        AABB aabb;
        uint32_t maxDepth;
        uint64_t numDescendants;
        uint64_t numLeafNodes;

        using TriangleQuery = std::function<Triangle (uint64_t)>;
        // (triangle-indices, num-indices, aabb-center)
        using MaterialQuery = std::function<Material (uint64_t*, size_t, const Vector3&)>;

        struct Node {
            Vector3 center;
            uint32_t depth; // aabb-extent-exponent
            union {
                Color::RGBA8 color;
                Material material;
            };
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

        static std::shared_ptr<AABBOctree> makeTree(uint32_t maxDepth, uint64_t numTriangles, uint64_t baseIndex, TriangleQuery, MaterialQuery);
        std::shared_ptr<AABBOctreeLayer> makeLayer(uint32_t maxDepth) const;

        enum RayHitResultOption {
            AnyHit,
            CloestHit,
            LongestHit,
        };

        struct RayHitResult {
            Vector3 hitPoint;
            const Node* node;
        };
        std::optional<RayHitResult> rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option = CloestHit) const;
        uint64_t rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool(const RayHitResult&)> filter) const;
    };
}
