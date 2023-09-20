#include <algorithm>
#include <limits>
#include "Triangle.h"
#include "AABB.h"

using namespace FV;

constexpr auto epsilon = std::numeric_limits<float>::epsilon();
constexpr auto fltmax = std::numeric_limits<float>::max();
constexpr bool epsilon_test = false;

namespace {
    // algorithm based on Tomas Möller
    // https://cs.lth.se/tomas-akenine-moller/
    inline bool _edge_edge_test(float ax, float ay,
                                uint8_t i0, uint8_t i1,
                                const Vector3& v0, const Vector3& u0, const Vector3& u1) {
        float bx = u0.val[i0] - u1.val[i0];
        float by = u0.val[i1] - u1.val[i1];
        float cx = v0.val[i0] - u0.val[i0];
        float cy = v0.val[i1] - u0.val[i1];

        float f = ay * bx - ax * by;
        float d = by * cx - bx * cy;

        if ((f > 0.0f && d >= 0.0f) || (f < 0.0f && d <= 0.0f && d >= f)) {
            float e = ax * cy - ay * cx;
            if (f > 0.0f) {
                if (e >= 0.0f && e <= f) return true;
            } else {
                if (e <= 0.0f && e >= f) return true;
            }
        }
        return false;
    }
    inline bool _edge_against_tri_edges(uint8_t i0, uint8_t i1,
                                        const Vector3& v0, const Vector3& v1,
                                        const Vector3& u0, const Vector3& u1, const Vector3& u2) {
        float ax = v1.val[i0] - v0.val[i0];
        float ay = v1.val[i1] - v0.val[i1];
        if (_edge_edge_test(ax, ay, i0, i1, v0, u0, u1))   return true;
        if (_edge_edge_test(ax, ay, i0, i1, v0, u1, u2))   return true;
        if (_edge_edge_test(ax, ay, i0, i1, v0, u2, u0))   return true;
        return false;
    }
    inline bool _point_in_tri(uint8_t i0, uint8_t i1,
                              const Vector3& v0, const Vector3& u0, const Vector3& u1, const Vector3& u2) {
        float a, b, c, d0, d1, d2;
        /* is T1 completly inside T2? */
        /* check if V0 is inside tri(U0,U1,U2) */
        a = u1.val[i1] - u0.val[i1];
        b = -(u1.val[i0] - u0.val[i0]);
        c = -a * u0.val[i0] - b * u0.val[i1];
        d0 = a * v0.val[i0] + b * v0.val[i1] + c;

        a = u2.val[i1] - u1.val[i1];
        b = -(u2.val[i0] - u1.val[i0]);
        c = -a * u1.val[i0] - b * u1.val[i1];
        d1 = a * v0.val[i0] + b * v0.val[i1] + c;

        a = u0.val[i1] - u2.val[i1];
        b = -(u0.val[i0] - u2.val[i0]);
        c = -a * u2.val[i0] - b * u2.val[i1];
        d2 = a * v0.val[i0] + b * v0.val[i1] + c;

        if (d0 * d1 > 0.0f) {
            if (d0 * d2 > 0.0f) return true;
        }
        return false;
    }
    inline bool _coplanar_tri_tri(const Vector3& n,
                                  const Vector3& v0, const Vector3& v1, const Vector3& v2,
                                  const Vector3& u0, const Vector3& u1, const Vector3& u2) {
        Vector3 a;
        uint8_t i0, i1;
        /* first project onto an axis-aligned plane, that maximizes the area */
        /* of the triangles, compute indices: i0,i1. */
        a.x = fabs(n.x);
        a.y = fabs(n.y);
        a.z = fabs(n.z);
        if (a.x > a.y) {
            if (a.x > a.z) {
                i0 = 1;     /* a.x is greatest */
                i1 = 2;
            } else {
                i0 = 0;     /* a.z is greatest */
                i1 = 1;
            }
        } else {            /* a.x <= a.y */
            if (a.z > a.y) {
                i0 = 0;      /* a.z is greatest */
                i1 = 1;
            } else {
                i0 = 0;      /* a.y is greatest */
                i1 = 2;
            }
        }
        /* test all edges of triangle 1 against the edges of triangle 2 */
        if (_edge_against_tri_edges(i0, i1, v0, v1, u0, u1, u2)) return true;
        if (_edge_against_tri_edges(i0, i1, v1, v2, u0, u1, u2)) return true;
        if (_edge_against_tri_edges(i0, i1, v2, v0, u0, u1, u2)) return true;

        /* finally, test if tri1 is totally contained in tri2 or vice versa */
        if (_point_in_tri(i0, i1, v0, u0, u1, u2)) return true;
        if (_point_in_tri(i0, i1, u0, v0, v1, v2)) return true;

        return false;
    }
    template <typename T> inline void _sort(T& a, T& b) { if (a > b) std::swap(a, b); }
    
