#include <limits>
#include "Rect.h"

using namespace FV;

const Point Point::zero = Point(0, 0);

const Size Size::zero = Size(0, 0);

const Rect Rect::zero = Rect(0, 0, 0, 0);

const Rect Rect::null = Rect(std::numeric_limits<float>::infinity(),
                             std::numeric_limits<float>::infinity(),
                             0, 0);

const Rect Rect::infinite = Rect(0, 0,
                                 std::numeric_limits<float>::infinity(),
                                 std::numeric_limits<float>::infinity());

bool Rect::isNull() const {
    return
        origin.x == std::numeric_limits<float>::infinity() ||
        origin.y == std::numeric_limits<float>::infinity();
}

bool Rect::isInfinite() const {
    return (*this) == infinite;
}

bool Rect::isEmpty() const {
    return isNull() || size.width == 0 || size.height == 0;
}

bool Rect::contains(const Point& pt) const {
    if (isNull() || isEmpty()) return false;

    return pt.x >= minX() && pt.x < maxX() &&
        pt.y >= minY() && pt.y < maxY();
}

