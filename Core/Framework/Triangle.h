#pragma once
#include "../include.h"
#include <algorithm>
#include <optional>
#include "Vector3.h"

#pragma pack(push, 4)
namespace FV {
    struct AABB;
    struct FVCORE_API Triangle {
        Vector3 p0, p1, p2;

        float area() const;
        AABB aabb() const;
        Vector3 barycentric(const Vector3&) const;

        struct RayTestResult {
            float t;    // distance from ray origin
            float u, v; // barycentric coordinates of intersection point inside the triangle.
                        // intersection point T(u,v) = (1-u-v)*p0 + u*p1 + v*p2
        };
        std::optional<RayTestResult> rayTest(const Vector3& origin, const Vector3& dir) const;
        std::optional<RayTestResult> rayTestCW(const Vector3& origin, const Vector3& dir) const;
        std::optional<RayTestResult> rayTestCCW(const Vector3& origin, const Vector3& dir) const;

        struct LineSegment {
            Vector3 p0, p1;
        };
        std::optional<LineSegment> intersectionTest(const Triangle&) const;
        bool intersects(const Triangle&) const;
    };
}
#pragma pack(pop)
