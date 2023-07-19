#pragma once
#include "../include.h"
#include "Vector3.h"

#pragma pack(push, 4)
namespace FV
{
    struct FVCORE_API Matrix3
    {
        Matrix3()
            : _11(1), _12(0), _13(0)
            , _21(0), _22(1), _23(0)
            , _31(0), _32(0), _33(1) {}

        Matrix3(const Vector3& row1, const Vector3& row2, const Vector3& row3)
            : _11(row1.x), _12(row1.y), _13(row1.z)
            , _21(row2.x), _22(row2.y), _23(row2.z)
            , _31(row3.x), _32(row3.y), _33(row3.z) {}

        Matrix3(float m11, float m12, float m13,
                float m21, float m22, float m23,
                float m31, float m32, float m33)
            : _11(m11), _12(m12), _13(m13)
            , _21(m21), _22(m22), _23(m23)
            , _31(m31), _32(m32), _33(m33) {}

        float determinant() const;
        Matrix3 inverted() const;
        Matrix3& invert()			{ *this = inverted(); return *this; }
        Matrix3 transposed() const	{ return { column1(), column2(), column3()}; }
        Matrix3& transpose()		{ *this = transposed(); return *this; }
        Matrix3 concatenating(const Matrix3&) const;
        Matrix3& concatenate(const Matrix3&);

        Vector3 row1() const { return { _11, _12, _13 }; }
        Vector3 row2() const { return { _21, _22, _23 }; }
        Vector3 row3() const { return { _31, _32, _33 }; }
        Vector3 column1() const { return { _11, _21, _31 }; }
        Vector3 column2() const { return { _12, _22, _32 }; }
        Vector3 column3() const { return { _13, _23 ,_33 }; }

        Matrix3 operator + (const Matrix3& m) const;
        Matrix3 operator - (const Matrix3& m) const;
        Matrix3 operator * (const Matrix3& m) const { return concatenating(m); }
        Matrix3 operator / (const Matrix3& m) const { return concatenating(m.inverted()); }
        Matrix3 operator * (float f) const;
        Matrix3 operator / (float f) const { return (*this) * (1.0 / f); }
              
        Matrix3& operator += (const Matrix3& m) { *this = (*this) + m; return *this; }
        Matrix3& operator -= (const Matrix3& m) { *this = (*this) - m; return *this; }
        Matrix3& operator *= (const Matrix3& m) { *this = (*this) * m; return *this; }
        Matrix3& operator /= (const Matrix3& m) { *this = (*this) * m; return *this; }
        Matrix3& operator *= (float f)          { *this = (*this) * f; return *this; }
        Matrix3& operator /= (float f)          { *this = (*this) * f; return *this; }

        bool operator==(const Matrix3&) const;

        union
        {
            struct
            {
                float _11, _12, _13;
                float _21, _22, _23;
                float _31, _32, _33;
            };
            struct
            {
                float m[3][3];
            };
            float val[9];
        };

        static const Matrix3 identity;
    };
}
#pragma pack(pop)