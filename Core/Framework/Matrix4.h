#pragma once
#include "../include.h"
#include "Vector4.h"

#pragma pack(push, 4)
namespace FV {
    struct FVCORE_API Matrix4 {
        Matrix4()
            : _11(1), _12(0), _13(0), _14(0)
            , _21(0), _22(1), _23(0), _24(0)
            , _31(0), _32(0), _33(1), _34(0)
            , _41(0), _42(0), _43(1), _44(1) {
        }

        Matrix4(const Vector4& row1, const Vector4& row2, const Vector4& row3, const Vector4& row4)
            : _11(row1.x), _12(row1.y), _13(row1.z), _14(row1.w)
            , _21(row2.x), _22(row2.y), _23(row2.z), _24(row2.w)
            , _31(row3.x), _32(row3.y), _33(row3.z), _34(row3.w)
            , _41(row4.x), _42(row4.y), _43(row4.z), _44(row4.w) {
        }

        Matrix4(float m11, float m12, float m13, float m14,
                float m21, float m22, float m23, float m24,
                float m31, float m32, float m33, float m34,
                float m41, float m42, float m43, float m44)
            : _11(m11), _12(m12), _13(m13), _14(m14)
            , _21(m21), _22(m22), _23(m23), _24(m24)
            , _31(m31), _32(m32), _33(m33), _34(m34)
            , _41(m41), _42(m42), _43(m43), _44(m44) {
        }

        float determinant() const;
        Matrix4 inverted() const;
        Matrix4& invert()          { *this = inverted(); return *this; }
        Matrix4 transposed() const { return { column1(), column2(), column3(), column4() }; }
        Matrix4& transpose()       { *this = transposed(); return *this; }
        Matrix4 concatenating(const Matrix4&) const;
        Matrix4& concatenate(const Matrix4&);

        Vector4 row1() const { return { _11, _12, _13, _14 }; }
        Vector4 row2() const { return { _21, _22, _23, _24 }; }
        Vector4 row3() const { return { _31, _32, _33, _34 }; }
        Vector4 row4() const { return { _41, _42, _43, _44 }; }
        Vector4 column1() const { return { _11, _21, _31, _41 }; }
        Vector4 column2() const { return { _12, _22, _32, _42 }; }
        Vector4 column3() const { return { _13, _23 ,_33 ,_43 }; }
        Vector4 column4() const { return { _14, _24 ,_34 ,_44 }; }

        Matrix4 operator + (const Matrix4& m) const;
        Matrix4 operator - (const Matrix4& m) const;
        Matrix4 operator * (const Matrix4& m) const { return concatenating(m); }
        Matrix4 operator / (const Matrix4& m) const { return concatenating(m.inverted()); }
        Matrix4 operator * (float f) const;
        Matrix4 operator / (float f) const { return (*this) * (1.0f / f); }

        Matrix4& operator += (const Matrix4& m) { *this = (*this) + m; return *this; }
        Matrix4& operator -= (const Matrix4& m) { *this = (*this) - m; return *this; }
        Matrix4& operator *= (const Matrix4& m) { *this = (*this) * m; return *this; }
        Matrix4& operator /= (const Matrix4& m) { *this = (*this) * m; return *this; }
        Matrix4& operator *= (float f)          { *this = (*this) * f; return *this; }
        Matrix4& operator /= (float f)          { *this = (*this) * f; return *this; }

        bool operator==(const Matrix4&) const;

        union {
            struct {
                float _11, _12, _13, _14;
                float _21, _22, _23, _24;
                float _31, _32, _33, _34;
                float _41, _42, _43, _44;
            };
            struct {
                float m[4][4];
            };
            float val[16];
        };

        static const Matrix4 identity;
    };
}
#pragma pack(pop)