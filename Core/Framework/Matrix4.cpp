#include "Matrix4.h"

using namespace FV;

const Matrix4 Matrix4::identity = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

float Matrix4::determinant() const {
    return
        _14 * _23 * _32 * _41 - _13 * _24 * _32 * _41 -
        _14 * _22 * _33 * _41 + _12 * _24 * _33 * _41 +
        _13 * _22 * _34 * _41 - _12 * _23 * _34 * _41 -
        _14 * _23 * _31 * _42 + _13 * _24 * _31 * _42 +
        _14 * _21 * _33 * _42 - _11 * _24 * _33 * _42 -
        _13 * _21 * _34 * _42 + _11 * _23 * _34 * _42 +
        _14 * _22 * _31 * _43 - _12 * _24 * _31 * _43 -
        _14 * _21 * _32 * _43 + _11 * _24 * _32 * _43 +
        _12 * _21 * _34 * _43 - _11 * _22 * _34 * _43 -
        _13 * _22 * _31 * _44 + _12 * _23 * _31 * _44 +
        _13 * _21 * _32 * _44 - _11 * _23 * _32 * _44 -
        _12 * _21 * _33 * _44 + _11 * _22 * _33 * _44;
}

Matrix4 Matrix4::inverted() const {
    float det = determinant();
    if (det != 0.0f) {
        float inv = 1.0f / det;

        float m11 = (_23 * _34 * _42 - _24 * _33 * _42 + _24 * _32 * _43 - _22 * _34 * _43 - _23 * _32 * _44 + _22 * _33 * _44) * inv;
        float m12 = (_14 * _33 * _42 - _13 * _34 * _42 - _14 * _32 * _43 + _12 * _34 * _43 + _13 * _32 * _44 - _12 * _33 * _44) * inv;
        float m13 = (_13 * _24 * _42 - _14 * _23 * _42 + _14 * _22 * _43 - _12 * _24 * _43 - _13 * _22 * _44 + _12 * _23 * _44) * inv;
        float m14 = (_14 * _23 * _32 - _13 * _24 * _32 - _14 * _22 * _33 + _12 * _24 * _33 + _13 * _22 * _34 - _12 * _23 * _34) * inv;
        float m21 = (_24 * _33 * _41 - _23 * _34 * _41 - _24 * _31 * _43 + _21 * _34 * _43 + _23 * _31 * _44 - _21 * _33 * _44) * inv;
        float m22 = (_13 * _34 * _41 - _14 * _33 * _41 + _14 * _31 * _43 - _11 * _34 * _43 - _13 * _31 * _44 + _11 * _33 * _44) * inv;
        float m23 = (_14 * _23 * _41 - _13 * _24 * _41 - _14 * _21 * _43 + _11 * _24 * _43 + _13 * _21 * _44 - _11 * _23 * _44) * inv;
        float m24 = (_13 * _24 * _31 - _14 * _23 * _31 + _14 * _21 * _33 - _11 * _24 * _33 - _13 * _21 * _34 + _11 * _23 * _34) * inv;
        float m31 = (_22 * _34 * _41 - _24 * _32 * _41 + _24 * _31 * _42 - _21 * _34 * _42 - _22 * _31 * _44 + _21 * _32 * _44) * inv;
        float m32 = (_14 * _32 * _41 - _12 * _34 * _41 - _14 * _31 * _42 + _11 * _34 * _42 + _12 * _31 * _44 - _11 * _32 * _44) * inv;
        float m33 = (_12 * _24 * _41 - _14 * _22 * _41 + _14 * _21 * _42 - _11 * _24 * _42 - _12 * _21 * _44 + _11 * _22 * _44) * inv;
        float m34 = (_14 * _22 * _31 - _12 * _24 * _31 - _14 * _21 * _32 + _11 * _24 * _32 + _12 * _21 * _34 - _11 * _22 * _34) * inv;
        float m41 = (_23 * _32 * _41 - _22 * _33 * _41 - _23 * _31 * _42 + _21 * _33 * _42 + _22 * _31 * _43 - _21 * _32 * _43) * inv;
        float m42 = (_12 * _33 * _41 - _13 * _32 * _41 + _13 * _31 * _42 - _11 * _33 * _42 - _12 * _31 * _43 + _11 * _32 * _43) * inv;
        float m43 = (_13 * _22 * _41 - _12 * _23 * _41 - _13 * _21 * _42 + _11 * _23 * _42 + _12 * _21 * _43 - _11 * _22 * _43) * inv;
        float m44 = (_12 * _23 * _31 - _13 * _22 * _31 + _13 * _21 * _32 - _11 * _23 * _32 - _12 * _21 * _33 + _11 * _22 * _33) * inv;

        return Matrix4(m11, m12, m13, m14,
                       m21, m22, m23, m24,
                       m31, m32, m33, m34,
                       m41, m42, m43, m44);
    }
    return identity;
}

Matrix4 Matrix4::concatenating(const Matrix4& m) const {
    Vector4 row1 = this->row1();
    Vector4 row2 = this->row2();
    Vector4 row3 = this->row3();
    Vector4 row4 = this->row4();
    Vector4 col1 = m.column1();
    Vector4 col2 = m.column2();
    Vector4 col3 = m.column3();
    Vector4 col4 = m.column4();

    return Matrix4(Vector4::dot(row1, col1), Vector4::dot(row1, col2), Vector4::dot(row1, col3), Vector4::dot(row1, col4),
                   Vector4::dot(row2, col1), Vector4::dot(row2, col2), Vector4::dot(row2, col3), Vector4::dot(row2, col4),
                   Vector4::dot(row3, col1), Vector4::dot(row3, col2), Vector4::dot(row3, col3), Vector4::dot(row3, col4),
                   Vector4::dot(row4, col1), Vector4::dot(row4, col2), Vector4::dot(row4, col3), Vector4::dot(row4, col4));
}

Matrix4& Matrix4::concatenate(const Matrix4& rhs) {
    *this = concatenating(rhs);
    return *this;
}

Matrix4 Matrix4::operator + (const Matrix4& m) const {
    return Matrix4(_11 + m._11, _12 + m._12, _13 + m._13, _14 + m._14,
                   _21 + m._21, _22 + m._22, _23 + m._23, _24 + m._24,
                   _31 + m._31, _32 + m._32, _33 + m._33, _34 + m._34,
                   _41 + m._41, _42 + m._42, _43 + m._43, _44 + m._44);
}

Matrix4 Matrix4::operator - (const Matrix4& m) const {
    return Matrix4(_11 - m._11, _12 - m._12, _13 - m._13, _14 - m._14,
                   _21 - m._21, _22 - m._22, _23 - m._23, _24 - m._24,
                   _31 - m._31, _32 - m._32, _33 - m._33, _34 - m._34,
                   _41 - m._41, _42 - m._42, _43 - m._43, _44 - m._44);
}

Matrix4 Matrix4::operator * (float f) const {
    return Matrix4(_11 * f, _12 * f, _13 * f, _14 * f,
                   _21 * f, _22 * f, _23 * f, _24 * f,
                   _31 * f, _32 * f, _33 * f, _34 * f,
                   _41 * f, _42 * f, _43 * f, _44 * f);
}

bool Matrix4::operator==(const Matrix4& m) const {
    for (int i = 0; i < 16; ++i) {
        if (val[i] != m.val[i]) return false;
    }
    return true;
}
