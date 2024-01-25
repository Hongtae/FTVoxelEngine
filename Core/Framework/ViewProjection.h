#pragma once
#include "../include.h"
#include "Matrix3.h"
#include "Matrix4.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Plane.h"
#include "Sphere.h"
#include "AABB.h"

namespace FV {
    struct ViewTransform {
        Matrix3 matrix;
        Vector3 t;

        Matrix3 matrix3() const {
            return matrix;
        }

        Matrix4 matrix4() const {
            return {
                matrix._11, matrix._12, matrix._13, 0.0,
                matrix._21, matrix._22, matrix._23, 0.0,
                matrix._31, matrix._32, matrix._33, 0.0,
                t.x, t.y, t.z, 1.0
            };
        }

        ViewTransform(const Matrix3& mat = Matrix3::identity,
                      const Vector3& trans = Vector3::zero)
            : matrix(mat), t(trans) {
        }
        ViewTransform(const Vector3& pos, const Vector3& dir, const Vector3& up) {
            FVASSERT_DEBUG(dir.magnitudeSquared() > 0.0f);
            FVASSERT_DEBUG(up.magnitudeSquared() > 0.0f);

            auto axisZ = -dir.normalized();
            auto axisX = Vector3::cross(up, axisZ).normalized();
            auto axisY = Vector3::cross(axisZ, axisX).normalized();

            auto tX = -Vector3::dot(axisX, pos);
            auto tY = -Vector3::dot(axisY, pos);
            auto tZ = -Vector3::dot(axisZ, pos);

            this->matrix = Matrix3(axisX.x, axisY.x, axisZ.x,
                                   axisX.y, axisY.y, axisZ.y,
                                   axisX.z, axisY.z, axisZ.z);
            this->t = Vector3(tX, tY, tZ);
        }

        Vector3 direction() const {
            return (-matrix.column3()).normalized();
        }

        Vector3 up() const {
            return matrix.column2().normalized();
        }

        Vector3 position() const {
            return (-t).applying(matrix.inverted());
        }
    };

    struct ProjectionTransform {
        Matrix4 matrix;

        static constexpr bool leftHanded = false;

        static ProjectionTransform perspectiveLH(float fov, float aspect, float nearZ, float farZ) {
            FVASSERT_DEBUG(aspect > 0.0);
            FVASSERT_DEBUG(fov > 0.0);
            FVASSERT_DEBUG(nearZ > 0.0);
            FVASSERT_DEBUG(farZ > nearZ);

            auto f = 1.0f / std::tan(fov * 0.5f);
            return {
                Matrix4(
                    f / aspect, 0, 0, 0,
                    0, f, 0, 0,
                    0, 0, farZ / (farZ - nearZ), 1,
                    0, 0, -(farZ * nearZ) / (farZ - nearZ), 0)
            };
        }
        static ProjectionTransform perspectiveRH(float fov, float aspect, float nearZ, float farZ) {
            FVASSERT_DEBUG(aspect > 0.0);
            FVASSERT_DEBUG(fov > 0.0);
            FVASSERT_DEBUG(nearZ > 0.0);
            FVASSERT_DEBUG(farZ > nearZ);

            auto f = 1.0f / std::tan(fov * 0.5f);
            return {
                Matrix4(
                    f / aspect, 0, 0, 0,
                    0, f, 0, 0,
                    0, 0, farZ / (nearZ - farZ), -1,
                    0, 0, -(farZ * nearZ) / (farZ - nearZ), 0)
            };
        }
        static ProjectionTransform perspective(float fov, float aspect, float nearZ, float farZ) {
            if constexpr (leftHanded) {
                return perspectiveLH(fov, aspect, nearZ, farZ);
            }
            return perspectiveRH(fov, aspect, nearZ, farZ);
        }

        static ProjectionTransform orthographicLH(float left, float right,
                                                  float bottom, float top,
                                                  float nearZ, float farZ) {
            return {
                Matrix4(
                    2.0f / (right - left), 0, 0, 0,
                    0, 2.0f / (top - bottom), 0, 0,
                    0, 0, 1.0f / (farZ - nearZ), 0,
                    -(right + left) / (right - left), -(top + bottom) / (top - bottom), -nearZ / (farZ - nearZ), 1)
            };
        }
        static ProjectionTransform orthographicRH(float left, float right,
                                                  float bottom, float top,
                                                  float nearZ, float farZ) {
            return {
                Matrix4(
                    2.0f / (right - left), 0, 0, 0,
                    0, 2.0f / (top - bottom), 0, 0,
                    0, 0, -1.0f / (farZ - nearZ), 0,
                    -(right + left) / (right - left), -(top + bottom) / (top - bottom), -nearZ / (farZ - nearZ), 1)
            };
        }

