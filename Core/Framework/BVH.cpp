#include <limits>
#include "Bvh.h"
#include "Matrix4.h"
#include "AffineTransform3.h"

using namespace FV;

std::optional<Vector3> BVH::rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option) const {
    std::optional<Vector3> rayHit = {};
    if (option == CloestHit) {
        auto numHits = rayTest(rayOrigin, dir, [&](const Vector3& p2) {
            if (rayHit.has_value()) {
                auto p1 = rayHit.value();
                auto sq1 = (p1 - rayOrigin).magnitudeSquared();
                auto sq2 = (p2 - rayOrigin).magnitudeSquared();
                if (sq2 < sq1) {
                    rayHit = p2;
                }
            } else {
                rayHit = p2;
            }
            return true;
        });
    } else if (option == LongestHit) {
        auto numHits = rayTest(rayOrigin, dir, [&](const Vector3& p2) {
            if (rayHit.has_value()) {
                auto p1 = rayHit.value();
                auto sq1 = (p1 - rayOrigin).magnitudeSquared();
                auto sq2 = (p2 - rayOrigin).magnitudeSquared();
                if (sq2 > sq1) {
                    rayHit = p2;
                }
            } else {
                rayHit = p2;
            }
            return true;
        });
    } else {
        auto numHits = rayTest(rayOrigin, dir, [&](const Vector3& p2) {
            rayHit = p2;
            return false;
        });
    }
    return rayHit;
}

uint32_t BVH::rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool (const Vector3&)> filter) const {
    if (aabb.isNull()) return 0;

    Vector3 origin = aabb.min;
    Vector3 scale = aabb.extents();
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
        const Node& node = volumes.at(index);
        AABB aabb = {
            Vector3(float(node.aabbMin[0]), float(node.aabbMin[1]), float(node.aabbMin[2])) * q,
            Vector3(float(node.aabbMax[0]), float(node.aabbMax[1]), float(node.aabbMax[2])) * q
        };

        auto r = aabb.rayTest(rayStart, rayDir);
        if (r >= 0.0f) {
            numHits++;
            Vector3 hitPoint = (rayOrigin + dir * r).applying(quantize);
            if (filter(hitPoint) == false)
                break;
            index++;
        } else {
            index += node.strideToNextSibling;
        }
    }
    return numHits;
}
