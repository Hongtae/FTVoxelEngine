#pragma once
#include "../include.h"
#include "Matrix2.h"
#include "Matrix3.h"
#include "Vector2.h"

#pragma pack(push, 4)
namespace FV
{
    struct FVCORE_API AffineTransform2
    {
        AffineTransform2()
            : matrix2(Matrix2::identity), translation(Vector2::zero) {}
        AffineTransform2(const Matrix2& m, const Vector2& t = Vector2::zero)
            : matrix2(m), translation(t) {}
        AffineTransform2(const Vector2& t)
            : matrix2(Matrix2::identity), translation(t) {}
        AffineTransform2(const Vector2& axisX, const Vector2& axisY,
                         const Vector2& origin = Vector2::zero)
            : matrix2(axisX, axisY), translation(origin) {}
        explicit AffineTransform2(const Matrix3& m)
            : matrix2(m._11, m._12, m._21, m._22) , translation(m._31, m._32)
        {}

        Matrix2 matrix2;
        Vector2 translation;

        Matrix3 matrix3() const;

        static const AffineTransform2 identity;

        AffineTransform2 translated(const Vector2& offset) const;
        AffineTransform2& translate(const Vector2& offset);

        AffineTransform2 inverted() const;
        AffineTransform2& invert();

        AffineTransform2 concatenating(const AffineTransform2& rhs) const;
        AffineTransform2& concatenate(const AffineTransform2& rhs);

        bool operator==(const AffineTransform2& rhs) const;
    };
}
#pragma pack(pop)
