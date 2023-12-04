#pragma once
#include "../include.h"

#pragma pack(push, 4)
namespace FV {
    struct Vector3;
    struct Matrix4;
    struct FVCORE_API Vector4 {
        Vector4() : x(0), y(0), z(0), w(0) {}
        Vector4(float _x, float _y, float _z, float _w)
            : x(_x), y(_y), z(_z), w(_w) {}
        Vector4(const Vector3& v3, float w);

        float magnitudeSquared() const { return dot(*this, *this); }
        float magnitude() const { return sqrt(magnitudeSquared()); }

        static float dot(const Vector4&, const Vector4&);
        static Vector4 cross(const Vector4&, const Vector4&, const Vector4&);
        static Vector4 lerp(const Vector4&, const Vector4&, float);
        static Vector4 maximum(const Vector4&, const Vector4&);
        static Vector4 minimum(const Vector4&, const Vector4&);

        Vector4 applying(const Matrix4&) const;
        Vector4& apply(const Matrix4& m) { *this = applying(m); return *this; }

        Vector4 normalized() const;
        Vector4& normalize() { *this = normalized(); return *this; }

        float length() const { return magnitude(); }

        Vector4 operator + (const Vector4& v) const { return { x + v.x, y + v.y, z + v.z, w + v.w }; }
        Vector4 operator - (const Vector4& v) const { return { x - v.x, y - v.y, z - v.z, w - v.w }; }
        Vector4 operator * (const Vector4& v) const { return { x * v.x, y * v.y, z * v.z, w * v.w }; }
        Vector4 operator / (const Vector4& v) const { return { x / v.x, y / v.y, z / v.z, w / v.w }; }
        Vector4 operator * (float f) const          { return { x * f, y * f, z * f, w * f }; }
        Vector4 operator / (float f) const          { return (*this) * (1.0f / f); }
        Vector4 operator - () const                 { return { -x, -y, -z, -w }; }

        Vector4& operator += (const Vector4& v) { *this = (*this) + v; return *this; }
        Vector4& operator -= (const Vector4& v) { *this = (*this) - v; return *this; }
        Vector4& operator *= (const Vector4& v) { *this = (*this) * v; return *this; }
        Vector4& operator /= (const Vector4& v) { *this = (*this) * v; return *this; }
        Vector4& operator *= (float f)          { *this = (*this) * f; return *this; }
        Vector4& operator /= (float f)          { *this = (*this) * f; return *this; }

        bool operator==(const Vector4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }

        union {
            struct {
                float x, y, z, w;
            };
            float val[4];
        };

        static const Vector4 zero;
    };
}
#pragma pack(pop)

namespace std {
    template <> struct formatter<FV::Vector4> : formatter<string> {
        auto format(const FV::Vector4& arg, format_context& ctx) const {
            auto str = std::format("Vector4({}, {}, {}, {})", arg.x, arg.y, arg.z, arg.w);
            return formatter<string>::format(str, ctx);
        }
    };
}
