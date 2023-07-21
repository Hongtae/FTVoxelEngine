#pragma once
#include <memory>
#include <vector>
#include "../../ShaderModule.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV::Vulkan
{
	class GraphicsDevice;
	class ShaderModule : public FV::ShaderModule, public std::enable_shared_from_this<ShaderModule>
	{
	public:
		ShaderModule(std::shared_ptr<GraphicsDevice>, VkShaderModule, const FV::Shader&);
		~ShaderModule();

		std::shared_ptr<FV::ShaderFunction> makeFunction(const std::string& name) override;
		std::shared_ptr<FV::ShaderFunction> makeSpecializedFunction(const std::string& name, const ShaderSpecialization* values, size_t numValues) override;

		const std::vector<std::string>& functionNames() const override { return fnNames; }
		std::shared_ptr<FV::GraphicsDevice> device() const override;

		std::vector<std::string> fnNames;
		std::shared_ptr<GraphicsDevice> gdevice;
		VkShaderModule module;
		VkShaderStageFlagBits stage;

        std::vector<ShaderAttribute> inputAttributes;
        std::vector<ShaderPushConstantLayout> pushConstantLayouts;
        std::vector<ShaderResource> resources;
        std::vector<ShaderDescriptor> descriptors;
	};
}
#endif //#if FVCORE_ENABLE_VULKAN
