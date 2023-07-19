#pragma once
#include "../include.h"

#pragma pack(push, 4)
namespace FV
{
    struct FVCORE_API Point
    {
        float x, y;

        static const Point zero;
    };

    struct FVCORE_API Size
    {
        float width, height;

        static const Size zero;
    };

    struct FVCORE_API Rect
    {
        Point origin;
        Size size;

        static const Rect zero;
    };
}
#pragma pack(pop)
