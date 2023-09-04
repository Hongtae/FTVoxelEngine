#include "Matrix2.h"

using namespace FV;

const Matrix2 Matrix2::identity = {
    1, 0,
    0, 1
};

float Matrix2::determinant() const {
    return _11 * _22 - _12 * _21;
}

Matrix2 Matrix2::inverted() const {
    float det = determinant();
    if (det != 0.0f) {
        return Matrix2(_22, -_12, -_21, _11) / det;
    }
    return identity;
}

Matrix2 Matrix2::concatenating(const Matrix2& m) const {
    Vector2 row1 = this->row1();
    Vector2 row2 = this->row2();
    Vector2 col1 = m.column1();
    Vector2 col2 = m.column2();

    return Matrix2(Vector2::dot(row1, col1), Vector2::dot(row1, col2),
                   Vector2::dot(row2, col1), Vector2::dot(row2, col2));
}

Matrix2& Matrix2::concatenate(const Matrix2& rhs) {
    *this = concatenating(rhs);
    return *this;
}

Matrix2 Matrix2::operator + (const Matrix2& m) const {
    if ((*this) == m) return m;
    return Matrix2(_11 + m._11, _12 + m._12,
                   _21 + m._21, _22 + m._22);
}

Matrix2 Matrix2::operator - (const Matrix2& m) const {
    return Matrix2(_11 - m._11, _12 - m._12,
                   _21 - m._21, _22 - m._22);
}

Matrix2 Matrix2::operator * (float f) const {
    return Matrix2(_11 * f, _12 * f,
                   _21 * f, _22 * f);
}

bool Matrix2::operator==(const Matrix2& m) const {
    for (int i = 0; i < 4; ++i) {
        if (val[i] != m.val[i]) return false;
    }
    return true;
}
