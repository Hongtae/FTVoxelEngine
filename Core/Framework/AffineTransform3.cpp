#include "AffineTransform3.h"

using namespace FV;

const AffineTransform3 AffineTransform3::identity = {
    Matrix3(1, 0, 0,
            0, 1, 0,
            0, 0, 1),
    Vector3(0, 0, 0)
};

Matrix4 AffineTransform3::matrix4() const {
    return Matrix4(
        matrix3._11, matrix3._12, matrix3._13, 0.0f,
        matrix3._21, matrix3._22, matrix3._23, 0.0f,
        matrix3._31, matrix3._32, matrix3._33, 0.0f,
        translation.x, translation.y, translation.z, 1.0f);
}

AffineTransform3 AffineTransform3::translated(const Vector3& offset) const {
    return AffineTransform3(matrix3, translation + offset);
}

AffineTransform3& AffineTransform3::translate(const Vector3& offset) {
    *this = translated(offset);
    return *this;
}

AffineTransform3 AffineTransform3::scaled(const Vector3& s) const {
    auto c1 = matrix3.column1() * s.x;
    auto c2 = matrix3.column2() * s.y;
    auto c3 = matrix3.column3() * s.z;
    return { Matrix3(c1, c2, c3).transposed(), translation };
}

AffineTransform3& AffineTransform3::scale(const Vector3& s) {
    matrix3._11 *= s.x;
    matrix3._21 *= s.x;
    matrix3._31 *= s.x;
    matrix3._12 *= s.y;
    matrix3._22 *= s.y;
    matrix3._32 *= s.y;
    matrix3._13 *= s.z;
    matrix3._23 *= s.z;
    matrix3._33 *= s.z;
    return *this;
}

AffineTransform3 AffineTransform3::rotated(const Quaternion& q) const {
    Matrix3 mat = matrix3.concatenating(q.matrix3());
    return { mat, this->translation };
}

AffineTransform3& AffineTransform3::rotate(const Quaternion& q) {
    matrix3.concatenate(q.matrix3());
    return *this;
}

AffineTransform3 AffineTransform3::inverted() const {
    Matrix3 matrix = matrix3.inverted();
    Vector3 origin = (-translation).applying(matrix);
    return AffineTransform3(matrix, origin);
}

AffineTransform3& AffineTransform3::invert() {
    *this = inverted();
    return *this;
}

AffineTransform3 AffineTransform3::concatenating(const AffineTransform3& rhs) const {
    return AffineTransform3(matrix3.concatenating(rhs.matrix3),
                            translation + rhs.translation);
}

AffineTransform3& AffineTransform3::concatenate(const AffineTransform3& rhs) {
    *this = concatenating(rhs);
    return *this;
}

bool AffineTransform3::decompose(Vector3& scale, Quaternion& rotation) const {
    constexpr float epsilon = std::numeric_limits<float>::epsilon();

    if (abs(matrix3.determinant()) < epsilon)
        return false;

    Vector3 row[3] = {
        matrix3.row1(),
        matrix3.row2(),
        matrix3.row3(),
    };

    float skewYZ, skewXZ, skewXY;

    // get scale-x and normalize row-1
    scale.x = row[0].magnitude();
    row[0] = row[0] / scale.x;

    // xy shear
    skewXY = Vector3::dot(row[0], row[1]);
    row[1] = row[1] + row[0] * -skewXY;

    // get scale-y, normalize row-2
    scale.y = row[1].magnitude();
    row[1] = row[1] / scale.y;
    skewXY /= scale.y;

    // xz, yz shear
    skewXZ = Vector3::dot(row[0], row[2]);
    row[2] = row[2] + row[0] * -skewXZ;
    skewYZ = Vector3::dot(row[1], row[2]);
    row[2] = row[2] + row[1] * -skewYZ;

    // get scale-z, normalize row-3
    scale.z = row[2].magnitude();
    row[2] = row[2] / scale.z;
    skewXZ /= scale.z;
    skewYZ /= scale.z;

    //Vector3 skew = { skewYZ, skewXZ, skewXY };

    // check coordindate system flip
    Vector3 pdum3 = Vector3::cross(row[1], row[2]);
    if (Vector3::dot(row[0], pdum3) < 0.0f) {
        scale *= -1.0f;
        row[0] *= -1.0f;
        row[1] *= -1.0f;
        row[2] *= -1.0f;
    }

    float t = row[0].x + row[1].y + row[2].z;
    if (t > 0.0f) {
        float root = sqrt(t + 1.0f);
        rotation.w = 0.5f * root;
        root = 0.5f / root;
        rotation.x = root * (row[1].z - row[2].y);
        rotation.y = root * (row[2].x - row[0].z);
        rotation.z = root * (row[0].y - row[1].x);
    } else {
        int i = 0;
        if (row[1].y > row[0].x) { i = 1; }
        if (row[2].z > row[i].val[i]) { i = 2; }
        int j = (i + 1) % 3;
        int k = (j + 1) % 3;

        float root = sqrt(row[i].val[i] - row[j].val[j] - row[k].val[k] + 1.0f);
        rotation.val[i] = 0.5f * root;
        root = 0.5f / root;
        rotation.val[j] = root * (row[i].val[j] + row[j].val[i]);
        rotation.val[k] = root * (row[i].val[k] + row[k].val[i]);
        rotation.w = root * (row[j].val[k] - row[k].val[j]);
    }
    return true;
}

bool AffineTransform3::operator==(const AffineTransform3& rhs) const {
    return matrix3 == rhs.matrix3 && translation == rhs.translation;
}
