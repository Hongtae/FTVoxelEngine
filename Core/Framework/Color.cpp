#include "Color.h"

using namespace FV;

const Color Color::black = Color(0.0, 0.0, 0.0);
const Color Color::white = Color(1.0, 1.0, 1.0);
const Color Color::blue = Color(0.0, 0.0, 1.0);
const Color Color::brown = Color(0.6, 0.4, 0.2);
const Color Color::cyan = Color(0.0, 1.0, 1.0);
const Color Color::gray = Color(0.5, 0.5, 0.5);
const Color Color::darkGray = Color(0.3, 0.3, 0.3);
const Color Color::lightGray = Color(0.6, 0.6, 0.6);
const Color Color::green = Color(0.0, 1.0, 0.0);
const Color Color::magenta = Color(1.0, 0.0, 1.0);
const Color Color::orange = Color(1.0, 0.5, 0.0);
const Color Color::purple = Color(0.5, 0.0, 0.5);
const Color Color::red = Color(1.0, 0.0, 0.0);
const Color Color::yellow = Color(1.0, 1.0, 0.0);
const Color Color::clear = Color(0, 0, 0, 0);

// sRGB to linear
const Color Color::nonLinearRed = Color(1, 0.231373, 0.188235);
const Color Color::nonLinearOrange = Color(1, 0.584314, 0);
const Color Color::nonLinearYellow = Color(1, 0.8, 0);
const Color Color::nonLinearGreen = Color(0.156863, 0.803922, 0.254902);
const Color Color::nonLinearMint = Color(0, 0.780392, 0.745098);
const Color Color::nonLinearTeal = Color(0.34902, 0.678431, 0.768627);
const Color Color::nonLinearCyan = Color(0.333333, 0.745098, 0.941176);
const Color Color::nonLinearBlue = Color(0, 0.478431, 1);
const Color Color::nonLinearIndigo = Color(0.345098, 0.337255, 0.839216);
const Color Color::nonLinearPurple = Color(0.686275, 0.321569, 0.870588);
const Color Color::nonLinearPink = Color(1, 0.176471, 0.333333);
const Color Color::nonLinearBrown = Color(0.635294, 0.517647, 0.368627);
const Color Color::nonLinearGray = Color(0.556863, 0.556863, 0.576471);
