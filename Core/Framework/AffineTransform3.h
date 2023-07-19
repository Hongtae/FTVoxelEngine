#pragma once
#include "../include.h"
#include "Vector3.h"
#include "Matrix3.h"

#pragma pack(push, 4)
namespace FV
{
    // 4x3 matrix for affine transform on 3 dimensional coordinates.
    struct FVCORE_API AffineTransform3
    {
        AffineTransform3()
            : linear(Matrix3::identity), translation(Vector3::zero) {}
        AffineTransform3(const Matrix3& m, const Vector3& t = Vector3::zero)
            : linear(m), translation(t) {}
        AffineTransform3(const Vector3& t)
            : linear(Matrix3::identity), translation(t) {}
        AffineTransform3(const Vector3& axisX,
                         const Vector3& axisY, 
                         const Vector3& axisZ,
                         const Vector3& origin = Vector3::zero)
            : linear(axisX, axisY, axisZ), translation(origin) {}

        Matrix3 linear;
        Vector3 translation;

        static const AffineTransform3 identity;

        AffineTransform3 translated(const Vector3& offset) const;
        AffineTransform3& translate(const Vector3& offset);

        AffineTransform3 inverted() const;
        AffineTransform3& invert();

        AffineTransform3 concatenating(const AffineTransform3& rhs) const;
        AffineTransform3& concatenate(const AffineTransform3& rhs);

        bool operator==(const AffineTransform3& rhs) const;
    };
}
#pragma pack(pop)
