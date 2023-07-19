#include "Matrix3.h"

using namespace FV;

const Matrix3 Matrix3::identity = {
    1, 0, 0,
    0, 1, 0,
    0, 0, 1
};

float Matrix3::determinant() const
{
    return
        _11 * _22 * _33 + _12 * _23 * _31 +
        _13 * _21 * _32 - _11 * _23 * _32 -
        _12 * _21 * _33 - _13 * _22 * _31;
}

Matrix3 Matrix3::inverted() const
{
    float det = determinant();
    if (det != 0.0f)
    {
        float inv = 1.0f / det;

        float m11 = (_22 * _33 - _23 * _32) * inv;
        float m12 = (_13 * _32 - _12 * _33) * inv;
        float m13 = (_12 * _23 - _13 * _22) * inv;
        float m21 = (_23 * _31 - _21 * _33) * inv;
        float m22 = (_11 * _33 - _13 * _31) * inv;
        float m23 = (_13 * _21 - _11 * _23) * inv;
        float m31 = (_21 * _32 - _22 * _31) * inv;
        float m32 = (_12 * _31 - _11 * _32) * inv;
        float m33 = (_11 * _22 - _12 * _21) * inv;

        return Matrix3(m11, m12, m13,
                       m21, m22, m23,
                       m31, m32, m33);
    }
    return identity;
}

Matrix3 Matrix3::concatenating(const Matrix3& m) const
{
	Vector3 row1 = this->row1();
	Vector3 row2 = this->row2();
	Vector3 row3 = this->row3();
	Vector3 col1 = m.column1();
	Vector3 col2 = m.column2();
	Vector3 col3 = m.column3();

	return Matrix3(Vector3::dot(row1, col1), Vector3::dot(row1, col2), Vector3::dot(row1, col3),
				   Vector3::dot(row2, col1), Vector3::dot(row2, col2), Vector3::dot(row2, col3), 
				   Vector3::dot(row3, col1), Vector3::dot(row3, col2), Vector3::dot(row3, col3));
}

Matrix3& Matrix3::concatenate(const Matrix3& rhs)
{
	*this = concatenating(rhs);
	return *this;
}

Matrix3 Matrix3::operator + (const Matrix3& m) const {
    return Matrix3(_11 + m._11, _12 + m._12, _13 + m._13,
                   _21 + m._21, _22 + m._22, _23 + m._23,
                   _31 + m._31, _32 + m._32, _33 + m._33);
}

Matrix3 Matrix3::operator - (const Matrix3& m) const {
    return Matrix3(_11 - m._11, _12 - m._12, _13 - m._13,
                   _21 - m._21, _22 - m._22, _23 - m._23,
                   _31 - m._31, _32 - m._32, _33 - m._33);
}

Matrix3 Matrix3::operator * (float f) const {
    return Matrix3(_11 * f, _12 * f, _13 * f,
                   _21 * f, _22 * f, _23 * f,
                   _31 * f, _32 * f, _33 * f);
}

bool Matrix3::operator==(const Matrix3& m) const
{
    for (int i = 0; i < 9; ++i)
    {
        if (val[i] != m.val[i]) return false;
    }
    return true;
}
