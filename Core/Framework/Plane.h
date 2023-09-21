#pragma once
#include "../include.h"
#include <optional>
#include "Vector3.h"
#include "Vector4.h"

namespace FV {
    struct FVCORE_API Plane {
        union {
            struct {
                float a, b, c, d;
            };
            float val[4];
        };

        Plane(const Vector3& v1, const Vector3& v2, const Vector3& v3) {
            auto n = Vector3::cross(v2 - v1, v3 - v1).normalized();
            a = n.x;
            b = n.y;
            c = n.z;
            d = -Vector3::dot(n, v1);
        }

        Plane(const Vector3& n, const Vector3& p)
            : a(n.x), b(n.y), c(n.z), d(-Vector3::dot(n, p)) {
        }

        explicit Plane(const Vector4& v)
            : a(v.x), b(v.y), c(v.z), d(v.w) {
        }

        float dot(const Vector3& v) const {
            return Vector4::dot(vector4(), Vector4(v.x, v.y, v.z, 1.0f));
        }

        float dot(const Vector4& v) const {
            return Vector4::dot(vector4(), v);
        }

        Vector3 normal() const {
            return Vector3(a, b, c);
        }

        Vector4 vector4() const {
            return Vector4(a, b, c, d);
        }

        std::optional<Vector3> rayTest(const Vector3& rayOrigin, const Vector3& rayDir) const;
    };
}
