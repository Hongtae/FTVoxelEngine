#pragma once
#include "../include.h"
#include <algorithm>
#include <optional>
#include "Vector3.h"

#ifndef NOMINMAX
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#endif

#pragma pack(push, 4)
namespace FV {
    struct Triangle;
    struct Plane;
    struct FVCORE_API AABB {
        Vector3 min;
        Vector3 max;
        
        static const AABB null;

        AABB();
        AABB(const Vector3& _min, const Vector3& _max);

        bool isNull() const {
            return max.x < min.x || max.y < min.y || max.z < min.z; 
        }

        bool isPointInside(const Vector3& pt) const {
            return pt.x >= min.x && pt.x <= max.x &&
                pt.y >= min.y && pt.y <= max.y &&
                pt.z >= min.z && pt.z <= max.z;
        }

        AABB& expand(const Vector3& point) {
            if (isNull()) {
                min = point;
                max = point;
            } else {
                min = Vector3::minimum(min, point);
                max = Vector3::maximum(max, point);
            }
            return *this;
        }

        AABB& expand(std::initializer_list<Vector3> pts) {
            for (auto& p : pts) expand(p);
            return *this;
        }

        AABB intersection(const AABB& other) const {
            if (isNull() || other.isNull()) {
                return {};
            }
            return {
                Vector3::maximum(min, other.min),
                Vector3::minimum(max, other.max)
            };
        }

        AABB combining(const AABB& other) const {
            return AABB{ min, max }.combine(other);
        }

        AABB& combine(const AABB& other) {
            if (other.isNull() == false) {
                if (isNull()) {
                    min = other.min;
                    max = other.max;
                } else {
                    min = Vector3::minimum(min, other.min);
                    max = Vector3::maximum(max, other.max);
                }
            }
            return *this;
        }

        bool intersects(const AABB& other) const {
            return intersection(other).isNull() == false;
        }

        Vector3 center() const {
            return (min + max) * 0.5f;
        }

        Vector3 extents() const {
            if (isNull()) return Vector3::zero;
            return (max - min);
        }

        std::optional<Vector3> rayTest(const Vector3& rayOrigin, const Vector3& rayDir) const;
        bool overlapTest(const Plane& plane) const;
        bool overlapTest(const Triangle& tri) const;
        bool overlapTest(const AABB& aabb) const;
    };
}
#pragma pack(pop)

#ifndef NOMINMAX
#pragma pop_macro("min")
#pragma pop_macro("max")
#endif
