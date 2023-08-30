#include "../../../Libs/SPIRV-Cross/spirv_cross.hpp"
#include "Extensions.h"
#include "ShaderFunction.h"
#include "ShaderModule.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

ShaderFunction::ShaderFunction(std::shared_ptr<ShaderModule> m, const std::string& name, const ShaderSpecialization* values, size_t numValues)
	: module(m)
	, functionName(name)
	, specializationData(NULL)
	, specializationInfo({ 0 })
{
	if (values && numValues)
	{
		size_t size = 0;
		for (size_t i = 0; i < numValues; ++i)
		{
			const ShaderSpecialization& sp = values[i];
			size += sp.size;
		}

		if (size > 0)
		{
			specializationData = malloc(size + sizeof(VkSpecializationMapEntry) * numValues);
			VkSpecializationMapEntry* mapEntries = reinterpret_cast<VkSpecializationMapEntry*>(specializationData);
			uint8_t* data = reinterpret_cast<uint8_t*>(&mapEntries[numValues]);

			specializationInfo.mapEntryCount = (uint32_t)numValues;
			specializationInfo.pMapEntries = mapEntries;
			specializationInfo.pData = data;

			size_t offset = 0;
			for (size_t i = 0; i < numValues; ++i)
			{
				const ShaderSpecialization& sp = values[i];
				mapEntries[i].constantID = sp.index;
				mapEntries[i].offset = (uint32_t)offset;
				mapEntries[i].size = sp.size;

				memcpy(&data[offset], sp.data, sp.size);
				offset += sp.size;
			}
		}
	}
}

ShaderFunction::~ShaderFunction()
{
	if (specializationData)
		free(specializationData);
}

const std::vector<FV::ShaderAttribute>& ShaderFunction::stageInputAttributes() const
{
	return module->inputAttributes;
}

FV::ShaderStage ShaderFunction::stage() const
{
	switch (module->stage)
	{
	case VK_SHADER_STAGE_VERTEX_BIT:
		return ShaderStage::Vertex;
	case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		return ShaderStage::TessellationControl;
	case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		return ShaderStage::TessellationEvaluation;
	case VK_SHADER_STAGE_GEOMETRY_BIT:
		return ShaderStage::Geometry;
	case VK_SHADER_STAGE_FRAGMENT_BIT:
		return ShaderStage::Fragment;
	case VK_SHADER_STAGE_COMPUTE_BIT:
		return ShaderStage::Compute;
	}
	return ShaderStage::Unknown;
}

std::shared_ptr<FV::GraphicsDevice> ShaderFunction::device() const
{
	return module->device();
}

#endif //#if FVCORE_ENABLE_VULKAN
