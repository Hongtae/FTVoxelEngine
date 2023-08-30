#include "Color.h"

using namespace FV;

const Color Color::black = Color(0.0, 0.0, 0.0);
const Color Color::white = Color(1.0, 1.0, 1.0);
const Color Color::blue = Color(0.0, 0.0, 1.0);
const Color Color::brown = Color(0.6f, 0.4f, 0.2f);
const Color Color::cyan = Color(0.0, 1.0, 1.0);
const Color Color::gray = Color(0.5, 0.5, 0.5);
const Color Color::darkGray = Color(0.3f, 0.3f, 0.3f);
const Color Color::lightGray = Color(0.6f, 0.6f, 0.6f);
const Color Color::green = Color(0.0, 1.0, 0.0);
const Color Color::magenta = Color(1.0, 0.0, 1.0);
const Color Color::orange = Color(1.0, 0.5, 0.0);
const Color Color::purple = Color(0.5, 0.0, 0.5);
const Color Color::red = Color(1.0, 0.0, 0.0);
const Color Color::yellow = Color(1.0, 1.0, 0.0);
const Color Color::clear = Color(0, 0, 0, 0);

// sRGB to linear
const Color Color::nonLinearRed = Color(1, 0.231373f, 0.188235f);
const Color Color::nonLinearOrange = Color(1, 0.584314f, 0);
const Color Color::nonLinearYellow = Color(1, 0.8f, 0);
const Color Color::nonLinearGreen = Color(0.156863f, 0.803922f, 0.254902f);
const Color Color::nonLinearMint = Color(0, 0.780392f, 0.745098f);
const Color Color::nonLinearTeal = Color(0.34902f, 0.678431f, 0.768627f);
const Color Color::nonLinearCyan = Color(0.333333f, 0.745098f, 0.941176f);
const Color Color::nonLinearBlue = Color(0, 0.478431f, 1);
const Color Color::nonLinearIndigo = Color(0.345098f, 0.337255f, 0.839216f);
const Color Color::nonLinearPurple = Color(0.686275f, 0.321569f, 0.870588f);
const Color Color::nonLinearPink = Color(1, 0.176471f, 0.333333f);
const Color Color::nonLinearBrown = Color(0.635294f, 0.517647f, 0.368627f);
const Color Color::nonLinearGray = Color(0.556863f, 0.556863f, 0.576471f);
