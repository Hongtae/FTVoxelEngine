#include "Transform.h"
#include "Matrix3.h"

using namespace FV;

const Transform Transform::identity = { Quaternion::identity, Vector3::zero };

Matrix4 Transform::matrix4() const {
    auto m = orientation.matrix3();
    return Matrix4(m._11, m._12, m._13, 0.0f,
                   m._21, m._22, m._23, 0.0f,
                   m._31, m._32, m._33, 0.0f,
                   position.x, position.y, position.z, 1.0f);
}

Transform Transform::translated(const Vector3& offset) const {
    return Transform(offset).concatenating(*this);
}

Transform& Transform::translate(const Vector3& offset) {
    *this = translated(offset);
    return *this;
}

Transform Transform::rotated(const Quaternion& q) const {
    return Transform(q).concatenating(*this);
}

Transform& Transform::rotate(const Quaternion& q) {
    *this = rotated(q);
    return *this;
}

Transform Transform::inverted() const {
    auto r = orientation.conjugated();
    auto p = (-position).applying(r);
    return { r, p };
}

Transform& Transform::invert() {
    *this = inverted();
    return *this;
}

Transform Transform::concatenating(const Transform& rhs) const {
    auto r = orientation.concatenating(rhs.orientation);
    auto p = position.applying(rhs.orientation) + rhs.position;
    return { r, p };
}

Transform& Transform::concatenate(const Transform& rhs) {
    *this = concatenating(rhs);
    return *this;
}

bool Transform::operator==(const Transform& rhs) const {
    return orientation == rhs.orientation &&
        position == rhs.position;
}
