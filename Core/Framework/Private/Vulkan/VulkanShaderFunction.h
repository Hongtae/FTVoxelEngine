#pragma once
#include <memory>
#include <string>
#include <vector>
#include <map>
#include "../../ShaderFunction.h"
#include "../../ShaderModule.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV {
    class VulkanShaderModule;
    class VulkanShaderFunction : public ShaderFunction {
    public:
        VulkanShaderFunction(std::shared_ptr<VulkanShaderModule> module, const std::string& name, const ShaderSpecialization* values, size_t numValues);
        ~VulkanShaderFunction();

        const std::vector<ShaderAttribute>& stageInputAttributes() const override;
        const std::map<std::string, Constant>& functionConstants() const override { return functionConstantsMap; }
        std::string name() const override { return functionName; }
        ShaderStage stage() const override;

        std::shared_ptr<GraphicsDevice> device() const override;

        std::shared_ptr<VulkanShaderModule> module;
        std::string functionName;

        std::vector<ShaderAttribute> inputAttributes;
        VkSpecializationInfo specializationInfo;
        void* specializationData;

        std::map<std::string, Constant> functionConstantsMap;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
