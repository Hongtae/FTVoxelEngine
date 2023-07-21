#pragma once
#include "../include.h"
#include "Shader.h"
#include <vector>
#include <map>

namespace FV
{
	class GraphicsDevice;
	class ShaderFunction
	{
	public:
		virtual ~ShaderFunction() {}

		struct Constant
		{
			std::string name;
			ShaderDataType type;
			uint32_t index;
			bool required;
		};

		virtual const std::vector<ShaderAttribute>& stageInputAttributes() const = 0;
		virtual const std::map<std::string, Constant>& functionConstants() const = 0;
		virtual std::string name() const = 0;
		virtual ShaderStage stage() const = 0;

		virtual std::shared_ptr<GraphicsDevice> device() const = 0;
	};
}
