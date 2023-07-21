#include "../Libs/SPIRV-Cross/spirv_cross.hpp"
#include "../Libs/SPIRV-Cross/spirv_common.hpp"
#include "Shader.h"

using namespace FV;

Shader::Shader()
    : _stage(ShaderStage::Unknown)
    , _threadgroupSize({ 1, 1, 1 })
{
}

Shader::~Shader()
{
}

bool Shader::compile(const std::vector<uint32_t>& data)
{
    return false;
}

bool Shader::validate()
{
    return isValid();
}

bool Shader::isValid() const
{
    return _stage != ShaderStage::Unknown && _data.empty() == false;
}
