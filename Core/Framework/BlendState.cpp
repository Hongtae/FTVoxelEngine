#include "BlendState.h"

using namespace FV;

const BlendState BlendState::defaultOpaque {
    false,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::Zero,
    BlendFactor::Zero,
    BlendOperation::Add
};

const BlendState BlendState::defaultAlpha {
    true,
    BlendFactor::SourceAlpha,
    BlendFactor::OneMinusDestinationAlpha,
    BlendFactor::OneMinusSourceAlpha,
    BlendFactor::One,
    BlendOperation::Add,
    BlendOperation::Add
};

const BlendState BlendState::defaultMultiply {
    true,
    BlendFactor::Zero,
    BlendFactor::Zero,
    BlendFactor::SourceColor,
    BlendFactor::SourceColor,
    BlendOperation::Add,
    BlendOperation::Add
};

const BlendState BlendState::defaultScreen {
    true,
    BlendFactor::OneMinusDestinationColor,
    BlendFactor::OneMinusDestinationColor,
    BlendFactor::One,
    BlendFactor::One,
    BlendOperation::Add,
    BlendOperation::Add
};

const BlendState BlendState::defaultDarken {
    true,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::One,
    BlendOperation::Min,
    BlendOperation::Min
};

const BlendState BlendState::defaultLighten {
    true,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::One,
    BlendOperation::Max,
    BlendOperation::Max
};

const BlendState BlendState::defaultLinearBurn {
    true,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::OneMinusDestinationColor,
    BlendFactor::OneMinusDestinationColor,
    BlendOperation::Subtract,
    BlendOperation::Subtract
};

const BlendState BlendState::defaultLinearDodge {
    true,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::One,
    BlendFactor::One,
    BlendOperation::Add,
    BlendOperation::Add
};
