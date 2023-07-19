#include "AffineTransform3.h"

using namespace FV;

const AffineTransform3 AffineTransform3::identity = {
    Matrix3::identity, Vector3::zero 
};

AffineTransform3 AffineTransform3::translated(const Vector3& offset) const
{
    return AffineTransform3(linear, translation + offset);
}

AffineTransform3& AffineTransform3::translate(const Vector3& offset)
{
    *this = translated(offset);
    return *this;
}

AffineTransform3 AffineTransform3::inverted() const
{
    Matrix3 matrix = linear.inverted();
    Vector3 origin = (-translation).applying(matrix);
    return AffineTransform3(matrix, origin);
}

AffineTransform3& AffineTransform3::invert()
{
    *this = inverted();
    return *this;
}

AffineTransform3 AffineTransform3::concatenating(const AffineTransform3& rhs) const
{
    return AffineTransform3(linear.concatenating(rhs.linear),
                            translation + rhs.translation);
}

AffineTransform3& AffineTransform3::concatenate(const AffineTransform3& rhs)
{
    *this = concatenating(rhs);
    return *this;
}

bool AffineTransform3::operator==(const AffineTransform3& rhs) const
{
    return linear == rhs.linear && translation == rhs.translation;
}
