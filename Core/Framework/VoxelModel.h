#pragma once
#include "../include.h"
#include <vector>
#include <bit>
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
            uint16_t x, y, z;
            uint8_t depth;
            uint8_t flags;
            uint32_t advance;
            Color::RGBA8 color;
            
            bool isLeaf() const { return flags != 0; }
            AABB aabb() const {
                constexpr float q = 1.0f / float(std::numeric_limits<uint16_t>::max());
                uint32_t exp = (126 - depth) << 23;
                float halfExtent = std::bit_cast<float>(exp);
                Vector3 ext = Vector3(halfExtent, halfExtent, halfExtent);
                Vector3 center = Vector3(float(x), float(y), float(z)) * q;
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

    struct Voxel {
        Color::RGBA8 color;
        uint16_t metallic;
        uint16_t roughness;

        bool operator==(const Voxel& other) const {
            return color.value == other.color.value &&
                metallic == other.metallic &&
                roughness == other.roughness;
        }
    };

    struct FVCORE_API VoxelOctree {
        Voxel value;
        VoxelOctree* subdivisions[8] = {};

        static constexpr auto maxDepth = 10U; // 2^(10*3)

        bool isLeafNode() const {
            auto merge = [&]<int... N>(std::integer_sequence<int, N...>) {
                return (uintptr_t(subdivisions[N]) | ...);
            };
            return merge(std::make_integer_sequence<int, 8>{}) == 0;
        }

        static constexpr float halfExtent(uint32_t depth) {
            uint32_t exp = (126 - std::clamp(depth, 0U, 125U)) << 23;
            return std::bit_cast<float>(exp);
        }

        size_t numDescendants() const {
            size_t n = 1;
            for (auto p : subdivisions) {
                if (p)
                    n += p->numDescendants();
            }
            return n;
        }

        size_t numLeafNodes() const {
            bool isLeaf = true;
            size_t n = 0;
            for (auto p : subdivisions) {
                if (p) {
                    isLeaf = false;
                    n += p->numLeafNodes();
                }
            }
            if (isLeaf)
                return 1;
            return n;
        }

        uint32_t maxDepthLevels() const {
            uint32_t level = 0;
            for (auto p : subdivisions) {
                if (p) {
                    level = std::max(level, p->maxDepthLevels() + 1);
                }
            }
            return level;
        }

        template <typename T> requires std::is_invocable_v<T, const Vector3&, uint32_t, const VoxelOctree&>
        void enumerate(const Vector3& center, uint32_t depth, uint32_t numDepthLevels, T&& fn) const {
            fn(center, depth, *this);
            if (numDepthLevels > 0) {
                float halfExtent = halfExtent(depth);
                auto pivot = center - Vector3(halfExtent, halfExtent, halfExtent) * 0.5f;

                for (int i = 0; i < 8; ++i) {
                    auto node = subdivisions[i];
                    if (node == nullptr) continue;

                    const int x = i & 1;
                    const int y = (i >> 1) & 1;
                    const int z = (i >> 2) & 1;

                    Vector3 pt = pivot;
                    pt.x += halfExtent * x;
                    pt.y += halfExtent * y;
                    pt.z += halfExtent * z;
                    node->enumerate(pt, depth+1, numDepthLevels-1, std::forward<T>(fn));
                }
            }
        }

        bool mergeSolidBranches();

        VolumeArray makeArray(const AABB& aabb, uint32_t maxDepth) const;
    };

    class VoxelOctreeBuilder {
    public:
        ~VoxelOctreeBuilder() {}

        using VolumeID = const void*;
        virtual AABB aabb() = 0;
        virtual bool volumeTest(const AABB&, VolumeID, VolumeID) = 0;
        virtual Voxel value(const AABB&, VolumeID) = 0;
        virtual void clear(VolumeID) = 0;
    };

    class DispatchQueue;
    class FVCORE_API VoxelModel {
    public:
        VoxelModel(const AABB& aabb, int depth);
        VoxelModel(VoxelOctreeBuilder*, int depth);
        VoxelModel(VoxelOctreeBuilder*, int depth, DispatchQueue&);
        ~VoxelModel();

        void update(uint32_t x, uint32_t y, uint32_t z, const Voxel& value);
        void erase(uint32_t x, uint32_t y, uint32_t z);
        std::optional<Voxel> lookup(uint32_t x, uint32_t y, uint32_t z) const;

        int enumerateLevel(int depth, std::function<void(const AABB&, uint32_t, const VoxelOctree&)>) const;

        const VoxelOctree* root() const { return _root; }
        uint32_t resolution() const { return 1ULL << _maxDepth; }

        void setDepth(uint32_t depth);
        uint32_t depth() const { return _maxDepth; }
        void setScale(float scale);
        float scale() const { return _scale; }
        AABB aabb() const;

        void optimize();

        struct ForEachNode {
            VoxelOctree* node;
            template <typename T> requires std::invocable<T, VoxelOctree*>
            void forward(T&& callback) { // invoked top-down
                callback(node);
                for (auto p : node->subdivisions) {
                    if (p)
                        ForEachNode{ p }.forward(std::forward<T>(callback));
                }
            }
            template <typename T> requires std::invocable<T, VoxelOctree*>
            void backward(T&& callback) { // invoked bottom-up
                for (auto p : node->subdivisions) {
                    if (p)
                        ForEachNode{ p }.backward(std::forward<T>(callback));
                }
                callback(node);
            }
        };

        template <typename T> requires std::invocable<T, VoxelOctree*>
        void forEach(bool forward, T&& fn) {
            if (_root) {
                if (forward)
                    ForEachNode{ _root }.forward(std::forward<T>(fn));
                else
                    ForEachNode{ _root }.backward(std::forward<T>(fn));
            }
        }

        enum RayHitResultOption {
            AnyHit,
            CloestHit,
            LongestHit,
        };

        struct RayHitResult {
            float t;
            const VoxelOctree* node;
            struct { uint32_t x, y, z; } location;
            uint32_t depth;
        };

        std::optional<RayHitResult> rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option = RayHitResultOption::CloestHit) const;
        uint64_t rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool(const RayHitResult&)> filter) const;

    private:
        VoxelOctree* _root;
        Vector3 _center;
        float _scale;
        uint32_t _maxDepth;

        static void deleteNode(VoxelOctree*);
    };
}
