#pragma once
#include "../include.h"
#include <compare>

#pragma pack(push, 2)
namespace FV {
    /**
     @brief Half precision floating point type.

     @verbatim
     binary16 layout (IEEE 754-2008)
      +-------+----------+---------------------+
      | sign  | exponent | fraction (mantissa) |
      +-------+----------+---------------------+
      | 1 bit | 5 bit    | 10 bit              |
      +-------+----------+---------------------+
     @endverbatim
     */
    class FVCORE_API Float16 {
    public:
        Float16();
        Float16(const Float16&);
        explicit Float16(float);

        operator float() const;
        Float16& operator = (const Float16&);

        Float16 abs() const;
        bool isInfinity() const;
        bool isPositiveInfinity() const;
        bool isNegativeInfinity() const;
        bool isNumeric() const;	///< false if NaN
        bool isSubnormalNumber() const;
        bool isPositive() const;
        bool isZero() const;

        /// @warning
        /// compare function does not check Inf,NaN
        int compare(const Float16&) const;

        static const Float16 zero;			///< 0.0 (0 for positive, 0x8000 for negative)
        static const Float16 max;			///< maximum positive value (65504.0)
        static const Float16 min;			///< minimum positive normal (2^-14)
        static const Float16 maxSubnormal;	///< minimum positive subnormal (2^-14 - 2^-24)
        static const Float16 minSubnormal;	///< minimum positive subnormal (2^-24)
        static const Float16 posInfinity;	///< +Inf (0x7c00)
        static const Float16 negInfinity;	///< -Inf (0xfc00)

        std::partial_ordering operator <=> (const Float16&) const;
    private:
        uint16_t binary16;	// packed 16bit float
    };

    static_assert(sizeof(Float16) == 2, "float16 should be 2 bytes!");

    //inline Float16 abs(Float16 f) { return f.abs(); }
}
#pragma pack(pop)
