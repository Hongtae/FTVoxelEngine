#pragma once
#include "../include.h"
#include "ShaderFunction.h"

namespace FV
{
	struct ShaderSpecialization
	{
		ShaderDataType type;
		const void* data;
		uint32_t index;
		size_t size;
	};

	class ShaderModule
	{
	public:
		virtual ~ShaderModule() {}

		virtual std::shared_ptr<ShaderFunction> createFunction(const std::string& name) const = 0;
		virtual std::shared_ptr<ShaderFunction> createSpecializedFunction(const std::string& name, const ShaderSpecialization* values, size_t numValues) const = 0;
		virtual const std::vector<std::string>& functionNames() const = 0;

		virtual std::shared_ptr<GraphicsDevice> device() const = 0;
	};
}
