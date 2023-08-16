#pragma once
#include "../include.h"
#include "Vector2.h"

#pragma pack(push, 4)
namespace FV
{
    struct FVCORE_API Matrix2
    {
        Matrix2()
            : _11(1), _12(0) , _21(0), _22(1) {}

        Matrix2(const Vector2& row1, const Vector2& row2)
            : _11(row1.x), _12(row1.y), _21(row2.x), _22(row2.y) {}

        Matrix2(float m11, float m12, float m21, float m22)
            : _11(m11), _12(m12), _21(m21), _22(m22) {}

        float determinant() const;
        Matrix2 inverted() const;
        Matrix2& invert()           { *this = inverted(); return *this; }
        Matrix2 transposed() const  { return { column1(), column2() }; }
        Matrix2& transpose()        { *this = transposed(); return *this; }
        Matrix2 concatenating(const Matrix2&) const;
        Matrix2& concatenate(const Matrix2&);

        Vector2 row1() const	{ return { _11, _12 }; }
        Vector2 row2() const	{ return { _21, _22 }; }
        Vector2 column1() const { return { _11, _21 }; }
        Vector2 column2() const { return { _12, _22 }; }

        Matrix2 operator + (const Matrix2& m) const;
        Matrix2 operator - (const Matrix2& m) const;
        Matrix2 operator * (const Matrix2& m) const { return concatenating(m); }
        Matrix2 operator / (const Matrix2& m) const { return concatenating(m.inverted()); }
        Matrix2 operator * (float f) const;
        Matrix2 operator / (float f) const { return (*this) * (1.0f / f); }

        Matrix2& operator += (const Matrix2& m) { *this = (*this) + m; return *this; }
        Matrix2& operator -= (const Matrix2& m) { *this = (*this) - m; return *this; }
        Matrix2& operator *= (const Matrix2& m) { *this = (*this) * m; return *this; }
        Matrix2& operator /= (const Matrix2& m) { *this = (*this) * m; return *this; }
        Matrix2& operator *= (float f)          { *this = (*this) * f; return *this; }
        Matrix2& operator /= (float f)          { *this = (*this) * f; return *this; }

        bool operator==(const Matrix2& v) const;

        union
        {
            struct
            {
                float _11, _12;
                float _21, _22;
            };
            struct
            {
                float m[2][2];
            };
            float val[4];
        };

        static const Matrix2 identity;
    };
}
#pragma pack(pop)