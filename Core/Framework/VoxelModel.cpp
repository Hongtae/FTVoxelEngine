#include "AffineTransform3.h"
#include "VoxelModel.h"

using namespace FV;

AABB VolumeTree::aabb() const {
    float halfExtent = 0.5f;
    for (uint32_t i = 0; i < depth; ++i) halfExtent *= 0.5f;

    return {
        center - Vector3(halfExtent, halfExtent, halfExtent),
        center + Vector3(halfExtent, halfExtent, halfExtent)
    };
}

VolumeArray VolumeTree::makeArray(const AABB& volume, int depthLevels) const {

    struct MakeArray {
        const VolumeTree* node;
        uint32_t maxDepth;
        void operator() (std::vector<VolumeArray::Node>& vector) const {
            auto index = vector.size();
            vector.push_back({});
            auto& n = vector.at(index);
            constexpr float q = float(std::numeric_limits<uint16_t>::max());
            n.center[0] = static_cast<uint16_t>(node->center.x * q);
            n.center[1] = static_cast<uint16_t>(node->center.y * q);
            n.center[2] = static_cast<uint16_t>(node->center.z * q);
            n.depth = node->depth;
            n.flags = 0;
            n.color = node->color;

            if (node->depth < maxDepth) {
                for (const VolumeTree* c : node->subdivisions) {
                    if (c) {
                        MakeArray{ c, maxDepth }(vector);
                    }
                }
            }
            if (vector.size() == index) {
                n.flags |= VolumeArray::FlagLeafNode;
            }
            auto stride = vector.size() - index;

            constexpr auto maxLength = std::numeric_limits<uint32_t>::max();
            FVASSERT_DEBUG(uint64_t(stride) < uint64_t(maxLength));

            n.advance = uint32_t(stride) + 1;
        }
    };

    VolumeArray va = {};
    va.aabb = volume;
    va.data.reserve(numberOfDescendants());

    depthLevels = std::clamp(depthLevels, 0, VolumeTree::maxDepth);
    auto depthLimits = this->depth + uint32_t(depthLevels);

    MakeArray{ this, depthLimits }(va.data);
    va.data.shrink_to_fit();

    return va;
}

size_t VolumeTree::numberOfDescendants() const {
    size_t d = 1;
    for (const VolumeTree* c : subdivisions) {
        d += c->numberOfDescendants();
    }
    return d;
}

std::optional<VolumeTree::RayHitResult>
VolumeTree::rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option) const {
    std::optional<RayHitResult> rayHit = {};
    if (option == CloestHit) {
        auto numHits = rayTest(
            rayOrigin, dir, [&](const auto& p2) {
                if (rayHit.has_value()) {
                    auto p1 = rayHit.value();
                    if (p1.t > p2.t)
                        rayHit = p2;
                } else {
                    rayHit = p2;
                }
                return true;
            });
    } else if (option == LongestHit) {
        auto numHits = rayTest(
            rayOrigin, dir, [&](const auto& p2) {
                if (rayHit.has_value()) {
                    auto p1 = rayHit.value();
                    if (p2.t > p1.t) {
                        rayHit = p2;
                    }
                } else {
                    rayHit = p2;
                }
                return true;
            });
    } else {
        auto numHits = rayTest(
            rayOrigin, dir, [&](const auto& p2) {
                rayHit = p2;
                return false;
            });
    }
    return rayHit;
}

uint64_t VolumeTree::rayTest(const Vector3& rayOrigin,
                              const Vector3& dir,
                              std::function<bool(const RayHitResult&)> filter) const {
    struct RayTestNode {
        const VolumeTree* node;
        bool& continueRayTest;
        std::function<bool(const RayHitResult&)> filter;
        uint64_t rayTest(const Vector3& start, const Vector3& dir) const {
            auto r = node->aabb().rayTest(start, dir);
            if (r >= 0.0f) {
                bool leafNode = true;
                uint64_t numHits = 0;
                for (const VolumeTree* n : node->subdivisions) {
                    if (n) {
                        leafNode = false;
                        if (!continueRayTest)
                            break;
                        numHits += RayTestNode{n, continueRayTest, filter}.rayTest(start, dir);
                    }
                }
                if (leafNode) {
                    if (filter(RayHitResult{ r, node }) == false) {
                        continueRayTest = false;
                    }
                    return 1;
                }
                return numHits;
            }
            return 0;
        }
    };
    bool continueRayTest = true;
    return RayTestNode{ this, continueRayTest, filter }.rayTest(rayOrigin, dir);
}

VoxelModel::VoxelModel(const AABB& volume, int depth)
    : _root(nullptr)
    , aabb(volume)
    , maxDepth(std::max(depth, 0)) {

}

VoxelModel::~VoxelModel() {
    deleteNode(_root);
}

int VoxelModel::enumerateLevel(int depth, std::function<void(const VolumeTree&)> cb) const {
    using Callback = std::function<void(const VolumeTree&)>;

    struct IterateDepth {
        const VolumeTree* node;
        int level;
        Callback& callback;
        void operator() (int depth, Callback& cb) {
            if (level < depth) {
                for (const VolumeTree* c : node->subdivisions) {
                    if (c)
                        IterateDepth{ c, level + 1, cb }(depth, cb);
                }
            } else {
                cb(*node);
            }
        }
    };
    if (_root) {
        IterateDepth{ _root, 0, cb }(std::max(0, depth), cb);
    }
    return 0;
}

void VoxelModel::deleteNode(VolumeTree* node) {
    if (node) {
        for (VolumeTree* c : node->subdivisions)
            deleteNode(c);
        delete node;
    }
}

std::optional<VoxelModel::RayHitResult> VoxelModel::rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option) const {
    if (aabb.isNull())
        return {};

    Vector3 origin = aabb.min;
    Vector3 scale = aabb.extents();
    FVASSERT_DEBUG(scale.x * scale.y * scale.z != 0.0f);

    auto quantize = AffineTransform3::identity.scaled(scale).translated(origin);
    auto normalize = quantize.inverted();

    auto rayStart = rayOrigin.applying(normalize);
    auto rayDir = dir.applying(normalize.matrix3);

    FVASSERT_DEBUG(_root);
    if (auto result = _root->rayTest(rayStart, rayDir, option); result.has_value()) {
        auto r = result.value();
        Vector3 hit = rayStart + rayDir * r.t;
        hit.apply(quantize);
        return RayHitResult{ (hit - rayOrigin).magnitude(), r.hit };
    }
    return {};
}

uint64_t VoxelModel::rayTest(const Vector3& rayOrigin, const Vector3& dir,
                             std::function<bool(const RayHitResult&)> filter) const {
    if (aabb.isNull())
        return 0;

    Vector3 origin = aabb.min;
    Vector3 scale = aabb.extents();
    FVASSERT_DEBUG(scale.x * scale.y * scale.z != 0.0f);

    auto quantize = AffineTransform3::identity.scaled(scale).translated(origin);
    auto normalize = quantize.inverted();

    auto rayStart = rayOrigin.applying(normalize);
    auto rayDir = dir.applying(normalize.matrix3);

    bool continueRayTest = true;

    FVASSERT_DEBUG(_root);
    return _root->rayTest(
        rayStart, rayDir, [&](const VolumeTree::RayHitResult& result)->bool {
            Vector3 hitPoint = rayStart + rayDir * result.t;
            hitPoint.apply(quantize);
            auto t = (hitPoint - rayOrigin).magnitude();
            return filter({ t, result.hit });
        });
}