    inline bool _compute_intervals(float vv0, float vv1, float vv2,
                                   float d0, float d1, float d2,
                                   float d0d1, float d0d2,
                                   float& isect0, float& isect1) {
        auto isect = [&](float vv0, float vv1, float vv2, float d0, float d1, float d2) {
            isect0 = vv0 + (vv1 - vv0) * d0 / (d0 - d1);
            isect1 = vv0 + (vv2 - vv0) * d0 / (d0 - d2);
        };

        if (d0d1 > 0.0f) {
            /* here we know that d0d2<=0.0 */
            /* that is d0, d1 are on the same side, d2 on the other or on the plane */
            isect(vv2, vv0, vv1, d2, d0, d1);
        } else if (d0d2 > 0.0f) {
            /* here we know that d0d1<=0.0 */
            isect(vv1, vv0, vv2, d1, d0, d2);
        } else if (d1 * d2 > 0.0f || d0 != 0.0f) {
            /* here we know that d0d1<=0.0 or that d0!=0.0 */
            isect(vv0, vv1, vv2, d0, d1, d2);
        } else if (d1 != 0.0f) {
            isect(vv1, vv0, vv2, d1, d0, d2);
        } else if (d2 != 0.0f) {
            isect(vv2, vv0, vv1, d2, d0, d1);
        } else {
            return true;    /* triangles are coplanar */
        }
        return false;
    }
    static bool _tri_tri_intersect(const Vector3& v0, const Vector3& v1, const Vector3& v2,
                                   const Vector3& u0, const Vector3& u1, const Vector3& u2) {
        /* compute plane equation of triangle(V0,V1,V2) */
        Vector3 e1 = v1 - v0;
        Vector3 e2 = v2 - v0;
        Vector3 n1 = Vector3::cross(e1, e2);
        float d1 = -Vector3::dot(n1, v0);
        /* plane equation 1: N1.X+d1=0 */

        /* put U0,U1,U2 into plane equation 1 to compute signed distances to the plane*/
        float du0 = Vector3::dot(n1, u0) + d1;
        float du1 = Vector3::dot(n1, u1) + d1;
        float du2 = Vector3::dot(n1, u2) + d1;

        /* coplanarity robustness check */
        if constexpr (epsilon_test) {
            if (fabs(du0) < epsilon) du0 = 0.0f;
            if (fabs(du1) < epsilon) du1 = 0.0f;
            if (fabs(du2) < epsilon) du2 = 0.0f;
        }
        float du0du1 = du0 * du1;
        float du0du2 = du0 * du2;

        if (du0du1 > 0.0f && du0du2 > 0.0f) /* same sign on all of them + not equal 0 ? */
            return false;                   /* no intersection occurs */

        /* compute plane of triangle (U0,U1,U2) */
        e1 = u1 - u0;
        e2 = u2 - u0;
        Vector3 n2 = Vector3::cross(e1, e2);
        float d2 = -Vector3::dot(n2, u0);
        /* plane equation 2: N2.X+d2=0 */

        /* put V0,V1,V2 into plane equation 2 */
        float dv0 = Vector3::dot(n2, v0) + d2;
        float dv1 = Vector3::dot(n2, v1) + d2;
        float dv2 = Vector3::dot(n2, v2) + d2;

        if constexpr (epsilon_test) {
            if (fabs(dv0) < epsilon) dv0 = 0.0;
            if (fabs(dv1) < epsilon) dv1 = 0.0;
            if (fabs(dv2) < epsilon) dv2 = 0.0;
        }
        float dv0dv1 = dv0 * dv1;
        float dv0dv2 = dv0 * dv2;

        if (dv0dv1 > 0.0f && dv0dv2 > 0.0f) /* same sign on all of them + not equal 0 ? */
            return false;                   /* no intersection occurs */

        uint8_t index = 0;

        /* compute direction of intersection line */
        Vector3 d = Vector3::cross(n1, n2);

        /* compute and index to the largest component of D */
        float max = fabs(d.x);
        float b = fabs(d.y);
        float c = fabs(d.z);
        if (b > max) { max = b; index = 1; }
        if (c > max) { max = c; index = 2; }

        /* this is the simplified projection onto L*/
        float vp0 = v0.val[index];
        float vp1 = v1.val[index];
        float vp2 = v2.val[index];
                      
        float up0 = u0.val[index];
        float up1 = u1.val[index];
        float up2 = u2.val[index];

        float isect1[2], isect2[2];
        /* compute interval for triangle 1 */
        if (_compute_intervals(vp0, vp1, vp2, dv0, dv1, dv2, dv0dv1, dv0dv2, isect1[0], isect1[1])) {
            return _coplanar_tri_tri(n1, v0, v1, v2, u0, u1, u2);
        }

        /* compute interval for triangle 2 */
        if (_compute_intervals(up0, up1, up2, du0, du1, du2, du0du1, du0du2, isect2[0], isect2[1])) {
            return _coplanar_tri_tri(n1, v0, v1, v2, u0, u1, u2);
        }

        _sort(isect1[0], isect1[1]);
        _sort(isect2[0], isect2[1]);

        if (isect1[1] < isect2[0] || isect2[1] < isect1[0]) return false;
        return true;
    }
    inline bool _compute_intervals2(float vv0, float vv1, float vv2,
                                    float d0, float d1, float d2,
                                    float d0d1, float d0d2,
                                    float& a, float& b, float& c, float& x0, float& x1) {
        if (d0d1 > 0.0f) {
            /* here we know that d0d2<=0.0 */
            /* that is d0, d1 are on the same side, d2 on the other or on the plane */
            a = vv2; b = (vv0 - vv2) * d2; c = (vv1 - vv2) * d2; x0 = d2 - d0; x1 = d2 - d1;
        } else if (d0d2 > 0.0f) {
            /* here we know that d0d1<=0.0 */
            a = vv1; b = (vv0 - vv1) * d1; c = (vv2 - vv1) * d1; x0 = d1 - d0; x1 = d1 - d2;
        } else if (d1 * d2 > 0.0f || d0 != 0.0f) {
            /* here we know that d0d1<=0.0 or that d0!=0.0 */
            a = vv0; b = (vv1 - vv0) * d0; c = (vv2 - vv0) * d0; x0 = d0 - d1; x1 = d0 - d2;
        } else if (d1 != 0.0f) {
            a = vv1; b = (vv0 - vv1) * d1; c = (vv2 - vv1) * d1; x0 = d1 - d0; x1 = d1 - d2;
        } else if (d2 != 0.0f) {
            a = vv2; b = (vv0 - vv2) * d2; c = (vv1 - vv2) * d2; x0 = d2 - d0; x1 = d2 - d1;
        } else {
            return true;        /* triangles are coplanar */
        }
        return false;
    }
    static bool _tri_tri_intersect_no_div(const Vector3& v0, const Vector3& v1, const Vector3& v2,
                                          const Vector3& u0, const Vector3& u1, const Vector3& u2) {
        /* compute plane equation of triangle(V0,V1,V2) */
        Vector3 e1 = v1 - v0;
        Vector3 e2 = v2 - v0;
        Vector3 n1 = Vector3::cross(e1, e2);
        float d1 = -Vector3::dot(n1, v0);
        /* plane equation 1: N1.X+d1=0 */

        /* put U0,U1,U2 into plane equation 1 to compute signed distances to the plane*/
        float du0 = Vector3::dot(n1, u0) + d1;
        float du1 = Vector3::dot(n1, u1) + d1;
        float du2 = Vector3::dot(n1, u2) + d1;

        /* coplanarity robustness check */
        if constexpr (epsilon_test) {
            if (fabs(du0) < epsilon) du0 = 0.0f;
            if (fabs(du1) < epsilon) du1 = 0.0f;
            if (fabs(du2) < epsilon) du2 = 0.0f;
        }
        float du0du1 = du0 * du1;
        float du0du2 = du0 * du2;

        if (du0du1 > 0.0f && du0du2 > 0.0f) /* same sign on all of them + not equal 0 ? */
            return false;                   /* no intersection occurs */

        /* compute plane of triangle (U0,U1,U2) */
        e1 = u1 - u0;
        e2 = u2 - u0;
        Vector3 n2 = Vector3::cross(e1, e2);
        float d2 = -Vector3::dot(n2, u0);
        /* plane equation 2: N2.X+d2=0 */

        /* put V0,V1,V2 into plane equation 2 */
        float dv0 = Vector3::dot(n2, v0) + d2;
        float dv1 = Vector3::dot(n2, v1) + d2;
        float dv2 = Vector3::dot(n2, v2) + d2;

        if constexpr (epsilon_test) {
            if (fabs(dv0) < epsilon) dv0 = 0.0f;
            if (fabs(dv1) < epsilon) dv1 = 0.0f;
            if (fabs(dv2) < epsilon) dv2 = 0.0f;
        }
        float dv0dv1 = dv0 * dv1;
        float dv0dv2 = dv0 * dv2;

        if (dv0dv1 > 0.0f && dv0dv2 > 0.0f) /* same sign on all of them + not equal 0 ? */
            return false;                   /* no intersection occurs */

        uint8_t index = 0;
        {
            /* compute direction of intersection line */
            Vector3 d = Vector3::cross(n1, n2);

            /* compute and index to the largest component of D */
            float max = fabs(d.x);
            float bb = fabs(d.y);
            float cc = fabs(d.z);
            if (bb > max) { max = bb; index = 1; }
            if (cc > max) { max = cc; index = 2; }
        }

        /* this is the simplified projection onto L*/
        float vp0 = v0.val[index];
        float vp1 = v1.val[index];
        float vp2 = v2.val[index];

        float up0 = u0.val[index];
        float up1 = u1.val[index];
        float up2 = u2.val[index];

        /* compute interval for triangle 1 */
        float a, b, c, x0, x1;
        if (_compute_intervals2(vp0, vp1, vp2, dv0, dv1, dv2, dv0dv1, dv0dv2, a, b, c, x0, x1)) {
            return _coplanar_tri_tri(n1, v0, v1, v2, u0, u1, u2);
        }

        /* compute interval for triangle 2 */
        float d, e, f, y0, y1;
        if (_compute_intervals2(up0, up1, up2, du0, du1, du2, du0du1, du0du2, d, e, f, y0, y1)) {
            return _coplanar_tri_tri(n1, v0, v1, v2, u0, u1, u2);
        }

        float xx = x0 * x1;
        float yy = y0 * y1;
        float xxyy = xx * yy;

        float tmp = a * xxyy;
        float isect1[2], isect2[2];
        isect1[0] = tmp + b * x1 * yy;
        isect1[1] = tmp + c * x0 * yy;

        tmp = d * xxyy;
        isect2[0] = tmp + e * xx * y1;
        isect2[1] = tmp + f * xx * y0;

        _sort(isect1[0], isect1[1]);
        _sort(isect2[0], isect2[1]);

        if (isect1[1] < isect2[0] || isect2[1] < isect1[0]) return false;
        return true;
    }

