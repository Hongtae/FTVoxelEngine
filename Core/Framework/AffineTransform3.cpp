#include "AffineTransform3.h"

using namespace FV;

const AffineTransform3 AffineTransform3::identity = {
    Matrix3::identity, Vector3::zero
};

Matrix4 AffineTransform3::matrix4() const {
    return Matrix4(
        matrix3._11, matrix3._12, matrix3._13, 0.0f,
        matrix3._21, matrix3._22, matrix3._23, 0.0f,
        matrix3._31, matrix3._32, matrix3._33, 0.0f,
        translation.x, translation.y, translation.z, 1.0f);
}

AffineTransform3 AffineTransform3::translated(const Vector3& offset) const {
    return AffineTransform3(matrix3, translation + offset);
}

AffineTransform3& AffineTransform3::translate(const Vector3& offset) {
    *this = translated(offset);
    return *this;
}

AffineTransform3 AffineTransform3::inverted() const {
    Matrix3 matrix = matrix3.inverted();
    Vector3 origin = (-translation).applying(matrix);
    return AffineTransform3(matrix, origin);
}

AffineTransform3& AffineTransform3::invert() {
    *this = inverted();
    return *this;
}

AffineTransform3 AffineTransform3::concatenating(const AffineTransform3& rhs) const {
    return AffineTransform3(matrix3.concatenating(rhs.matrix3),
                            translation + rhs.translation);
}

AffineTransform3& AffineTransform3::concatenate(const AffineTransform3& rhs) {
    *this = concatenating(rhs);
    return *this;
}

bool AffineTransform3::operator==(const AffineTransform3& rhs) const {
    return matrix3 == rhs.matrix3 && translation == rhs.translation;
}
