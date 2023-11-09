#pragma once
#include "../include.h"

#pragma pack(push, 4)
namespace FV {
    struct Vector2;
    struct Matrix3;
    struct Matrix4;
    struct AffineTransform3;
    struct Quaternion;
    struct FVCORE_API Vector3 {
        Vector3() : x(0), y(0), z(0) {}
        Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
        Vector3(const Vector2& v2, float z);

        float magnitudeSquared() const { return dot(*this, *this); }
        float magnitude() const { return sqrt(magnitudeSquared()); }

        static float dot(const Vector3&, const Vector3&);
        static Vector3 cross(const Vector3&, const Vector3&);
        static Vector3 lerp(const Vector3&, const Vector3&, float);
        static Vector3 maximum(const Vector3&, const Vector3&);
        static Vector3 minimum(const Vector3&, const Vector3&);

        Vector3 applying(const Matrix3&) const;
        Vector3 applying(const AffineTransform3&) const;
        Vector3 applying(const Quaternion&) const;
        Vector3 applying(const Matrix4&, float w = 1.0f) const;

        Vector3& apply(const Matrix3& m)          { *this = applying(m); return *this; }
        Vector3& apply(const AffineTransform3& t) { *this = applying(t); return *this; }
        Vector3& apply(const Quaternion& q)       { *this = applying(q); return *this; }
        Vector3& apply(const Matrix4& m, float w = 1.0f) { *this = applying(m, w); return *this; }

        Vector3 normalized() const;
        Vector3& normalize() { *this = normalized(); return *this; }

        float length() const { return magnitude(); }

        Vector3 operator + (const Vector3& v) const { return { x + v.x, y + v.y, z + v.z }; }
        Vector3 operator - (const Vector3& v) const { return { x - v.x, y - v.y, z - v.z }; }
        Vector3 operator * (const Vector3& v) const { return { x * v.x, y * v.y, z * v.z }; }
        Vector3 operator / (const Vector3& v) const { return { x / v.x, y / v.y, z / v.z }; }
        Vector3 operator * (float f) const          { return { x * f, y * f, z * f }; }
        Vector3 operator / (float f) const          { return (*this) * (1.0f / f); }
        Vector3 operator - () const                 { return { -x, -y, -z }; }

        Vector3& operator += (const Vector3& v) { *this = (*this) + v; return *this; }
        Vector3& operator -= (const Vector3& v) { *this = (*this) - v; return *this; }
        Vector3& operator *= (const Vector3& v) { *this = (*this) * v; return *this; }
        Vector3& operator /= (const Vector3& v) { *this = (*this) * v; return *this; }
        Vector3& operator *= (float f)          { *this = (*this) * f; return *this; }
        Vector3& operator /= (float f)          { *this = (*this) * f; return *this; }

        bool operator==(const Vector3& v) const { return x == v.x && y == v.y && z == v.z; }

        union {
            struct {
                float x, y, z;
            };
            float val[3];
        };

        static const Vector3 zero;
    };
}
#pragma pack(pop)

namespace std {
    template <> struct formatter<FV::Vector3> : formatter<string> {
        auto format(const FV::Vector3& arg, format_context& ctx) {
            auto str = std::format("Vector3({}, {}, {})", arg.x, arg.y, arg.z);
            return formatter<string>::format(str, ctx);
        }
    };
}