    inline bool _compute_intervals_isectline(const Vector3& vert0, const Vector3& vert1, const Vector3& vert2,
                                            float vv0, float vv1, float vv2, float d0, float d1, float d2,
                                            float d0d1, float d0d2, float* isect0, float* isect1,
                                            Vector3& isectpoint0, Vector3& isectpoint1) {
        auto isect2 = [](const Vector3& vtx0, const Vector3& vtx1, const Vector3& vtx2,
                         float vv0, float vv1, float vv2,
                         float d0, float d1, float d2,
                         float* isect0, float* isect1,
                         Vector3& isectpoint0, Vector3& isectpoint1) {
            float tmp = d0 / (d0 - d1);
            *isect0 = vv0 + (vv1 - vv0) * tmp;
            auto diff = (vtx1 - vtx0) * tmp;
            isectpoint0 = diff + vtx0;
            tmp = d0 / (d0 - d2);
            *isect1 = vv0 + (vv2 - vv0) * tmp;
            diff = (vtx2 - vtx0) * tmp;
            isectpoint1 = vtx0 + diff;
        };

        if (d0d1 > 0.0f) {
            /* here we know that d0d2<=0.0 */
            /* that is d0, d1 are on the same side, d2 on the other or on the plane */
            isect2(vert2, vert0, vert1, vv2, vv0, vv1, d2, d0, d1, isect0, isect1, isectpoint0, isectpoint1);
        } else if (d0d2 > 0.0f) {
            /* here we know that d0d1<=0.0 */
            isect2(vert1, vert0, vert2, vv1, vv0, vv2, d1, d0, d2, isect0, isect1, isectpoint0, isectpoint1);
        } else if (d1 * d2 > 0.0f || d0 != 0.0f) {
            /* here we know that d0d1<=0.0 or that d0!=0.0 */
            isect2(vert0, vert1, vert2, vv0, vv1, vv2, d0, d1, d2, isect0, isect1, isectpoint0, isectpoint1);
        } else if (d1 != 0.0f) {
            isect2(vert1, vert0, vert2, vv1, vv0, vv2, d1, d0, d2, isect0, isect1, isectpoint0, isectpoint1);
        } else if (d2 != 0.0f) {
            isect2(vert2, vert0, vert1, vv2, vv0, vv1, d2, d0, d1, isect0, isect1, isectpoint0, isectpoint1);
        } else {
            return true;    /* triangles are coplanar */
        }
        return false;
    }
    template <typename T, typename U> inline void _sort2(T& a, T& b, U& smallest) {
        if (a > b) {
            std::swap(a, b);
            smallest = 1;
        } else
            smallest = 0;
    }
    static bool _tri_tri_intersect_with_isectline(const Vector3& v0, const Vector3& v1, const Vector3& v2,
                                                  const Vector3& u0, const Vector3& u1, const Vector3& u2,
                                                  bool& coplanar,
                                                  Vector3& isectpt1, Vector3& isectpt2) {
        /* compute plane equation of triangle(V0,V1,V2) */
        Vector3 e1 = v1 - v0;
        Vector3 e2 = v2 - v0;
        Vector3 n1 = Vector3::cross(e1, e2);
        float d1 = -Vector3::dot(n1, v0);
        /* plane equation 1: N1.X+d1=0 */

        /* put U0,U1,U2 into plane equation 1 to compute signed distances to the plane*/
        float du0 = Vector3::dot(n1, u0) + d1;
        float du1 = Vector3::dot(n1, u1) + d1;
        float du2 = Vector3::dot(n1, u2) + d1;

        /* coplanarity robustness check */
        if constexpr (epsilon_test) {
            if (fabs(du0) < epsilon) du0 = 0.0f;
            if (fabs(du1) < epsilon) du1 = 0.0f;
            if (fabs(du2) < epsilon) du2 = 0.0f;
        }
        float du0du1 = du0 * du1;
        float du0du2 = du0 * du2;

        if (du0du1 > 0.0f && du0du2 > 0.0f) /* same sign on all of them + not equal 0 ? */
            return false;                   /* no intersection occurs */

        /* compute plane of triangle (U0,U1,U2) */
        e1 = u1 - u0;
        e2 = u2 - u0;
        Vector3 n2 = Vector3::cross(e1, e2);
        float d2 = -Vector3::dot(n2, u0);
        /* plane equation 2: N2.X+d2=0 */

        /* put V0,V1,V2 into plane equation 2 */
        float dv0 = Vector3::dot(n2, v0) + d2;
        float dv1 = Vector3::dot(n2, v1) + d2;
        float dv2 = Vector3::dot(n2, v2) + d2;

        if constexpr (epsilon_test) {
            if (fabs(dv0) < epsilon) dv0 = 0.0;
            if (fabs(dv1) < epsilon) dv1 = 0.0;
            if (fabs(dv2) < epsilon) dv2 = 0.0;
        }
        float dv0dv1 = dv0 * dv1;
        float dv0dv2 = dv0 * dv2;

        if (dv0dv1 > 0.0f && dv0dv2 > 0.0f) /* same sign on all of them + not equal 0 ? */
            return false;                   /* no intersection occurs */

        uint8_t index = 0;
        
        /* compute direction of intersection line */
        Vector3 d = Vector3::cross(n1, n2);

        /* compute and index to the largest component of D */
        float max = fabs(d.x);
        float b = fabs(d.y);
        float c = fabs(d.z);
        if (b > max) { max = b; index = 1; }
        if (c > max) { max = c; index = 2; }
        
        /* this is the simplified projection onto L*/
        float vp0 = v0.val[index];
        float vp1 = v1.val[index];
        float vp2 = v2.val[index];

        float up0 = u0.val[index];
        float up1 = u1.val[index];
        float up2 = u2.val[index];

        /* compute interval for triangle 1 */
        float isect1[2], isect2[2];
        Vector3 isectpointA1, isectpointA2;
        Vector3 isectpointB1, isectpointB2;
        coplanar = _compute_intervals_isectline(v0, v1, v2, vp0, vp1, vp2, dv0, dv1, dv2,
                                                dv0dv1, dv0dv2, &isect1[0], &isect1[1], isectpointA1, isectpointA2);
        if (coplanar)
            return _coplanar_tri_tri(n1, v0, v1, v2, u0, u1, u2);

        /* compute interval for triangle 2 */
        _compute_intervals_isectline(u0, u1, u2, up0, up1, up2, du0, du1, du2,
                                     du0du1, du0du2, &isect2[0], &isect2[1], isectpointB1, isectpointB2);

        uint8_t smallest1, smallest2;
        _sort2(isect1[0], isect1[1], smallest1);
        _sort2(isect2[0], isect2[1], smallest2);

        if (isect1[1] < isect2[0] || isect2[1] < isect1[0]) return false;

        /* at this point, we know that the triangles intersect */

        if (isect2[0] < isect1[0]) {
            if (smallest1 == 0)     { isectpt1 = isectpointA1; }
            else                    { isectpt1 = isectpointA2; }

            if (isect2[1] < isect1[1]) {
                if (smallest2 == 0) { isectpt2 = isectpointB2; }
                else                { isectpt2 = isectpointB1; }
            } else {
                if (smallest1 == 0) { isectpt2 = isectpointA2; }
                else                { isectpt2 = isectpointA1; }
            }
        } else {
            if (smallest2 == 0)     { isectpt1 = isectpointB1; }
            else                    { isectpt1 = isectpointB2; }

            if (isect2[1] > isect1[1]) {
                if (smallest1 == 0) { isectpt2 = isectpointA2; }
                else                { isectpt2 = isectpointA1; }
            } else {
                if (smallest2 == 0) { isectpt2 = isectpointB2; }
                else                { isectpt2 = isectpointB1; }
            }
        }
        return true;
    }
}

