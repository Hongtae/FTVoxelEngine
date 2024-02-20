#pragma once
#include <memory>
#include <vector>
#include "../../ShaderModule.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanShaderModule : public ShaderModule, public std::enable_shared_from_this<VulkanShaderModule> {
    public:
        VulkanShaderModule(std::shared_ptr<VulkanGraphicsDevice>, VkShaderModule, const Shader&);
        ~VulkanShaderModule();

        std::shared_ptr<ShaderFunction> makeFunction(const std::string& name) override;
        std::shared_ptr<ShaderFunction> makeSpecializedFunction(const std::string& name, const std::vector<ShaderSpecialization>& values) override;

        const std::vector<std::string>& functionNames() const override { return fnNames; }
        std::shared_ptr<GraphicsDevice> device() const override;

        std::vector<std::string> fnNames;
        std::shared_ptr<VulkanGraphicsDevice> gdevice;
        VkShaderModule module;
        VkShaderStageFlagBits stage;

        std::vector<ShaderAttribute> inputAttributes;
        std::vector<ShaderPushConstantLayout> pushConstantLayouts;
        std::vector<ShaderResource> resources;
        std::vector<ShaderDescriptor> descriptors;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
