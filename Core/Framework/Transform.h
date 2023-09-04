#pragma once
#include "../include.h"
#include "Quaternion.h"
#include "Vector3.h"
#include "Matrix4.h"
#include "Quaternion.h"

namespace FV {
    struct FVCORE_API Transform {
        Transform(const Quaternion& q = Quaternion::identity,
                  const Vector3& p = Vector3::zero)
            : orientation(q), position(p) {
        }
        Transform(const Vector3& p)
            : orientation(Quaternion::identity), position(p) {
        }

        Quaternion orientation;
        Vector3 position;

        static const Transform identity;

        Matrix4 matrix4() const;

        Transform translated(const Vector3& offset) const;
        Transform& translate(const Vector3& offset);

        Transform rotated(const Quaternion&) const;
        Transform& rotate(const Quaternion&);

        Transform inverted() const;
        Transform& invert();

        Transform concatenating(const Transform& rhs) const;
        Transform& concatenate(const Transform& rhs);

        bool operator==(const Transform& rhs) const;
    };
}
