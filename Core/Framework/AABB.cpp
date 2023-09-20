#include <algorithm>
#include <limits>
#include "AABB.h"
#include "Triangle.h"

using namespace FV;

constexpr auto epsilon = std::numeric_limits<float>::epsilon();
constexpr auto fltmax = std::numeric_limits<float>::max();

static const AABB null = AABB(Vector3(fltmax, fltmax, fltmax), Vector3(-fltmax, -fltmax, -fltmax));

AABB::AABB()
    : min(null.min)
    , max(null.max) {
}

AABB::AABB(const Vector3& _min, const Vector3& _max)
    : min(_min), max(_max) {
}

bool AABB::isNull() const {
    return max.x < min.x || max.y < min.y || max.z < min.z;
}

std::optional<Vector3> AABB::rayTest(const Vector3& origin, const Vector3& dir) const {
    bool inside = true;
    Vector3 maxT(-1, -1, -1);
    Vector3 coord(0, 0, 0);

    for (int i = 0; i < 3; ++i) {
        if (origin.val[i] < this->min.val[i]) {
            coord.val[i] = this->min.val[i];
            inside = false;
            if ((uint32_t&)dir.val[i])
                maxT.val[i] = (this->min.val[i] - origin.val[i]) / dir.val[i];
        } else if (origin.val[i] > this->max.val[i]) {
            coord.val[i] = this->max.val[i];
            inside = false;
            if ((uint32_t&)dir.val[i])
                maxT.val[i] = (this->max.val[i] - origin.val[i]) / dir.val[i];
        }
    }
    if (inside) {
        return origin;
    }
    // calculate maxT to find intersection point.
    uint32_t plane = 0;
    if (maxT.y > maxT.val[plane])	plane = 1;		// plane of axis Y
    if (maxT.z > maxT.val[plane])	plane = 2;		// plane of axis Z

    //	if (maxT.val[plane] < 0)
    //		return false;
    if (((uint32_t&)maxT.val[plane]) & 0x80000000)	// negative
        return {};

    for (int i = 0; i < 3; ++i) {
        if (i != plane) {
            coord.val[i] = origin.val[i] + maxT.val[plane] * dir.val[i];

            //if(coord.val[i] < this->min.val[i] - epsilon || coord.val[i] > this->max.val[i] + epsilon)
            //    return {};

            if (coord.val[i] < this->min.val[i] || coord.val[i] > this->max.val[i])
                return {};
        }
    }
    return coord;
}

bool AABB::overlapTest(const Triangle& tri) const {
    return false;
}

bool AABB::overlapTest(const AABB& aabb) const {
    return intersection(aabb).isNull() == false;
}
