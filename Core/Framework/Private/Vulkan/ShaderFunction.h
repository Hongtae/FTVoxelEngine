#pragma once
#include <memory>
#include <string>
#include <vector>
#include <map>
#include "../../ShaderFunction.h"
#include "../../ShaderModule.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV::Vulkan {
    class ShaderModule;
    class ShaderFunction : public FV::ShaderFunction {
    public:
        ShaderFunction(std::shared_ptr<ShaderModule> module, const std::string& name, const ShaderSpecialization* values, size_t numValues);
        ~ShaderFunction();

        const std::vector<ShaderAttribute>& stageInputAttributes() const override;
        const std::map<std::string, Constant>& functionConstants() const override { return functionConstantsMap; }
        std::string name() const override { return functionName; }
        ShaderStage stage() const override;

        std::shared_ptr<FV::GraphicsDevice> device() const override;

        std::shared_ptr<ShaderModule> module;
        std::string functionName;

        std::vector<ShaderAttribute> inputAttributes;
        VkSpecializationInfo specializationInfo;
        void* specializationData;

        std::map<std::string, Constant> functionConstantsMap;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
