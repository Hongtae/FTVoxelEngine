#include "../Libs/SPIRV-Cross/spirv_cross.hpp"
#include "../Libs/SPIRV-Cross/spirv_common.hpp"
#include "Shader.h"

using namespace FV;

Shader::Shader()
    : stage(ShaderStage::Unknown)
    , threadgroupSize({ 1, 1, 1 })
{
}

Shader::Shader(const uint32_t* data, size_t length)
    : Shader()
{
    compile();
}

Shader::~Shader()
{
}

bool Shader::compile()
{
    return false;
}

bool Shader::validate()
{
    return stage != ShaderStage::Unknown && data.empty() == false;
}
