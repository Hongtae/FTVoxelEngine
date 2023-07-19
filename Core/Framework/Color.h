#pragma once
#include "../include.h"
#include <algorithm>
#include "Vector4.h"

#pragma pack(push, 4)
namespace FV
{
    struct Color
    {
		union RGBA32  ///< 32bit int format (RGBA order).
		{
			struct {
				uint8_t r, g, b, a;
			};
			uint8_t bytes[4];
			uint32_t value;
		};
		union ARGB32  ///< 32bit int format (ARGB order).
		{
			struct {
				uint8_t a, r, g, b;
			};
			uint8_t bytes[4];
			uint32_t value;
		};

		union
		{
			struct {
				float r, g, b, a;
			};
			float val[4];
		};

		RGBA32 RGBA32Value() const
		{
			RGBA32 val = {
				static_cast<uint8_t>(std::clamp<int>(static_cast<int>(r * 255.0f), 0, 0xff)),
				static_cast<uint8_t>(std::clamp<int>(static_cast<int>(g * 255.0f), 0, 0xff)),
				static_cast<uint8_t>(std::clamp<int>(static_cast<int>(b * 255.0f), 0, 0xff)),
				static_cast<uint8_t>(std::clamp<int>(static_cast<int>(a * 255.0f), 0, 0xff)),
			};
			return val;
		}

		ARGB32 ARGB32Value() const
		{
			ARGB32 val = {
				static_cast<uint8_t>(std::clamp<int>(static_cast<int>(a * 255.0f), 0, 0xff)),
				static_cast<uint8_t>(std::clamp<int>(static_cast<int>(r * 255.0f), 0, 0xff)),
				static_cast<uint8_t>(std::clamp<int>(static_cast<int>(g * 255.0f), 0, 0xff)),
				static_cast<uint8_t>(std::clamp<int>(static_cast<int>(b * 255.0f), 0, 0xff)),
			};
			return val;
		}

		bool operator==(const Color& c) const
		{
			return r == c.r && g == c.g && b == c.b && a == c.a;
		}

		Vector4 vector4() const { return Vector4(r, g, b, a); }

    };
}
#pragma pack(pop)
