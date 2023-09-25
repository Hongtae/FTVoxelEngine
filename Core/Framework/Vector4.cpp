#include "Vector4.h"
#include "Matrix4.h"
#include "Vector3.h"

using namespace FV;

const Vector4 Vector4::zero = {};

Vector4::Vector4(const Vector3& v3, float _w)
    : x(v3.x), y(v3.y), z(v3.z), w(_w) {
}

float Vector4::dot(const Vector4& v1, const Vector4& v2) {
    return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z) + (v1.w * v2.w);
}

Vector4 Vector4::cross(const Vector4& v1, const Vector4& v2, const Vector4& v3) {
    return {
          v1.y * (v2.z * v3.w - v3.z * v2.w) - v1.z * (v2.y * v3.w - v3.y * v2.w) + v1.w * (v2.y * v3.z - v2.z * v3.y),
        -(v1.x * (v2.z * v3.w - v3.z * v2.w) - v1.z * (v2.x * v3.w - v3.x * v2.w) + v1.w * (v2.x * v3.z - v3.x * v2.z)),
          v1.x * (v2.y * v3.w - v3.y * v2.w) - v1.y * (v2.x * v3.w - v3.x * v2.w) + v1.w * (v2.x * v3.y - v3.x * v2.y),
        -(v1.x * (v2.y * v3.z - v3.y * v2.z) - v1.y * (v2.x * v3.z - v3.x * v2.z) + v1.z * (v2.x * v3.y - v3.x * v2.y))
    };
}

Vector4 Vector4::applying(const Matrix4& m) const {
    float x = dot(*this, m.column1());
    float y = dot(*this, m.column2());
    float z = dot(*this, m.column3());
    float w = dot(*this, m.column4());
    return { x, y, z, w };
}

Vector4 Vector4::normalized() const {
    auto sq = magnitudeSquared();
    if (sq != 0.0f) return (*this) / sqrt(sq);
    return *this;
}

Vector4 Vector4::lerp(const Vector4& v1, const Vector4& v2, float t) {
    return v1 * (1.0f - t) + v2 * t;
}

Vector4 Vector4::maximum(const Vector4& v1, const Vector4& v2) {
    return Vector4(std::max(v1.x, v2.x), std::max(v1.y, v2.y), std::max(v1.z, v2.z), std::max(v1.w, v2.w));
}

Vector4 Vector4::minimum(const Vector4& v1, const Vector4& v2) {
    return Vector4(std::min(v1.x, v2.x), std::min(v1.y, v2.y), std::min(v1.z, v2.z), std::min(v1.w, v2.w));
}
