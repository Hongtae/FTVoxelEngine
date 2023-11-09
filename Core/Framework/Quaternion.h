#pragma once
#include "../include.h"
#include <numbers>
#include "Vector4.h"

#pragma pack(push, 4)
namespace FV {
    template <typename T> requires std::floating_point<T>
    inline constexpr T radianToDegree(T r) {
        return r * T(180.0 / std::numbers::pi);
    }
    template <typename T> requires std::floating_point<T>
    inline constexpr T degreeToRadian(T d) {
        return d * T(std::numbers::pi / 180.0);
    }

    struct Vector3;
    struct Matrix3;
    struct FVCORE_API Quaternion {
        Quaternion() : x(0), y(0), z(0), w(1) {}
        Quaternion(float _x, float _y, float _z, float _w)
            : x(_x), y(_y), z(_z), w(_w) {}
        Quaternion(const Vector3& axis, float angle);
        Quaternion(float pitch, float yaw, float roll); ///< radian
        Quaternion(const Vector3& from, const Vector3& to, float t = 1.0); ///< t > 1 for over-rotate, < 0 for inverse

        static float dot(const Quaternion&, const Quaternion&);
        static Quaternion lerp(const Quaternion&, const Quaternion&, float);
        static Quaternion slerp(const Quaternion&, const Quaternion&, float);

        float magnitudeSquared() const { return dot(*this, *this); }
        float magnitude() const { return sqrt(magnitudeSquared()); }

        Quaternion normalized() const;
        Quaternion& normalize() { (*this) = normalized(); return *this; }

        Quaternion conjugated() const { return {-x, -y, -z, w}; }
        Quaternion& conjugate() { (*this) = conjugated(); return *this; }

        Quaternion inverted() const;
        Quaternion& invert() { (*this) = inverted(); return *this; }

        float roll() const;
        float pitch() const;
        float yaw() const;
        float angle() const;
        Vector3 axis() const;

        Quaternion concatenating(const Quaternion&) const;
        Quaternion& concatenate(const Quaternion&);

        Vector4 vector4() const { return { x, y, z, w }; }
        Matrix3 matrix3() const;

        Quaternion operator + (const Quaternion& v) const	{ return { x + v.x, y + v.y, z + v.z, w + v.w }; }
        Quaternion operator - (const Quaternion& v) const	{ return { x - v.x, y - v.y, z - v.z, w - v.w }; }
        Quaternion operator * (const Quaternion& v) const	{ return concatenating(v); }
        Quaternion operator / (const Quaternion& v) const	{ return { x / v.x, y / v.y, z / v.z, w / v.w }; }
        Quaternion operator * (float f) const				{ return { x * f, y * f, z * f, w * f }; }
        Quaternion operator / (float f) const				{ return (*this) * (1.0f / f); }
        Quaternion operator - () const						{ return { -x, -y, -z, -w }; }

        Quaternion& operator += (const Quaternion& v) { *this = (*this) + v; return *this; }
        Quaternion& operator -= (const Quaternion& v) { *this = (*this) - v; return *this; }
        Quaternion& operator *= (const Quaternion& v) { *this = (*this) * v; return *this; }
        Quaternion& operator /= (const Quaternion& v) { *this = (*this) * v; return *this; }
        Quaternion& operator *= (float f) 			  { *this = (*this) * f; return *this; }
        Quaternion& operator /= (float f)             { *this = (*this) * f; return *this; }

        bool operator==(const Quaternion& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }

        union {
            struct {
                float x, y, z, w;
            };
            float val[4];
        };

        static const Quaternion identity;
    };
}
#pragma pack(pop)

namespace std {
    template <> struct formatter<FV::Quaternion> : formatter<string> {
        auto format(const FV::Quaternion& arg, format_context& ctx) {
            auto str = std::format("Quaternion({}, {}, {}, {})", arg.x, arg.y, arg.z, arg.w);
            return formatter<string>::format(str, ctx);
        }
    };
}
