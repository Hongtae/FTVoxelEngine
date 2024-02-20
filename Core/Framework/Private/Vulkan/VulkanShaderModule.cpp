#include "VulkanExtensions.h"
#include "VulkanTypes.h"
#include "VulkanShaderModule.h"
#include "VulkanShaderFunction.h"
#include "VulkanGraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV;

VulkanShaderModule::VulkanShaderModule(std::shared_ptr<VulkanGraphicsDevice> d, VkShaderModule m, const Shader& s)
    : gdevice(d)
    , module(m) {
    switch (s.stage()) {
    case ShaderStage::Vertex:
        stage = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case ShaderStage::TessellationControl:
        stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        break;
    case ShaderStage::TessellationEvaluation:
        stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    case ShaderStage::Geometry:
        stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        break;
    case ShaderStage::Fragment:
        stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case ShaderStage::Compute:
        stage = VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    default:
        FVASSERT_DEBUG(0);
        break;
    }

    fnNames = s.functions();
    pushConstantLayouts = s.pushConstantLayouts();
    descriptors = s.descriptors();
    inputAttributes = s.inputAttributes();
    resources = s.resources();
}

VulkanShaderModule::~VulkanShaderModule() {
    vkDestroyShaderModule(gdevice->device, module, gdevice->allocationCallbacks());
}

std::shared_ptr<ShaderFunction> VulkanShaderModule::makeFunction(const std::string& name) {
    if (auto it = std::find_if(fnNames.begin(), fnNames.end(),
                               [&](const std::string& fn) {
                                   return fn.compare(name) == 0;
                               }); it != fnNames.end()) {
        return std::make_shared<VulkanShaderFunction>(shared_from_this(), name);
    }
    return nullptr;
}

std::shared_ptr<ShaderFunction> VulkanShaderModule::makeSpecializedFunction(const std::string& name,
                                                                            const std::vector<ShaderSpecialization>& values) {
    if (values.empty() == false) {
        // TODO: verify values with SPIR-V Cross reflection

        if (auto it = std::find_if(fnNames.begin(), fnNames.end(),
                                   [&](const std::string& fn) {
                                       return fn.compare(name) == 0;
                                   }); it != fnNames.end()) {
            return std::make_shared<VulkanShaderFunction>(shared_from_this(), name, values);
        }
    }
    return nullptr;
}

std::shared_ptr<GraphicsDevice> VulkanShaderModule::device() const {
    return gdevice;
}

#endif //#if FVCORE_ENABLE_VULKAN
