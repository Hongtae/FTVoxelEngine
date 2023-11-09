#pragma once
#include "../include.h"
#include "Vector2.h"

#ifndef NOMINMAX
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#endif
#pragma pack(push, 4)
namespace FV {
    struct FVCORE_API Point {
        float x, y;

        static const Point zero;

        Point() : x(0.0f), y(0.0f) {}
        Point(float _x, float _y) : x(_x), y(_y) {}

        Point operator + (const Point& p) const { return { x + p.x, y + p.y }; }
        Point operator - (const Point& p) const { return { x - p.x, y - p.y }; }
        Point operator * (const Point& p) const { return { x * p.x, y * p.y }; }
        Point operator / (const Point& p) const { return { x / p.x, y / p.y }; }
        Point operator * (float f) const { return { x * f, y * f }; }
        Point operator / (float f) const { return (*this) * (1.0f / f); }
        Point operator - () const { return { -x, -y }; }

        Point& operator += (const Point& p) { *this = (*this) + p; return *this; }
        Point& operator -= (const Point& p) { *this = (*this) - p; return *this; }
        Point& operator *= (const Point& p) { *this = (*this) * p; return *this; }
        Point& operator /= (const Point& p) { *this = (*this) * p; return *this; }
        Point& operator *= (float f) { *this = (*this) * f; return *this; }
        Point& operator /= (float f) { *this = (*this) * f; return *this; }

        bool operator==(const Point& p) const { return x == p.x && y == p.y; }

        Vector2 vector2() const { return { x, y }; }
    };

    struct FVCORE_API Size {
        float width, height;

        static const Size zero;

        Size() : width(0.0f), height(0.0f) {}
        Size(float w, float h) : width(w), height(h) {}

        Size operator + (const Size& s) const { return { width + s.width, height + s.height }; }
        Size operator - (const Size& s) const { return { width - s.width, height - s.height }; }
        Size operator * (const Size& s) const { return { width * s.width, height * s.height }; }
        Size operator / (const Size& s) const { return { width / s.width, height / s.height }; }
        Size operator * (float f) const { return { width * f, height * f }; }
        Size operator / (float f) const { return (*this) * (1.0f / f); }
        Size operator - () const { return { -width, -height }; }

        Size& operator += (const Size& s) { *this = (*this) + s; return *this; }
        Size& operator -= (const Size& s) { *this = (*this) - s; return *this; }
        Size& operator *= (const Size& s) { *this = (*this) * s; return *this; }
        Size& operator /= (const Size& s) { *this = (*this) * s; return *this; }
        Size& operator *= (float f) { *this = (*this) * f; return *this; }
        Size& operator /= (float f) { *this = (*this) * f; return *this; }

        bool operator==(const Size& s) const { return width == s.width && height == s.height; }

        Vector2 vector2() const { return { width, height }; }
    };

    struct FVCORE_API Rect {
        Point origin;
        Size size;

        static const Rect zero;
        static const Rect null;
        static const Rect infinite;

        Rect() {}
        Rect(float x, float y, float width, float height)
            : origin(x, y), size(width, height) {}
        Rect(const Point& o, const Size& s) : origin(o), size(s) {}

        float width() const     { return std::abs(size.width); }
        float height() const    { return std::abs(size.height); }

        float minX() const { return origin.x + std::min(size.width, 0.0f); }
        float maxX() const { return origin.x + std::max(size.width, 0.0f); }
        float midX() const { return (minX() + maxX()) * 0.5f; }

        float minY() const { return origin.y + std::min(size.height, 0.0f); }
        float maxY() const { return origin.y + std::max(size.height, 0.0f); }
        float midY() const { return (minY() + maxY()) * 0.5f; }

        bool operator==(const Rect& r) const { return origin == r.origin && size == r.size; }

        bool isNull() const;
        bool isInfinite() const;
        bool isEmpty() const;

        bool contains(const Point& pt) const;
    };
}
#pragma pack(pop)
#ifndef NOMINMAX
#pragma pop_macro("min")
#pragma pop_macro("max")
#endif