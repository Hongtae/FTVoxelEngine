#include "Vector3.h"
#include "Matrix3.h"
#include "AffineTransform3.h"
#include "Quaternion.h"

using namespace FV;

const Vector3 Vector3::zero = {};

float Vector3::dot(const Vector3& v1, const Vector3& v2) {
    return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
}

Vector3 Vector3::cross(const Vector3& v1, const Vector3& v2) {
    return {
        v1.y * v2.z - v1.z * v2.y,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x
    };
}

Vector3 Vector3::applying(const Matrix3& m) const {
    float x = dot(*this, m.column1());
    float y = dot(*this, m.column2());
    float z = dot(*this, m.column3());
    return { x, y, z };
}

Vector3 Vector3::applying(const AffineTransform3& t) const {
    return applying(t.matrix3) + t.translation;
}

Vector3 Vector3::applying(const Quaternion& q) const {
    auto vec = Vector3(q.x, q.y, q.z);
    auto uv = Vector3::cross(vec, *this);
    auto uuv = Vector3::cross(vec, uv);
    uv *= (2.0f * q.w);
    uuv *= 2.0f;
    return (*this) + uv + uuv;
}

Vector3 Vector3::normalized() const {
    auto sq = magnitudeSquared();
    if (sq != 0.0f) return (*this) / sqrt(sq);
    return *this;
}

Vector3 Vector3::lerp(const Vector3& v1, const Vector3& v2, float t) {
    return v1 * (1.0f - t) + v2 * t;
}

Vector3 Vector3::maximum(const Vector3& v1, const Vector3& v2) {
    return Vector3(std::max(v1.x, v2.x), std::max(v1.y, v2.y), std::max(v1.z, v2.z));
}

Vector3 Vector3::minimum(const Vector3& v1, const Vector3& v2) {
    return Vector3(std::min(v1.x, v2.x), std::min(v1.y, v2.y), std::min(v1.z, v2.z));
}
