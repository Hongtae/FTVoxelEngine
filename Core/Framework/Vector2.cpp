#include "Vector2.h"
#include "Matrix2.h"
#include "AffineTransform2.h"

using namespace FV;

const Vector2 Vector2::zero = {};

Vector2 Vector2::applying(const Matrix2& m) const
{
	float x = dot(*this, m.column1());
	float y = dot(*this, m.column2());
	return { x, y };
}

Vector2 Vector2::applying(const AffineTransform2& t) const
{
	return applying(t.matrix2) + t.translation;
}

Vector2 Vector2::normalized() const
{
	auto sq = magnitudeSquared();
	if (sq != 0.0f) return (*this) * sqrt(sq);
	return *this;
}

Vector2 Vector2::lerp(const Vector2& v1, const Vector2& v2, float t)
{
	return v1 * (1.0f - t) + v2 * t;
}

Vector2 Vector2::maximum(const Vector2& v1, const Vector2& v2)
{
	return Vector2(std::max(v1.x, v2.x), std::max(v1.y, v2.y));
}

Vector2 Vector2::minimum(const Vector2& v1, const Vector2& v2)
{
	return Vector2(std::min(v1.x, v2.x), std::min(v1.y, v2.y));
}