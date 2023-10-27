#include "AffineTransform2.h"

using namespace FV;

const AffineTransform2 AffineTransform2::identity = {
    Matrix2(1, 0, 0, 1), Vector2(0, 0)
};

Matrix3 AffineTransform2::matrix3() const {
    return Matrix3(
        matrix2._11, matrix2._12, 0.0f,
        matrix2._21, matrix2._22, 0.0f,
        translation.x, translation.y, 1.0f);
}

AffineTransform2 AffineTransform2::translated(const Vector2& offset) const {
    return AffineTransform2(matrix2, translation + offset);
}

AffineTransform2& AffineTransform2::translate(const Vector2& offset) {
    *this = translated(offset);
    return *this;
}

AffineTransform2 AffineTransform2::scaled(const Vector2& s) const {
    auto c1 = matrix2.column1() * s.x;
    auto c2 = matrix2.column2() * s.y;
    return { Matrix2(c1, c2).transposed(), translation };
}

AffineTransform2& AffineTransform2::scale(const Vector2& s) {
    matrix2._11 *= s.x;
    matrix2._21 *= s.x;
    matrix2._12 *= s.y;
    matrix2._22 *= s.y;
    return *this;
}

AffineTransform2 AffineTransform2::rotated(float r) const {
    float c = cos(r);
    float s = sin(r);
    Matrix2 m = { c, s, -s, c };
    return { matrix2.concatenating(m), translation };
}

AffineTransform2& AffineTransform2::rotate(float r) {
    float c = cos(r);
    float s = sin(r);
    Matrix2 m = { c, s, -s, c };
    matrix2.concatenate(m);
    return *this;
}

AffineTransform2 AffineTransform2::inverted() const {
    Matrix2 matrix = matrix2.inverted();
    Vector2 origin = (-translation).applying(matrix);
    return AffineTransform2(matrix, origin);
}

AffineTransform2& AffineTransform2::invert() {
    *this = inverted();
    return *this;
}

AffineTransform2 AffineTransform2::concatenating(const AffineTransform2& rhs) const {
    return AffineTransform2(matrix2.concatenating(rhs.matrix2),
                            translation + rhs.translation);
}

AffineTransform2& AffineTransform2::concatenate(const AffineTransform2& rhs) {
    *this = concatenating(rhs);
    return *this;
}

bool AffineTransform2::operator==(const AffineTransform2& rhs) const {
    return matrix2 == rhs.matrix2 && translation == rhs.translation;
}