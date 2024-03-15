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

#pragma pack(push, 1)
    struct Voxel {
        Color::RGBA8 color;
        uint8_t metallic;
        uint8_t roughness;

        bool operator==(const Voxel& other) const {
            return color.value == other.color.value &&
                metallic == other.metallic &&
                roughness == other.roughness;
        }
    };
#pragma pack(pop)

#pragma pack(push, 4)
    struct FVCORE_API VoxelOctree {
        Voxel value;
        uint8_t subdivisionMasks = 0;
        VoxelOctree* subdivisions = nullptr;

        VoxelOctree();
        VoxelOctree(const Voxel&);
        VoxelOctree(const VoxelOctree&);
        VoxelOctree(VoxelOctree&&);
        ~VoxelOctree();

        VoxelOctree& operator=(const VoxelOctree&);
        VoxelOctree& operator=(VoxelOctree&&);

        VoxelOctree deepCopy() const;

        static constexpr auto maxDepth = 124U;

        bool isLeafNode() const {
            return subdivisionMasks == 0;
        }

        uint8_t numSubdivisions() const {
            return std::popcount(subdivisionMasks);
        }

        void subdivide(std::initializer_list<uint8_t> indices);
        void subdivide(uint8_t mask);
        void erase(std::initializer_list<uint8_t> indices);
        void erase(uint8_t mask);

        static constexpr float halfExtent(uint32_t depth) {
            uint32_t exp = (126 - std::clamp(depth, 0U, 125U)) << 23;
            return std::bit_cast<float>(exp);
        }

        size_t numDescendants() const {
            size_t n = 1;
            enumerate([&](uint8_t, const VoxelOctree* node) {
                n += node->numDescendants();
            });
            return n;
        }

        size_t countNodesToDepth(uint32_t depth, uint32_t cdepth = 0) const {
            size_t n = 1;
            if (depth > cdepth) {
                enumerate([&](uint8_t, const VoxelOctree* node) {
                    n += node->countNodesToDepth(depth, cdepth + 1);
                });
            }
            return n;
        }

        size_t numLeafNodes() const {
            if (isLeafNode()) return 1;
            size_t n = 0;
            enumerate([&](uint8_t, const VoxelOctree* node) {
                n += node->numLeafNodes();
            });
            return n;
        }

        uint32_t maxDepthLevels() const {
            uint32_t level = 0;
            enumerate([&](uint8_t, const VoxelOctree* node) {
                level = std::max(level, node->maxDepthLevels() + 1);
            });
            return level;
        }

        template <typename T> requires std::is_invocable_v<T, uint8_t, VoxelOctree*>
        void enumerate(T&& fn) {
            VoxelOctree* child = subdivisions;
            for (uint8_t i = 0; i < 8; ++i) {
                if ((subdivisionMasks >> i) & 1) {
                    fn(i, child);
                    child += 1;
                }
            }
        }

        template <typename T> requires std::is_invocable_v<T, uint8_t, const VoxelOctree*>
        void enumerate(T&& fn) const {
            const VoxelOctree* child = subdivisions;
            for (uint8_t i = 0; i < 8; ++i) {
                if ((subdivisionMasks >> i) & 1) {
                    fn(i, child);
                    child += 1;
                }
            }
        }

        template <typename T> requires std::is_invocable_v<T, const AABB&, const VoxelOctree*>
        void enumerate(const AABB& aabb, T&& fn) const {
            const auto pivot = aabb.min;
            const auto halfExtents = aabb.extents() * 0.5f;

            enumerate([&](uint8_t i, const VoxelOctree* node) {
                const int x = i & 1;
                const int y = (i >> 1) & 1;
                const int z = (i >> 2) & 1;

                Vector3 pt = {
                    pivot.x + halfExtents.x * x,
                    pivot.y + halfExtents.y * y,
                    pivot.z + halfExtents.z * z
                };
                AABB aabb2 = { pt, pt + halfExtents };
                fn(aabb2, node);
            });
        }

        template <typename T> requires std::is_invocable_v<T, const Vector3&, uint32_t, const VoxelOctree*>
        void enumerate(const Vector3& center, uint32_t depth, T&& fn) const {
            const auto hext = halfExtent(depth);

            enumerate([&](uint8_t i, const VoxelOctree* node) {
                const int x = i & 1;
                const int y = (i >> 1) & 1;
                const int z = (i >> 2) & 1;

                Vector3 pt = {
                    center.x + hext * (float(x) - 0.5f),
                    center.y + hext * (float(y) - 0.5f),
                    center.z + hext * (float(z) - 0.5f),
                };
                fn(pt, depth + 1, node);
            });
        }

        bool mergeSolidBranches();
        /* Filter (current-AABB, current-depth, maxDepth) */
        using MakeArrayFilter = std::function<void (const AABB&, uint32_t, uint32_t&)>;
        VolumeArray makeArray(uint32_t maxDepth, 
                              MakeArrayFilter = {}) const;

        VolumeArray makeSubarray(const Vector3& nodeCenter,
                                 uint32_t currentLevel,
                                 uint32_t maxDepth) const;

        using MakeArrayCallback = std::function<void(const Vector3&, uint32_t, float, const VoxelOctree*, std::vector<VolumeArray::Node>&)>;
        VolumeArray makeArray(MakeArrayCallback) const;

        using VolumePriorityCallback = std::function<float(const Vector3&, uint32_t)>;
        void makeSubarray(const Vector3& center,
                          uint32_t level,
                          const MakeArrayCallback&,
                          const VolumePriorityCallback&,
                          std::vector<VolumeArray::Node>& vector) const;

        void makeSubarray(const Vector3& nodeCenter,
                          uint32_t currentLevel,
                          uint32_t maxDepth,
                          const VolumePriorityCallback&,
                          std::vector<VolumeArray::Node>& vector) const;

        void makeSubarray(const Vector3& nodeCenter,
                          uint32_t currentLevel,
                          uint32_t maxDepth,
                          std::vector<VolumeArray::Node>& vector) const;
    };
#pragma pack(pop)

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
        VoxelModel(int depth);
        VoxelModel(VoxelOctreeBuilder*, int depth);
        ~VoxelModel();

        void update(uint32_t x, uint32_t y, uint32_t z, const Voxel& value);
        void erase(uint32_t x, uint32_t y, uint32_t z);
        std::optional<Voxel> lookup(uint32_t x, uint32_t y, uint32_t z) const;

        int enumerateLevel(int depth, std::function<void(const AABB&, uint32_t, const VoxelOctree*)>) const;
        int enumerateLevel(int depth, std::function<void(const Vector3&, uint32_t, const VoxelOctree*)>) const;

        const VoxelOctree* root() const { return _root; }
        uint32_t resolution() const { return 1ULL << _maxDepth; }

        size_t numNodes() const {
            if (_root) return _root->numDescendants();
            return 0U;
        }
        size_t numLeafNodes() const {
            if (_root) return _root->numLeafNodes();
            return 0U;
        }

        void setDepth(uint32_t depth);
        uint32_t depth() const { return _maxDepth; }

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

        bool deserialize(std::istream&);
        uint64_t serialize(std::ostream&) const;

        struct {
            Vector3 center = { 0, 0, 0 };
            float scale = 0.0f;
        } metadata;
    private:
        VoxelOctree* _root;
        uint32_t _maxDepth;

        static void deleteNode(VoxelOctree*);
    };
}
