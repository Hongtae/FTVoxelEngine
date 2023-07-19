#include "AffineTransform2.h"

using namespace FV;

const AffineTransform2 AffineTransform2::identity = {
    Matrix2::identity, Vector2::zero
};

AffineTransform2 AffineTransform2::translated(const Vector2& offset) const
{
    return AffineTransform2(linear, translation + offset);
}

AffineTransform2& AffineTransform2::translate(const Vector2& offset)
{
    *this = translated(offset);
    return *this;
}

AffineTransform2 AffineTransform2::inverted() const
{
    Matrix2 matrix = linear.inverted();
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
    return AffineTransform2(linear.concatenating(rhs.linear),
                            translation + rhs.translation);
}

AffineTransform2& AffineTransform2::concatenate(const AffineTransform2& rhs)
{
    *this = concatenating(rhs);
    return *this;
}

bool AffineTransform2::operator==(const AffineTransform2& rhs) const
{
    return linear == rhs.linear && translation == rhs.translation;
}