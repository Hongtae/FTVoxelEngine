#include "AffineTransform2.h"

using namespace FV;

const AffineTransform2 AffineTransform2::identity = {
    Matrix2::identity, Vector2::zero
};

Matrix3 AffineTransform2::matrix3() const
{
    return Matrix3(
        matrix2._11, matrix2._12, 0.0f,
        matrix2._21, matrix2._22, 0.0f,
        translation.x, translation.y, 1.0f);
}

AffineTransform2 AffineTransform2::translated(const Vector2& offset) const
{
    return AffineTransform2(matrix2, translation + offset);
}

AffineTransform2& AffineTransform2::translate(const Vector2& offset)
{
    *this = translated(offset);
    return *this;
}

AffineTransform2 AffineTransform2::inverted() const
{
    Matrix2 matrix = matrix2.inverted();
    Vector2 origin = (-translation).applying(matrix);
    return AffineTransform2(matrix, origin);
}

AffineTransform2& AffineTransform2::invert()
{
    *this = inverted();
    return *this; 
}

AffineTransform2 AffineTransform2::concatenating(const AffineTransform2& rhs) const
{
    return AffineTransform2(matrix2.concatenating(rhs.matrix2),
                            translation + rhs.translation);
}

AffineTransform2& AffineTransform2::concatenate(const AffineTransform2& rhs)
{
    *this = concatenating(rhs);
    return *this;
}

bool AffineTransform2::operator==(const AffineTransform2& rhs) const
{
    return matrix2 == rhs.matrix2 && translation == rhs.translation;
}