        static ProjectionTransform orthographic(float left, float right,
                                                float bottom, float top,
                                                float nearZ, float farZ) {
            if constexpr (leftHanded) {
                return orthographicLH(left, right, bottom, top, nearZ, farZ);
            }
            return orthographicRH(left, right, bottom, top, nearZ, farZ);
        }

        bool isPerspective() const { return matrix._44 != 1.0f; }
        bool isOrthographic() const { return matrix._44 == 1.0; }
    };

    struct ViewFrustum {
        ViewTransform view;
        ProjectionTransform projection;

        Matrix4 matrix() const { return view.matrix4().concatenating(projection.matrix); }

        Plane nearPlane;
        Plane farPlane;
        Plane leftPlane;
        Plane rightPlane;
        Plane topPlane;
        Plane bottomPlane;

        ViewFrustum(const ViewTransform& v, const ProjectionTransform& p)
            : view(v), projection(p) {
            Vector3 vec[] = {
                Vector3( 1,  1,  0),    // near right top
                Vector3( 1, -1,  0),    // near right bottom
                Vector3(-1, -1,  0),    // near left bottom
                Vector3(-1,  1,  0),    // near left top
                Vector3( 1,  1,  1),    // far right top
                Vector3( 1, -1,  1),    // far right bottom
                Vector3(-1, -1,  1),    // far left bottom
                Vector3(-1,  1,  1),    // far left top
            };
            auto mat = this->matrix().inverted();
            for (auto& v : vec) { v.apply(mat); }

            auto makePlane = [](const Vector3& v1, const Vector3& v2, const Vector3& v3)->Vector4 {
                auto n = Vector3::cross(v2 - v1, v3 - v1).normalized();
                return Vector4(n.x, n.y, n.z, -Vector3::dot(n, v1));
            };

            if constexpr (ProjectionTransform::leftHanded) {
                this->farPlane = Plane(vec[5], vec[7], vec[4]);
                this->nearPlane = Plane(vec[2], vec[0], vec[3]);
                this->topPlane = Plane(vec[0], vec[7], vec[3]);
                this->bottomPlane = Plane(vec[2], vec[5], vec[1]);
                this->leftPlane = Plane(vec[3], vec[6], vec[2]);
                this->rightPlane = Plane(vec[1], vec[4], vec[0]);
            } else {
                this->farPlane = Plane(vec[4], vec[7], vec[5]);
                this->nearPlane = Plane(vec[3], vec[0], vec[2]);
                this->topPlane = Plane(vec[3], vec[7], vec[0]);
                this->bottomPlane = Plane(vec[1], vec[5], vec[2]);
                this->leftPlane = Plane(vec[2], vec[6], vec[3]);
                this->rightPlane = Plane(vec[0], vec[4], vec[1]);
            }
        }

        bool isSphereInside(const Sphere& sp) const {
            if (sp.radius < 0.0f) return false;

            Vector4 pt = Vector4(sp.center.x, sp.center.y, sp.center.z, 1.0f);
            if (this->nearPlane.dot(pt) < -sp.radius)   return false;
            if (this->farPlane.dot(pt) < -sp.radius)    return false;
            if (this->leftPlane.dot(pt) < -sp.radius)   return false;
            if (this->rightPlane.dot(pt) < -sp.radius)  return false;
            if (this->topPlane.dot(pt) < -sp.radius)    return false;
            if (this->bottomPlane.dot(pt) < -sp.radius) return false;
            return true;
        }

        bool isPointInside(const Vector3& point) const {
            return isSphereInside({ point, 0.0f });
        }

        bool isAABBInside(const AABB& aabb) const {
            if (aabb.isNull())
                return false;

            const Plane* planes[6] = {
                &nearPlane,
                &farPlane,
                &leftPlane,
                &rightPlane,
                &topPlane,
                &bottomPlane,
            };

            const Vector3* minMax[2] = { &aabb.min, &aabb.max };

            for (int i = 0; i < 6; ++i) {
                auto plane = planes[i];
                int bx = (plane->a > 0.0f) ? 1 : 0;
                int by = (plane->b > 0.0f) ? 1 : 0;
                int bz = (plane->c > 0.0f) ? 1 : 0;

                auto d = plane->dot(Vector3(minMax[bx]->x, minMax[by]->y, minMax[bz]->z));
                if (d < 0.0f)
                    return false;

                bx = 1 - bx;
                by = 1 - by;
                bz = 1 - bz;
                d = plane->dot(Vector3(minMax[bx]->x, minMax[by]->z, minMax[bz]->z));
                if (d <= 0.0f)
                    return true; // intersects
            }
            // inside
            return true;
        }
    };
}
