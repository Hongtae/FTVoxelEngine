#include <algorithm>
#include <limits>
#include <tuple>
#include "AABB.h"
#include "Triangle.h"
#include "Plane.h"

using namespace FV;

constexpr auto epsilon = std::numeric_limits<float>::epsilon();
constexpr auto fltmax = std::numeric_limits<float>::max();

const AABB AABB::null = AABB(Vector3(fltmax, fltmax, fltmax), Vector3(-fltmax, -fltmax, -fltmax));

AABB::AABB()
    : min(null.min)
    , max(null.max) {
}

AABB::AABB(const Vector3& _min, const Vector3& _max)
    : min(_min), max(_max) {
}

std::optional<Vector3> AABB::rayTest(const Vector3& origin, const Vector3& dir) const {
    if (isNull()) return {};

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
    //		return {};
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

bool AABB::overlapTest(const Plane& plane) const {
    if (isNull()) return false;

    Vector3 vmin, vmax;
    for (int n = 0; n < 3; n++) {
        if (plane.val[n] > 0.0f) {
            vmin.val[n] = this->min.val[n];
            vmax.val[n] = this->max.val[n];
        } else {
            vmin.val[n] = this->max.val[n];
            vmax.val[n] = this->min.val[n];
        }
    }
    if (plane.dot(vmax) < 0.0f) return false; // box is below plane
    if (plane.dot(vmin) > 0.0f) return false; // box is above plane
    return true;
}

bool AABB::overlapTest(const Triangle& tri) const {
    // algorithm based on Tomas Möller
    // https://cs.lth.se/tomas-akenine-moller/

    if (isNull()) return false;

    /*    use separating axis theorem to test overlap between triangle and box */
    /*    need to test for overlap in these directions: */
    /*    1) the {x,y,z}-directions (actually, since we use the AABB of the triangle */
    /*       we do not even need to test these) */
    /*    2) normal of the triangle */
    /*    3) crossproduct(edge from tri, {x,y,z}-directin) */
    /*       this gives 3x3=9 more tests */

    const Vector3 boxcenter = this->center();
    const Vector3 boxhalfsize = this->extents() * 0.5f;

    auto axisTest = [&boxhalfsize](float a, float b, float fa, float fb,
                                   const Vector3& v1, const Vector3& v2,
                                   int idx1, int idx2) -> bool {
        float p1 = a * v1.val[idx1] + b * v1.val[idx2];
        float p2 = a * v2.val[idx1] + b * v2.val[idx2];
        float min, max;
        if (p1 > p2) { min = p2; max = p1; } else { min = p1; max = p2; }
        float rad = fa * boxhalfsize.val[idx1] + fb * boxhalfsize.val[idx2];
        if (min > rad || max < -rad)
            return false;
        return true;
    };

    /* move everything so that the boxcenter is in (0,0,0) */
    const Vector3 v0 = tri.p0 - boxcenter;
    const Vector3 v1 = tri.p1 - boxcenter;
    const Vector3 v2 = tri.p2 - boxcenter;

    enum { _X = 0, _Y = 1, _Z = 2 };
    auto axisTest_X01 = [&](float a, float b, float fa, float fb) -> bool {
        return axisTest(a, -b, fa, fb, v0, v2, _Y, _Z);
    };
    auto axisTest_X2 = [&](float a, float b, float fa, float fb) -> bool {
        return axisTest(a, -b, fa, fb, v0, v1, _Y, _Z);
    };
    auto axisTest_Y02 = [&](float a, float b, float fa, float fb) -> bool {
        return axisTest(-a, b, fa, fb, v0, v2, _X, _Z);
    };
    auto axisTest_Y1 = [&](float a, float b, float fa, float fb) -> bool {
        return axisTest(-a, b, fa, fb, v0, v1, _X, _Z);
    };
    auto axisTest_Z12 = [&](float a, float b, float fa, float fb) -> bool {
        return axisTest(a, -b, fa, fb, v1, v2, _X, _Y);
    };
    auto axisTest_Z0 = [&](float a, float b, float fa, float fb) -> bool {
        return axisTest(a, -b, fa, fb, v0, v1, _X, _Y);
    };

    /* compute triangle edges */
    const Vector3 e0 = v1 - v0;      /* tri edge 0 */
    const Vector3 e1 = v2 - v1;      /* tri edge 1 */
    const Vector3 e2 = v0 - v2;      /* tri edge 2 */

    // float axis[3];
    float fex, fey, fez;		// -NJMP- "d" local variable removed

    /* Bullet 3:  */
    /*  test the 9 tests first (this was faster) */

    fex = fabs(e0.x);
    fey = fabs(e0.y);
    fez = fabs(e0.z);
    if (!axisTest_X01(e0.z, e0.y, fez, fey)) return false;
    if (!axisTest_Y02(e0.z, e0.x, fez, fex)) return false;
    if (!axisTest_Z12(e0.y, e0.x, fey, fex)) return false;

    fex = fabs(e1.x);
    fey = fabs(e1.y);
    fez = fabs(e1.z);
    if (!axisTest_X01(e1.z, e1.y, fez, fey)) return false;
    if (!axisTest_Y02(e1.z, e1.x, fez, fex)) return false;
    if (!axisTest_Z0(e1.y, e1.x, fey, fex)) return false;

    fex = fabs(e2.x);
    fey = fabs(e2.y);
    fez = fabs(e2.z);
    if (!axisTest_X2(e2.z, e2.y, fez, fey)) return false;
    if (!axisTest_Y1(e2.z, e2.x, fez, fex)) return false;
    if (!axisTest_Z12(e2.y, e2.x, fey, fex)) return false;

    /* Bullet 1: */
    /*  first test overlap in the {x,y,z}-directions */
    /*  find min, max of the triangle each direction, and test for overlap in */
    /*  that direction -- this is equivalent to testing a minimal AABB around */
    /*  the triangle against the AABB */

    constexpr auto findMinMax = [](float x0, float x1, float x2) -> std::tuple<float, float> {
        return { std::min({ x0, x1, x2 }), std::max({ x0, x1, x2 }) };
    };

    float min, max;

    /* test in X-direction */
    std::tie(min, max) = findMinMax(v0.x, v1.x, v2.x);
    if (min > boxhalfsize.x || max < -boxhalfsize.x) return false;

    /* test in Y-direction */
    std::tie(min, max) = findMinMax(v0.y, v1.y, v2.y);
    if (min > boxhalfsize.y || max < -boxhalfsize.y) return false;

    /* test in Z-direction */
    std::tie(min, max) = findMinMax(v0.z, v1.z, v2.z);
    if (min > boxhalfsize.z || max < -boxhalfsize.z) return false;

    /* Bullet 2: */
    /*  test if the box intersects the plane of the triangle */
    /*  compute plane equation of triangle: normal*x+d=0 */

    auto planeBoxOverlap = [](const Vector3& normal, const Vector3& vert, const Vector3& maxbox) -> bool {
        Vector3 vmin, vmax;
        for (int q = 0; q < 3; q++) {
            float v = vert.val[q];
            if (normal.val[q] > 0.0f) {
                vmin.val[q] = -maxbox.val[q] - v;
                vmax.val[q] = maxbox.val[q] - v;
            } else {
                vmin.val[q] = maxbox.val[q] - v;
                vmax.val[q] = -maxbox.val[q] - v;
            }
        }
        if (Vector3::dot(normal, vmin) > 0.0f) return false;
        if (Vector3::dot(normal, vmax) >= 0.0f) return true;
        return false;
    };
    Vector3 normal = Vector3::cross(e0, e1);
    if (!planeBoxOverlap(normal, v0, boxhalfsize)) return false;

    /* box and triangle overlaps */
    return true;
}

bool AABB::overlapTest(const AABB& aabb) const {
    return intersection(aabb).isNull() == false;
}
