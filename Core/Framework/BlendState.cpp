#include "BlendState.h"

using namespace FV;

const BlendState BlendState::opaque {
    false,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::Zero,
    BlendFactor::Zero,
    BlendOperation::Add
};

const BlendState BlendState::alphaBlend {
    true,
    BlendFactor::SourceAlpha,
    BlendFactor::OneMinusDestinationAlpha,
    BlendFactor::OneMinusSourceAlpha,
    BlendFactor::One,
    BlendOperation::Add,
    BlendOperation::Add
};

const BlendState BlendState::multiply {
    true,
    BlendFactor::Zero,
    BlendFactor::Zero,
    BlendFactor::SourceColor,
    BlendFactor::SourceColor,
    BlendOperation::Add,
    BlendOperation::Add
};

const BlendState BlendState::screen {
    true,
    BlendFactor::OneMinusDestinationColor,
    BlendFactor::OneMinusDestinationColor,
    BlendFactor::One,
    BlendFactor::One,
    BlendOperation::Add,
    BlendOperation::Add
};

const BlendState BlendState::darken {
    true,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::One,
    BlendOperation::Min,
    BlendOperation::Min
};

const BlendState BlendState::lighten {
    true,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::One,
    BlendOperation::Max,
    BlendOperation::Max
};

const BlendState BlendState::linearBurn {
    true,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::OneMinusDestinationColor,
    BlendFactor::OneMinusDestinationColor,
    BlendOperation::Subtract,
    BlendOperation::Subtract
};

const BlendState BlendState::linearDodge {
    true,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::One,
    BlendOperation::Add,
    BlendOperation::Add
};