float Triangle::area() const {
    auto ab = p1 - p0;
    auto ac = p2 - p0;
    return Vector3::cross(ab, ac).magnitude() * 0.5f;
}

AABB Triangle::aabb() const {
    AABB aabb;
    aabb.min.x = std::min({ p0.x, p1.x, p2.x });
    aabb.min.y = std::min({ p0.y, p1.y, p2.y });
    aabb.min.z = std::min({ p0.z, p1.z, p2.z });
    aabb.max.x = std::max({ p0.x, p1.x, p2.x });
    aabb.max.y = std::max({ p0.y, p1.y, p2.y });
    aabb.max.z = std::max({ p0.z, p1.z, p2.z });
    return aabb;
}

Vector3 Triangle::barycentric(const Vector3& p) const {
    Vector3 v0 = p1 - p0;
    Vector3 v1 = p2 - p0;
    Vector3 v2 = p - p0;
    float d00 = Vector3::dot(v0, v0);
    float d01 = Vector3::dot(v0, v1);
    float d11 = Vector3::dot(v1, v1);
    float d20 = Vector3::dot(v2, v0);
    float d21 = Vector3::dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    float invDenom = 1.0f / denom;
    float v = (d11 * d20 - d01 * d21) * invDenom;
    float w = (d00 * d21 - d01 * d20) * invDenom;
    float u = 1.0f - v - w;
    return Vector3(u, v, w);
}

