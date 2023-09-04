#pragma once
#include "../include.h"
#include "ShaderFunction.h"

namespace FV {
    struct ShaderSpecialization {
        ShaderDataType type;
        const void* data;
        uint32_t index;
        size_t size;
    };

    class ShaderModule {
    public:
        virtual ~ShaderModule() {}

        virtual std::shared_ptr<ShaderFunction> makeFunction(const std::string& name) = 0;
        virtual std::shared_ptr<ShaderFunction> makeSpecializedFunction(const std::string& name, const ShaderSpecialization* values, size_t numValues) = 0;

        virtual const std::vector<std::string>& functionNames() const = 0;
        virtual std::shared_ptr<GraphicsDevice> device() const = 0;
    };
}
