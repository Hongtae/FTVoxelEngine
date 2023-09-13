#pragma once
#include "../include.h"
#include <algorithm>
#include "Vector3.h"

#ifndef NOMINMAX
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#endif
namespace FV {
    struct AABB {
        Vector3 min = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        };
        Vector3 max = {
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max()
        };
        
        bool isValid() const {
            return max.x >= min.x && max.y >= min.y && max.z >= min.z;
        }

        bool isPointInside(const Vector3& pt) const {
            return pt.x >= min.x && pt.x <= max.x &&
                pt.y >= min.y && pt.y <= max.y &&
                pt.z >= min.z && pt.z <= max.z;
        }

        AABB& expand(const Vector3& point) {
            if (isValid()) {
                min = Vector3::minimum(min, point);
                max = Vector3::maximum(max, point);
            } else {
                min = point;
                max = point;
            }
            return *this;
        }

        AABB intersection(const AABB& other) const {
            if (isValid() && other.isValid()) {
                return {
                    Vector3::maximum(min, other.min),
                    Vector3::minimum(max, other.max)
                };
            }
            return {};
        }

        AABB combining(const AABB& other) const {
            return AABB{ min, max }.combine(other);
        }

        AABB& combine(const AABB& other) {
            if (other.isValid()) {
                if (isValid()) {
                    min = Vector3::minimum(min, other.min);
                    max = Vector3::maximum(max, other.max);
                } else {
                    min = other.min;
                    max = other.max;
                }
            }
            return *this;
        }

        bool intersects(const AABB& other) const {
            return intersection(other).isValid();
        }

        Vector3 center() const {
            return (min + max) * 0.5f;
        }
    };
}
#ifndef NOMINMAX
#pragma pop_macro("min")
#pragma pop_macro("max")
#endif
