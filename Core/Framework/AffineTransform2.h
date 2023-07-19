#pragma once
#include "../include.h"
#include "Matrix2.h"
#include "Vector2.h"

#pragma pack(push, 4)
namespace FV
{
    struct FVCORE_API AffineTransform2
    {
        AffineTransform2()
            : linear(Matrix2::identity), translation(Vector2::zero) {}
        AffineTransform2(const Matrix2& m, const Vector2& t = Vector2::zero)
            : linear(m), translation(t) {}
        AffineTransform2(const Vector2& t)
            : linear(Matrix2::identity), translation(t) {}
        AffineTransform2(const Vector2& axisX, const Vector2& axisY,
                         const Vector2& origin = Vector2::zero)
            : linear(axisX, axisY), translation(origin) {}

        Matrix2 linear;
        Vector2 translation;

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
