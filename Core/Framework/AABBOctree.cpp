#include "AABBOctree.h"
#include "Matrix4.h"
#include "AffineTransform3.h"

using namespace FV;

std::optional<AABBOctree::RayHitResult>
AABBOctree::rayTest(const Vector3& rayOrigin, const Vector3& dir,
                    RayHitResultOption option) const {
    std::optional<RayHitResult> rayHit = {};
    if (option == CloestHit) {
        auto numHits = rayTest(rayOrigin, dir, [&](const auto& p2) {
            if (rayHit.has_value()) {
                auto p1 = rayHit.value();
                auto sq1 = (p1.hitPoint - rayOrigin).magnitudeSquared();
                auto sq2 = (p2.hitPoint - rayOrigin).magnitudeSquared();
                if (sq2 < sq1) {
                    rayHit = p2;
                }
            } else {
                rayHit = p2;
            }
            return true;
        });
    } else if (option == LongestHit) {
        auto numHits = rayTest(rayOrigin, dir, [&](const auto& p2) {
            if (rayHit.has_value()) {
                auto p1 = rayHit.value();
                auto sq1 = (p1.hitPoint - rayOrigin).magnitudeSquared();
                auto sq2 = (p2.hitPoint - rayOrigin).magnitudeSquared();
                if (sq2 > sq1) {
                    rayHit = p2;
                }
            } else {
                rayHit = p2;
            }
            return true;
        });
    } else {
        auto numHits = rayTest(rayOrigin, dir, [&](const auto& p2) {
            rayHit = p2;
            return false;
        });
    }
    return rayHit;
}

uint32_t AABBOctree::rayTest(const Vector3& rayOrigin,
                             const Vector3& dir,
                             std::function<bool(const RayHitResult&)> filter) const {
    if (quantizedAABB.isNull()) return 0;

    Vector3 origin = quantizedAABB.min;
    Vector3 scale = quantizedAABB.extents();
    for (float& s : scale.val) {
        if (s == 0.0f) s = 1.0;
    }

    auto quantize = AffineTransform3::identity.scaled(scale).translated(origin);
    auto normalize = quantize.inverted();

    auto rayStart = rayOrigin.applying(normalize);
    auto rayDir = dir.applying(normalize.matrix3); // linear only

    uint32_t numHits = 0;
    constexpr float q = 1.0f / float(std::numeric_limits<uint16_t>::max());

    uint64_t index = 0;
    while (index < volumes.size()) {
        const auto& node = volumes.at(index);
        Vector3 center = Vector3(float(node.aabbCenter[0]), float(node.aabbCenter[1]), float(node.aabbCenter[2])) * q;
        float halfExtent = float(node.aabbHalfExtent) * q;

        AABB aabb = {
            center - Vector3(halfExtent, halfExtent, halfExtent),
            center + Vector3(halfExtent, halfExtent, halfExtent)
        };

        auto r = aabb.rayTest(rayStart, rayDir);
        if (r.has_value()) {
            if (node.isLeaf()) {
                numHits++;
                Vector3 hitPoint = r.value().applying(quantize);
                if (filter(RayHitResult{ hitPoint, node.payload }) == false)
                    break;
            }
            index++;
        } else {
            index += node.strideToNextSibling;
        }
    }
    return numHits;
}
