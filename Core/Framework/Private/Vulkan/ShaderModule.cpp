#include "Extensions.h"
#include "Types.h"
#include "ShaderModule.h"
#include "ShaderFunction.h"
#include "GraphicsDevice.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

ShaderModule::ShaderModule(std::shared_ptr<GraphicsDevice> d, VkShaderModule m, const FV::Shader& s)
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

ShaderModule::~ShaderModule() {
    vkDestroyShaderModule(gdevice->device, module, gdevice->allocationCallbacks());
}

std::shared_ptr<FV::ShaderFunction> ShaderModule::makeFunction(const std::string& name) {
    if (auto it = std::find_if(fnNames.begin(), fnNames.end(),
                               [&](const std::string& fn) {
                                   return fn.compare(name) == 0;
                               }); it != fnNames.end()) {
        return std::make_shared<ShaderFunction>(shared_from_this(), name, nullptr, 0);
    }
    return nullptr;
}

std::shared_ptr<FV::ShaderFunction> ShaderModule::makeSpecializedFunction(const std::string& name, const ShaderSpecialization* values, size_t numValues) {
    if (values && numValues > 0) {
        // TODO: verify values with SPIR-V Cross reflection

        if (auto it = std::find_if(fnNames.begin(), fnNames.end(),
                                   [&](const std::string& fn) {
                                       return fn.compare(name) == 0;
                                   }); it != fnNames.end()) {
            return std::make_shared<ShaderFunction>(shared_from_this(), name, values, numValues);
        }
    }
    return nullptr;
}

std::shared_ptr<FV::GraphicsDevice> ShaderModule::device() const {
    return gdevice;
}

#endif //#if FVCORE_ENABLE_VULKAN
