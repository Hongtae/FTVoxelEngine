#pragma once
#include "../include.h"
#include <algorithm>
#include "Vector3.h"
#include "Vector4.h"

#pragma pack(push, 4)
namespace FV {
    struct FVCORE_API Color {
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

        Color() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {
        }
        Color(const Color& c) : r(c.r), g(c.g), b(c.b), a(c.a) {
        }
        Color(float _r, float _g, float _b, float _a = 1.0f)
            : r(_r), g(_g), b(_b), a(_a) {
        }
        Color(RGBA32 rgba)
            : r(static_cast<float>(rgba.r) / 255.0f)
            , g(static_cast<float>(rgba.g) / 255.0f)
            , b(static_cast<float>(rgba.b) / 255.0f)
            , a(static_cast<float>(rgba.a) / 255.0f) {
        }
        Color(ARGB32 argb)
            : r(static_cast<float>(argb.r) / 255.0f)
            , g(static_cast<float>(argb.g) / 255.0f)
            , b(static_cast<float>(argb.b) / 255.0f)
            , a(static_cast<float>(argb.a) / 255.0f) {
        }
        explicit Color(const Vector3& v, float alpha = 1.0f)
            : r(v.x), g(v.y), b(v.z), a(alpha) {
        }
        explicit Color(const Vector4& v)
            : r(v.x), g(v.y), b(v.z), a(v.w) {
        }

        union {
            struct {
                float r, g, b, a;
            };
            float val[4];
        };

        RGBA32 RGBA32Value() const {
            RGBA32 val = {
                static_cast<uint8_t>(std::clamp<int>(static_cast<int>(r * 255.0f), 0, 0xff)),
                static_cast<uint8_t>(std::clamp<int>(static_cast<int>(g * 255.0f), 0, 0xff)),
                static_cast<uint8_t>(std::clamp<int>(static_cast<int>(b * 255.0f), 0, 0xff)),
                static_cast<uint8_t>(std::clamp<int>(static_cast<int>(a * 255.0f), 0, 0xff)),
            };
            return val;
        }

        ARGB32 ARGB32Value() const {
            ARGB32 val = {
                static_cast<uint8_t>(std::clamp<int>(static_cast<int>(a * 255.0f), 0, 0xff)),
                static_cast<uint8_t>(std::clamp<int>(static_cast<int>(r * 255.0f), 0, 0xff)),
                static_cast<uint8_t>(std::clamp<int>(static_cast<int>(g * 255.0f), 0, 0xff)),
                static_cast<uint8_t>(std::clamp<int>(static_cast<int>(b * 255.0f), 0, 0xff)),
            };
            return val;
        }

        bool operator==(const Color& c) const {
            return r == c.r && g == c.g && b == c.b && a == c.a;
        }

        Vector4 vector4() const { return Vector4(r, g, b, a); }

        // predefined values
        static const Color black;
        static const Color white;
        static const Color blue;
        static const Color brown;
        static const Color cyan;
        static const Color gray;
        static const Color darkGray;
        static const Color lightGray;
        static const Color green;
        static const Color magenta;
        static const Color orange;
        static const Color purple;
        static const Color red;
        static const Color yellow;
        static const Color clear;

        static const Color nonLinearRed;
        static const Color nonLinearOrange;
        static const Color nonLinearYellow;
        static const Color nonLinearGreen;
        static const Color nonLinearMint;
        static const Color nonLinearTeal;
        static const Color nonLinearCyan;
        static const Color nonLinearBlue;
        static const Color nonLinearIndigo;
        static const Color nonLinearPurple;
        static const Color nonLinearPink;
        static const Color nonLinearBrown;
        static const Color nonLinearGray;
    };
}
#pragma pack(pop)
