#pragma once
#include "../include.h"
#include <algorithm>
#include <optional>
#include <vector>
#include <functional>
#include "Vector3.h"
#include "AABB.h"
#include "AABBOctree.h"
#include "Triangle.h"
#include "Color.h"

namespace FV {
    struct VolumeArray {

        enum NodeFlagBit {
            FlagLeafNode = 1,
            FlagMaterial = 1 << 1,
            FlagPayload = 1 << 2,
        };

#pragma pack(push, 1)
        struct Node {
            uint16_t center[3];
            uint8_t depth;
            uint8_t flags;
            uint32_t advance;
            Color::RGBA8 color;
            
            bool isLeaf() const { return flags != 0; }
            AABB aabb() const {
                constexpr float q = 1.0f / float(std::numeric_limits<uint16_t>::max());
                float halfExtent = 0.5f;
                for (int i = 0; i < depth; ++i)
                    halfExtent = halfExtent * 0.5f;
                Vector3 ext = Vector3(halfExtent, halfExtent, halfExtent);
                Vector3 center = Vector3(float(this->center[0]),
                                         float(this->center[1]),
                                         float(this->center[2])) * q;
                return { center - ext, center + ext };
            }
            AABB aabb(const AABB& volume) const {
                AABB aabb = this->aabb();
                auto extents = volume.extents();
                return {
                    aabb.min * extents + volume.min,
                    aabb.max * extents + volume.min
                };
            }
        };
#pragma pack(pop)
        static_assert(sizeof(Node) == 16);

        std::vector<Node> data;
        AABB aabb;
    };

    struct FVCORE_API VolumeTree {
        Vector3 center;
        uint32_t depth; // aabb-extent-exponent

        constexpr static int maxDepth = 10; // 2^(10*3) = 2^30

        struct {
            Color::RGBA8 color;
            int materialIndex;
        };

        AABB aabb() const;
        AABB aabb(const AABB& volume) const {
            AABB aabb = this->aabb();
            auto extents = volume.extents();
            return {
                aabb.min * extents + volume.min,
                aabb.max * extents + volume.min
            };
        }

        VolumeTree* subdivisions[8];
        
        VolumeArray makeArray(const AABB& volume, int depthLevels) const;
        size_t numberOfDescendants() const;

        enum RayHitResultOption {
            AnyHit,
            CloestHit,
            LongestHit,
        };

        struct RayHitResult {
            float t;
            const VolumeTree* hit;
        };

        std::optional<RayHitResult> rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option = CloestHit) const;
        uint64_t rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool(const RayHitResult&)> filter) const;
    };

    class FVCORE_API VoxelModel {
    public:
        VoxelModel(const AABB& aabb, int depth);
        ~VoxelModel();

        int enumerateLevel(int depth, std::function<void(const VolumeTree&)>) const;

        const VolumeTree* root() const { return _root; }

        using RayHitResultOption = VolumeTree::RayHitResultOption;
        using RayHitResult = VolumeTree::RayHitResult;

        std::optional<RayHitResult> rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option = RayHitResultOption::CloestHit) const;
        uint64_t rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool(const RayHitResult&)> filter) const;

        AABB aabb;
    private:
        VolumeTree* _root;
        uint32_t maxDepth;
        void deleteNode(VolumeTree*);
    };
}
