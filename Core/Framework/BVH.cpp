#include "Bvh.h"
#include "Matrix4.h"
#include "AffineTransform3.h"

using namespace FV;

std::optional<Vector3> BVH::rayTest(const Vector3& rayOrigin, const Vector3& dir) const {
    std::optional<Vector3> rayHit = {};
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
    return rayHit;
}

uint32_t BVH::rayTest(const Vector3& rayOrigin, const Vector3& dir, std::function<bool (const Vector3&)> filter) const {
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

    for (const Node& node : volumes) {
        AABB aabb = {
            Vector3(float(node.aabbUnormMin[0]), float(node.aabbUnormMin[1]), float(node.aabbUnormMin[2])) * q,
            Vector3(float(node.aabbUnormMax[0]), float(node.aabbUnormMax[1]), float(node.aabbUnormMax[2])) * q
        };

        auto r = aabb.rayTest(rayStart, rayDir);
        if (r.has_value()) {
            Vector3 hitPoint = r.value().applying(quantize);
            if (filter(hitPoint) == false)
                break;
        }
    }
    return numHits;
}