std::optional<Triangle::RayTestResult> Triangle::rayTest(const Vector3& origin, const Vector3& dir) const {
    Vector3 edge1 = p1 - p0;
    Vector3 edge2 = p2 - p0;
    Vector3 p = Vector3::cross(dir, edge2);
    float det = Vector3::dot(edge1, p);

    if (det > -epsilon && det < epsilon)
        return {};

    float invDet = 1.0f / det;

    Vector3 s = origin - p0;
    float u = Vector3::dot(s, p) * invDet;
    if (u < 0.0f || u > 1.0f)
        return {};

    Vector3 q = Vector3::cross(s, edge1);
    float v = Vector3::dot(dir, q) * invDet;
    if (v < 0.0f || u + v > 1.0)
        return {};

    RayTestResult result = {};
    result.t = Vector3::dot(edge2, q) * invDet;
    result.u = u;
    result.v = v;
    return result;
}

std::optional<Triangle::RayTestResult> Triangle::rayTestCW(const Vector3& origin, const Vector3& dir) const {
    return Triangle{ p2, p1, p0 }.rayTestCCW(origin, dir);
}

std::optional<Triangle::RayTestResult> Triangle::rayTestCCW(const Vector3& origin, const Vector3& dir) const {
    Vector3 edge1 = p1 - p0;
    Vector3 edge2 = p2 - p0;
    Vector3 p = Vector3::cross(dir, edge2);
    float det = Vector3::dot(edge1, p);

    if (det > -epsilon && det < epsilon)
        return {};

    Vector3 s = origin - p0;
    float u = Vector3::dot(s, p);
    if (u < 0.0f || u > det)
        return {};

    Vector3 q = Vector3::cross(s, edge1);
    float v = Vector3::dot(dir, q);
    if (v < 0.0f || u + v > det)
        return {};

    float invDet = 1.0f / det;
    RayTestResult result = {};
    result.t = Vector3::dot(edge2, q) * invDet;
    result.u = u * invDet;
    result.v = v * invDet;
    return result;
}

std::optional<Triangle::LineSegment> Triangle::intersectionTest(const Triangle& other) const {
    LineSegment line = {};
    bool coplanar = false;
    if (_tri_tri_intersect_with_isectline(p0, p1, p2,
                                          other.p0, other.p1, other.p2,
                                          coplanar,
                                          line.p0, line.p1)) {
        return line;
    }
    return {};
}

bool Triangle::intersects(const Triangle& other) const {
    return _tri_tri_intersect_no_div(p0, p1, p2,
                                     other.p0, other.p1, other.p2);
}
