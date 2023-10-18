#pragma once
#include "../include.h"
#include "Matrix3.h"
#include "Matrix4.h"
#include "Vector3.h"

namespace FV {
    struct ViewTransform {
        Matrix3 matrix;
        Vector3 position;

        Matrix3 matrix3() const {
            return matrix;
        }

        Matrix4 matrix4() const {
            return {
                matrix._11, matrix._12, matrix._13, 0.0,
                matrix._21, matrix._22, matrix._23, 0.0,
                matrix._31, matrix._32, matrix._33, 0.0,
                position.x, position.y, position.z, 1.0
            };
        }

        ViewTransform(const Matrix3& mat = Matrix3::identity,
                      const Vector3& pos = Vector3::zero)
            : matrix(mat), position(pos) {
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
            this->position = Vector3(tX, tY, tZ);
        }

        Vector3 direction() const {
            return (-matrix.column3()).normalized();
        }

        Vector3 up() const {
            return matrix.column2().normalized();
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

        Vector4 nearPlane;
        Vector4 farPlane;
        Vector4 leftPlane;
        Vector4 rightPlane;
        Vector4 topPlane;
        Vector4 bottomPlane;

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
            for (auto& v : vec) {
                auto v2 = Vector4(v.x, v.y, v.z, 1.0f).applying(mat);
                v = Vector3(v2.x, v2.y, v2.z) / v2.w;
            }

            auto makePlane = [](const Vector3& v1, const Vector3& v2, const Vector3& v3)->Vector4 {
                auto n = Vector3::cross(v2 - v1, v3 - v1).normalized();
                return Vector4(n.x, n.y, n.z, -Vector3::dot(n, v1));
            };

            if constexpr (ProjectionTransform::leftHanded) {
                this->farPlane = makePlane(vec[5], vec[7], vec[4]);
                this->nearPlane = makePlane(vec[2], vec[0], vec[3]);
                this->topPlane = makePlane(vec[0], vec[7], vec[3]);
                this->bottomPlane = makePlane(vec[2], vec[5], vec[1]);
                this->leftPlane = makePlane(vec[3], vec[6], vec[2]);
                this->rightPlane = makePlane(vec[1], vec[4], vec[0]);
            } else {
                this->farPlane = makePlane(vec[4], vec[7], vec[5]);
                this->nearPlane = makePlane(vec[3], vec[0], vec[2]);
                this->topPlane = makePlane(vec[3], vec[7], vec[0]);
                this->bottomPlane = makePlane(vec[1], vec[5], vec[2]);
                this->leftPlane = makePlane(vec[2], vec[6], vec[3]);
                this->rightPlane = makePlane(vec[0], vec[4], vec[1]);
            }
        }


        bool isSphereInside(const Vector3& center, float radius) const {
            if (radius < 0.0f) return false;

            Vector4 pt = Vector4(center.x, center.y, center.z, 1.0f);
            if (Vector4::dot(this->nearPlane, pt) < -radius)   return false;
            if (Vector4::dot(this->farPlane, pt) < -radius)    return false;
            if (Vector4::dot(this->leftPlane, pt) < -radius)   return false;
            if (Vector4::dot(this->rightPlane, pt) < -radius)  return false;
            if (Vector4::dot(this->topPlane, pt) < -radius)    return false;
            if (Vector4::dot(this->bottomPlane, pt) < -radius) return false;
            return true;
        }

        bool isPointInside(const Vector3& point) const {
            return isSphereInside(point, 0.0f);
        }
    };
}
