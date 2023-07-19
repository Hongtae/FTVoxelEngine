#include "Quaternion.h"
#include "Vector3.h"
#include "Matrix3.h"

using namespace FV;

const Quaternion Quaternion::identity = {0, 0, 0, 1};

Quaternion::Quaternion(const Vector3& axis, float angle)
	: x(0), y(0), z(0), w(1)
{
	if (axis.length() > 0)
	{
		Vector3 au = axis.normalized();

		angle *= 0.5f;
		float sinR = sin(angle);

		x = sinR * au.x;
		y = sinR * au.y;
		z = sinR * au.z;
		w = cos(angle);
	}
}

Quaternion::Quaternion(float pitch, float yaw, float roll)
{
	float p = pitch * 0.5f;
	float y = yaw * 0.5f;
	float r = roll * 0.5f;

	float sinp = sin(p);
	float cosp = cos(p);
	float siny = sin(y);
	float cosy = cos(y);
	float sinr = sin(r);
	float cosr = cos(r);

	x = cosr * sinp * cosy + sinr * cosp * siny;
	y = cosr * cosp * siny - sinr * sinp * cosy;
	z = sinr * cosp * cosy - cosr * sinp * siny;
	w = cosr * cosp * cosy + sinr * sinp * siny;

	normalize();
}

Quaternion::Quaternion(const Vector3& from, const Vector3& to, float t)
	: x(0), y(0), z(0), w(1)
{
	float len1 = from.magnitude();
	float len2 = to.magnitude();
	if (len1 > 0 && len2 > 0)
	{
		Vector3 axis = Vector3::cross(from, to);
		float angle = acos(Vector3::dot(from.normalized(), to.normalized()));
		angle = angle * t;

		Quaternion q = Quaternion(axis, angle);
		x = q.x;
		y = q.y;
		z = q.z;
		w = q.w;
	}
}

float Quaternion::dot(const Quaternion& v1, const Quaternion& v2)
{
	return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z) + (v1.w * v2.w);
}

Quaternion Quaternion::normalized() const
{
	auto sq = magnitudeSquared();
	if (sq > 0.0f)
		return (*this) * sqrt(sq);
	return *this;
}

Quaternion Quaternion::inverted() const
{
	float sq = magnitudeSquared();
	if (sq > 0.0f)
		return conjugated() / sq;
	return *this;
}

float Quaternion::roll() const
{
	return atan2(2 * (x * y + w * z), w * w + x * x - y * y - z * z);
}

float Quaternion::pitch() const
{
	return atan2(2 * (y * z + w * x), w * w - x * x - y * y + z * z);
}
float Quaternion::yaw() const
{
	return asin(-2 * (x * z - w * y));
}

float Quaternion::angle() const
{
	float lenSq = magnitudeSquared();
	if (lenSq > 0.0f && fabs(w) < 1.0f)
		return 2.0f * acos(w);
	return 0.0f;
}

Vector3 Quaternion::axis() const
{
	float msq = magnitudeSquared();
	if (msq > 0.0f)
	{
		return Vector3(x, y, z) / sqrt(msq);
	}
	return Vector3(1, 0, 0);
}

Quaternion Quaternion::concatenating(const Quaternion& q) const
{
	return Quaternion(
		q.w * x + q.x * w + q.y * z - q.z * y,		// x
		q.w * y + q.y * w + q.z * x - q.x * z,		// y
		q.w * z + q.z * w + q.x * y - q.y * x,		// z
		q.w * w - q.x * x - q.y * y - q.z * z		// w
	);
}

Quaternion& Quaternion::concatenate(const Quaternion& rhs)
{
	*this = concatenating(rhs);
	return *this;
}

Matrix3 Quaternion::matrix3() const
{
	Matrix3 mat = Matrix3::identity;

	mat.m[0][0] = 1.0f - 2.0f * (y * y + z * z);
	mat.m[0][1] = 2.0f * (x * y + z * w);
	mat.m[0][2] = 2.0f * (x * z - y * w);

	mat.m[1][0] = 2.0f * (x * y - z * w);
	mat.m[1][1] = 1.0f - 2.0f * (x * x + z * z);
	mat.m[1][2] = 2.0f * (y * z + x * w);

	mat.m[2][0] = 2.0f * (x * z + y * w);
	mat.m[2][1] = 2.0f * (y * z - x * w);
	mat.m[2][2] = 1.0f - 2.0f * (x * x + y * y);

	return mat;
}

Quaternion Quaternion::lerp(const Quaternion& q1, const Quaternion& q2, float t)
{
	return q1 * (1.0f - t) + q2 * t;
}

Quaternion Quaternion::slerp(const Quaternion& q1, const Quaternion& q2, float t)
{
    float cosHalfTheta = dot(q1, q2);
    bool flip = cosHalfTheta < 0.0;
	if (flip) { cosHalfTheta = -cosHalfTheta; }

	if (cosHalfTheta >= 1.0f) { return q1; }    // q1 = q2 or q1 = -q2

	float halfTheta = acos(cosHalfTheta);
	float oneOverSinHalfTheta = 1.0 / sin(halfTheta);

	float t2 = 1.0f - t;

	float ratio1 = sin(halfTheta * t2) * oneOverSinHalfTheta;
	float ratio2 = sin(halfTheta * t) * oneOverSinHalfTheta;

	if (flip) { ratio2 = -ratio2; }

	return q1 * ratio1 + q2 * ratio2;
}
