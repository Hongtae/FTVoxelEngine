#include <limits>
#include "Plane.h"

using namespace FV;

constexpr auto epsilon = std::numeric_limits<float>::epsilon();

float Plane::rayTest(const Vector3& origin, const Vector3& dir) const {
    auto distance = dot(origin);
    if (distance == 0) { return 0.0f; }

    auto d = dir.normalized();
    auto denom = Vector3::dot(this->normal(), d);

    if (abs(denom) > epsilon) {
        float t = -distance / denom;
        if (t >= 0.0)
            return t;
    }
    return -1.0f;
}
