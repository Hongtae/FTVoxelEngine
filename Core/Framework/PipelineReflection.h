#pragma once
#include "../include.h"
#include <vector>
#include "ShaderResource.h"

namespace FV
{
	struct PipelineReflection
	{
		std::vector<ShaderAttribute> inputAttributes;
		std::vector<ShaderPushConstantLayout> pushConstantLayouts;
		std::vector<ShaderResource> resources;
	};
}
