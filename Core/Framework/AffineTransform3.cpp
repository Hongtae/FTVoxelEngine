#include "AffineTransform3.h"

using namespace FV;

const AffineTransform3 AffineTransform3::identity = {
    Matrix3(1, 0, 0,
            0, 1, 0,
            0, 0, 1),
    Vector3(0, 0, 0)
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

AffineTransform3 AffineTransform3::scaled(const Vector3& s) const {
    auto c1 = matrix3.column1() * s.x;
    auto c2 = matrix3.column2() * s.y;
    auto c3 = matrix3.column3() * s.z;
    return { Matrix3(c1, c2, c3).transposed(), translation };
}

AffineTransform3& AffineTransform3::scale(const Vector3& s) {
    matrix3._11 *= s.x;
    matrix3._21 *= s.x;
    matrix3._31 *= s.x;
    matrix3._12 *= s.y;
    matrix3._22 *= s.y;
    matrix3._32 *= s.y;
    matrix3._13 *= s.z;
    matrix3._23 *= s.z;
    matrix3._33 *= s.z;
    return *this;
}

AffineTransform3 AffineTransform3::rotated(const Quaternion& q) const {
    Matrix3 mat = matrix3.concatenating(q.matrix3());
    return { mat, this->translation };
}

AffineTransform3& AffineTransform3::rotate(const Quaternion& q) {
    matrix3.concatenate(q.matrix3());
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

bool AffineTransform3::decompose(Vector3& scale, Quaternion& rotation) const {
    Vector3 row1 = matrix3.row1();
    Vector3 row2 = matrix3.row2();
    Vector3 row3 = matrix3.row3();
    Vector3 s = { row1.magnitude(),
                  row2.magnitude(),
                  row3.magnitude() };
    if (s.x * s.y * s.z == 0.0) return false;

    Matrix3 mat = Matrix3(row1 / s.x,
                          row2 / s.y,
                          row3 / s.z);

    float x = sqrt(std::max(0.0f, 1.0f + mat._11 - mat._22 - mat._33)) * 0.5f;
    float y = sqrt(std::max(0.0f, 1.0f - mat._11 + mat._22 - mat._33)) * 0.5f;
    float z = sqrt(std::max(0.0f, 1.0f - mat._11 - mat._22 + mat._33)) * 0.5f;
    float w = sqrt(std::max(0.0f, 1.0f + mat._11 + mat._22 + mat._33)) * 0.5f;
    x = copysign(x, mat._23 - mat._32);
    y = copysign(y, mat._31 - mat._13);
    z = copysign(z, mat._12 - mat._21);

    rotation = Quaternion(x, y, z, w);
    scale = s;
    return true;
}

bool AffineTransform3::operator==(const AffineTransform3& rhs) const {
    return matrix3 == rhs.matrix3 && translation == rhs.translation;
}
