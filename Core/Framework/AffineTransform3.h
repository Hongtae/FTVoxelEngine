#pragma once
#include "../include.h"
#include "Matrix3.h"
#include "Matrix4.h"
#include "Vector3.h"
#include "Quaternion.h"

#pragma pack(push, 4)
namespace FV {
    // 4x3 matrix for affine transform on 3 dimensional coordinates.
    struct FVCORE_API AffineTransform3 {
        AffineTransform3()
            : matrix3(Matrix3::identity), translation(Vector3::zero) {
        }
        AffineTransform3(const Matrix3& m, const Vector3& t = Vector3::zero)
            : matrix3(m), translation(t) {
        }
        AffineTransform3(const Vector3& t)
            : matrix3(Matrix3::identity), translation(t) {
        }
        AffineTransform3(const Vector3& axisX,
                         const Vector3& axisY,
                         const Vector3& axisZ,
                         const Vector3& origin = Vector3::zero)
            : matrix3(axisX, axisY, axisZ), translation(origin) {
        }
        explicit AffineTransform3(const Matrix4& m)
            : matrix3(m._11, m._12, m._13,
                      m._21, m._22, m._23,
                      m._31, m._32, m._33)
            , translation(m._41, m._42, m._43) {
        }

        Matrix3 matrix3;
        Vector3 translation;

        Matrix4 matrix4() const;

        static const AffineTransform3 identity;

        AffineTransform3 translated(const Vector3& offset) const;
        AffineTransform3& translate(const Vector3& offset);

        AffineTransform3 scaled(const Vector3& scale) const;
        AffineTransform3& scale(const Vector3& scale);

        AffineTransform3 rotated(const Quaternion& quat) const;
        AffineTransform3& rotate(const Quaternion& quat);

        AffineTransform3 inverted() const;
        AffineTransform3& invert();

        AffineTransform3 concatenating(const AffineTransform3& rhs) const;
        AffineTransform3& concatenate(const AffineTransform3& rhs);

        bool decompose(Vector3& scale, Quaternion& rotation) const;

        bool operator==(const AffineTransform3& rhs) const;
    };
}
#pragma pack(pop)
