#pragma once
#include "../include.h"
#include "Vector3.h"

namespace FV {
    struct Sphere {
        Vector3 center;
        float radius;

        float rayTest(const Vector3& origin, const Vector3& dir) const {
            if (radius >= 0.0f) {
                auto d = dir.normalized();

                Vector3 oc = origin - center;
                float b = 2.0f * Vector3::dot(oc, d);
                float c = oc.magnitudeSquared() - radius * radius;
                float discriminant = b * b - 4 * c;
                if (discriminant < 0.0f)
                    return -1.0f;
                return (-b - sqrt(discriminant)) * 0.5f;
            }
            return -1.0f;
        }

        bool isPointInside(const Vector3& pt) const {
            return (pt - center).magnitudeSquared() <= (radius * radius);
        }
    };
}
