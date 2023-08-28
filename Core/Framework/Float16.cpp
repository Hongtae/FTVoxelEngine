#include "Float16.h"

using namespace FV;

static inline Float16 uint16ToFloat16(uint16_t val)
{
	return reinterpret_cast<Float16&>(val);
}

const Float16 Float16::zero = uint16ToFloat16(0x0U);
const Float16 Float16::max = uint16ToFloat16(0x7bffU);
const Float16 Float16::min = uint16ToFloat16(0x400U);
const Float16 Float16::maxSubnormal = uint16ToFloat16(0x3ffU);
const Float16 Float16::minSubnormal = uint16ToFloat16(0x1U);
const Float16 Float16::posInfinity = uint16ToFloat16(0x7c00U);
const Float16 Float16::negInfinity = uint16ToFloat16(0xfc00U);


Float16::Float16()
	: binary16(0U)
{
}

Float16::Float16(const Float16& f)
	: binary16(f.binary16)
{
}

Float16::Float16(float val)
{
	uint32_t n = reinterpret_cast<uint32_t&>(val);

	uint16_t sign = uint16_t((n >> 16) & 0x00008000U);
	uint32_t exponent = n & 0x7f800000U;
	uint32_t mantissa = n & 0x007fffffU;

	if (exponent >= 0x47800000U) // overflow / NaN
	{
		if (exponent == 0x7f800000U && mantissa) // NaN
		{
			mantissa >>= 13;
			if (mantissa == 0) mantissa = 1;

			binary16 = sign | uint16_t(0x7c00U) | uint16_t(mantissa);
		}
		else // Inf / Overflow
		{
			binary16 = sign | uint16_t(0x7c00U);
		}
	}
	else if (exponent <= 0x38000000U) // underflow
	{
		if (exponent < 0x33000000U)
		{
			binary16 = sign;
		}
		else
		{
			exponent >>= 23;
			mantissa |= 0x00800000U;
			mantissa >>= (113 - exponent);
			mantissa |= 0x00001000U;
			binary16 = sign | uint16_t(mantissa >> 13);
		}
	}
	else // regular
	{
		exponent = exponent - 0x38000000U;
		mantissa |= 0x00001000U;
		binary16 = sign | uint16_t(exponent >> 13) | uint16_t(mantissa >> 13);
	}
}

Float16::operator float() const
{
	uint32_t sign = (binary16 >> 15) & 0x1U;
	uint32_t exponent = (binary16 >> 10) & 0x1fU;
	uint32_t mantissa = binary16 & 0x3ffU;

	if (exponent == 0)
	{
		if (mantissa)	// subnormal
		{
			exponent = 0x70U;
			mantissa <<= 1;
			while ((mantissa & 0x400U) == 0)
			{
				mantissa <<= 1;
				exponent -= 1;
			}
			mantissa &= 0x3ff;	// Clamp to 10 bits.
			mantissa = mantissa << 13;
		}
	}
	else if (exponent == 0x1fU)	// NaN or Inf
	{
		exponent = 0xffU;
		if (mantissa)	// NaN
			mantissa = mantissa << 13 | 0x1fffU;
	}
	else // Normalized
	{
		exponent = exponent + 0x70;
		mantissa = mantissa << 13;
	}

	union { float f; uint32_t n; };
	n = (sign << 31) | (exponent << 23) | mantissa;
	return f;
}

Float16& Float16::operator = (const Float16& v)
{
	binary16 = v.binary16;
	return *this;
}

Float16 Float16::abs() const
{
	return static_cast<Float16>(uint16_t(binary16 & 0x7fffU));
}

bool Float16::isInfinity() const
{
	uint32_t exponent = (binary16 >> 10) & 0x1fU;
	uint32_t mantissa = binary16 & 0x3ffU;
	return exponent == 0x1fU && mantissa == 0U;
}

bool Float16::isPositiveInfinity() const
{
	if (isInfinity())
		return isPositive();
	return false;
}

bool Float16::isNegativeInfinity() const
{
	if (isInfinity())
		return !isPositive();
	return false;
}

bool Float16::isNumeric() const
{
	uint32_t exponent = (binary16 >> 10) & 0x1fU;
	if (exponent == 0x1fU)
	{
		uint32_t mantissa = binary16 & 0x3ffU;
		if (mantissa)
			return false;
	}
	return true;
}

bool Float16::isSubnormalNumber() const
{
	uint32_t exponent = (binary16 >> 10) & 0x1fU;
	if (exponent == 0U)
	{
		uint32_t mantissa = binary16 & 0x3ffU;
		if (mantissa)
			return true;
	}
	return false;
}

bool Float16::isPositive() const
{
	return ((binary16 >> 15) & 0x1U) == 0;
}

bool Float16::isZero() const
{
	return (binary16 & 0x7fffU) == 0;
}

int Float16::compare(const Float16& rhs) const
{
	int sign1 = this->binary16 & 0x8000U;
	int sign2 = rhs.binary16 & 0x8000U;
	int v1 = this->binary16 & 0x7fffU;
	int v2 = rhs.binary16 & 0x7fffU;

	if (sign1)
		v1 = -v1;
	if (sign2)
		v2 = -v2;

	return v1 - v2;
}

std::partial_ordering Float16::operator <=> (const Float16& rhs) const
{
	if (isNumeric() && rhs.isNumeric())
	{
		if (isPositiveInfinity())
		{
			if (rhs.isPositiveInfinity())
				return std::partial_ordering::equivalent;
			return std::partial_ordering::greater;
		}
		if (isNegativeInfinity())
		{
			if (rhs.isNegativeInfinity())
				return std::partial_ordering::equivalent;
			return std::partial_ordering::less;
		}
		if (rhs.isNegativeInfinity())
			return std::partial_ordering::greater;
		if (rhs.isPositiveInfinity())
			return std::partial_ordering::less;

		auto n = compare(rhs);
		if (n > 0)
			return std::partial_ordering::greater;
		else if (n < 0)
			return std::partial_ordering::less;
		return std::partial_ordering::equivalent;
	}
	return std::partial_ordering::unordered;
}
