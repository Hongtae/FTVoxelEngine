#pragma once
#include "../include.h"

#pragma pack(push, 4)
namespace FV
{
    struct Matrix2;
    struct AffineTransform2;
    struct FVCORE_API Vector2
    {
        Vector2() : x(0), y(0) {}
        Vector2(float _x, float _y) : x(_x), y(_y) {}

        float magnitudeSquared() const  { return dot(*this, *this); }
        float magnitude() const         { return sqrt(magnitudeSquared()); }

        static float dot(const Vector2& v1, const Vector2& v2) { return (v1.x * v2.x) + (v1.y * v2.y); }
        static Vector2 lerp(const Vector2&, const Vector2&, float);
        static Vector2 maximum(const Vector2&, const Vector2&);
        static Vector2 minimum(const Vector2&, const Vector2&);

        Vector2 applying(const Matrix2&) const;
        Vector2 applying(const AffineTransform2&) const;
        Vector2& apply(const Matrix2& m)          { *this = applying(m); return *this; }
        Vector2& apply(const AffineTransform2& t) { *this = applying(t); return *this; }

        Vector2 normalized() const;
        Vector2& normalize() { *this = normalized(); return *this; }

        float length() const { return magnitude(); }

        Vector2 operator + (const Vector2& v) const { return { x + v.x, y + v.y }; }
        Vector2 operator - (const Vector2& v) const { return { x - v.x, y - v.y }; }
        Vector2 operator * (const Vector2& v) const { return { x * v.x, y * v.y }; }
        Vector2 operator / (const Vector2& v) const { return { x / v.x, y / v.y }; }
        Vector2 operator * (float f) const          { return { x * f, y * f }; }
        Vector2 operator / (float f) const          { return (*this) * (1.0f / f); }
        Vector2 operator - () const                 { return { -x, -y }; }

        Vector2& operator += (const Vector2& v) { *this = (*this) + v; return *this; }
        Vector2& operator -= (const Vector2& v) { *this = (*this) - v; return *this; }
        Vector2& operator *= (const Vector2& v) { *this = (*this) * v; return *this; }
        Vector2& operator /= (const Vector2& v) { *this = (*this) * v; return *this; }
        Vector2& operator *= (float f)          { *this = (*this) * f; return *this; }
        Vector2& operator /= (float f)          { *this = (*this) * f; return *this; }

        bool operator==(const Vector2& v) const { return x == v.x && y == v.y; }

        union {
            struct {
                float x, y;
            };
            float val[2];
        };

        static const Vector2 zero;
    };
}
#pragma pack(pop)