#pragma once
#include <string>
#include <vector>
#include <FVCore.h>


inline std::string shaderStageNames(uint32_t s)
{
    std::vector<const char*> stages;
    if (s & (uint32_t)FV::ShaderStage::Vertex)
        stages.push_back("Vertex");
    if (s & (uint32_t)FV::ShaderStage::TessellationControl)
        stages.push_back("TessCtrl");
    if (s & (uint32_t)FV::ShaderStage::TessellationEvaluation)
        stages.push_back("TessEval");
    if (s & (uint32_t)FV::ShaderStage::Geometry)
        stages.push_back("Geometry");
    if (s & (uint32_t)FV::ShaderStage::Fragment)
        stages.push_back("Fragment");
    if (s & (uint32_t)FV::ShaderStage::Compute)
        stages.push_back("Compute");

    if (stages.empty())
        return "";

    std::string str = stages.at(0);
    for (int i = 1; i < stages.size(); ++i)
        str += std::format(", {}", stages.at(i));
    return str;
}

inline const char* shaderDataTypeString(FV::ShaderDataType t)
{
    switch (t) {
    case FV::ShaderDataType::Unknown:		return "Unknown";
    case FV::ShaderDataType::None:			return "None";

    case FV::ShaderDataType::Struct:		return "Struct";
    case FV::ShaderDataType::Texture:		return "Texture";
    case FV::ShaderDataType::Sampler:		return "Sampler";

    case FV::ShaderDataType::Bool:          return "Bool";
    case FV::ShaderDataType::Bool2:         return "Bool2";
    case FV::ShaderDataType::Bool3:         return "Bool3";
    case FV::ShaderDataType::Bool4:         return "Bool4";

    case FV::ShaderDataType::Char:          return "Char";
    case FV::ShaderDataType::Char2:         return "Char2";
    case FV::ShaderDataType::Char3:         return "Char3";
    case FV::ShaderDataType::Char4:         return "Char4";

    case FV::ShaderDataType::UChar:         return "UChar";
    case FV::ShaderDataType::UChar2:        return "UChar2";
    case FV::ShaderDataType::UChar3:        return "UChar3";
    case FV::ShaderDataType::UChar4:        return "UChar4";

    case FV::ShaderDataType::Short:         return "Short";
    case FV::ShaderDataType::Short2:        return "Short2";
    case FV::ShaderDataType::Short3:        return "Short3";
    case FV::ShaderDataType::Short4:        return "Short4";

    case FV::ShaderDataType::UShort:        return "UShort";
    case FV::ShaderDataType::UShort2:       return "UShort2";
    case FV::ShaderDataType::UShort3:       return "UShort3";
    case FV::ShaderDataType::UShort4:       return "UShort4";

    case FV::ShaderDataType::Int:           return "Int";
    case FV::ShaderDataType::Int2:          return "Int2";
    case FV::ShaderDataType::Int3:          return "Int3";
    case FV::ShaderDataType::Int4:          return "Int4";

    case FV::ShaderDataType::UInt:          return "UInt";
    case FV::ShaderDataType::UInt2:         return "UInt2";
    case FV::ShaderDataType::UInt3:         return "UInt3";
    case FV::ShaderDataType::UInt4:         return "UInt4";

    case FV::ShaderDataType::Long:          return "Long";
    case FV::ShaderDataType::Long2:         return "Long2";
    case FV::ShaderDataType::Long3:         return "Long3";
    case FV::ShaderDataType::Long4:         return "Long4";

    case FV::ShaderDataType::ULong:         return "ULong";
    case FV::ShaderDataType::ULong2:        return "ULong2";
    case FV::ShaderDataType::ULong3:        return "ULong3";
    case FV::ShaderDataType::ULong4:        return "ULong4";

    case FV::ShaderDataType::Half:          return "Half";
    case FV::ShaderDataType::Half2:         return "Half2";
    case FV::ShaderDataType::Half3:         return "Half3";
    case FV::ShaderDataType::Half4:         return "Half4";
    case FV::ShaderDataType::Half2x2:       return "Half2x2";
    case FV::ShaderDataType::Half3x2:       return "Half3x2";
    case FV::ShaderDataType::Half4x2:       return "Half4x2";
    case FV::ShaderDataType::Half2x3:       return "Half2x3";
    case FV::ShaderDataType::Half3x3:       return "Half3x3";
    case FV::ShaderDataType::Half4x3:       return "Half4x3";
    case FV::ShaderDataType::Half2x4:       return "Half2x4";
    case FV::ShaderDataType::Half3x4:       return "Half3x4";
    case FV::ShaderDataType::Half4x4:       return "Half4x4";

    case FV::ShaderDataType::Float:         return "Float";
    case FV::ShaderDataType::Float2:        return "Float2";
    case FV::ShaderDataType::Float3:        return "Float3";
    case FV::ShaderDataType::Float4:        return "Float4";
    case FV::ShaderDataType::Float2x2:      return "Float2x2";
    case FV::ShaderDataType::Float3x2:      return "Float3x2";
    case FV::ShaderDataType::Float4x2:      return "Float4x2";
    case FV::ShaderDataType::Float2x3:      return "Float2x3";
    case FV::ShaderDataType::Float3x3:      return "Float3x3";
    case FV::ShaderDataType::Float4x3:      return "Float4x3";
    case FV::ShaderDataType::Float2x4:      return "Float2x4";
    case FV::ShaderDataType::Float3x4:      return "Float3x4";
    case FV::ShaderDataType::Float4x4:      return "Float4x4";

    case FV::ShaderDataType::Double:        return "Double";
    case FV::ShaderDataType::Double2:       return "Double2";
    case FV::ShaderDataType::Double3:       return "Double3";
    case FV::ShaderDataType::Double4:       return "Double4";
    case FV::ShaderDataType::Double2x2:     return "Double2x2";
    case FV::ShaderDataType::Double3x2:     return "Double3x2";
    case FV::ShaderDataType::Double4x2:     return "Double4x2";
    case FV::ShaderDataType::Double2x3:     return "Double2x3";
    case FV::ShaderDataType::Double3x3:     return "Double3x3";
    case FV::ShaderDataType::Double4x3:     return "Double4x3";
    case FV::ShaderDataType::Double2x4:     return "Double2x4";
    case FV::ShaderDataType::Double3x4:     return "Double3x4";
    case FV::ShaderDataType::Double4x4:     return "Double4x4";
    }
    return "Error";
}

inline void printShaderResourceStructMember(const FV::ShaderResourceStructMember& member,
                                            const char* prefix,
                                            int indent,
                                            FV::Log::Level lv = FV::Log::Level::Info)
{
    std::string indentStr = "";
    for (int i = 0; i < indent; ++i)
    {
        indentStr += "    ";
    }

    if (member.stride > 0)
    {
        FV::Log::log(lv, std::format("{} {}+ {}[{:d}] ({}, Offset: {:d}, size: {:d}, stride: {:d})",
                                     prefix,
                                     indentStr,
                                     member.name,
                                     member.count,
                                     shaderDataTypeString(member.dataType),
                                     member.offset, member.size, member.stride));
    }
    else
    {
        FV::Log::log(lv, std::format("{} {}+ {} ({}, Offset: {:d}, size: {:d})",
                                     prefix,
                                     indentStr,
                                     member.name,
                                     shaderDataTypeString(member.dataType),
                                     member.offset, member.size));
    }
    for (auto& mem : member.members)
    {
        printShaderResourceStructMember(mem, prefix, indent + 1, lv);
    }
}

inline void printShaderResource(const FV::ShaderResource& res, FV::Log::Level lv = FV::Log::Level::Info)
{
    if (res.count > 1)
        FV::Log::log(lv, std::format("ShaderResource: {}[{:d}] (set={:d}, binding={:d}, stages={})",
                                     res.name, res.count, res.set, res.binding,
                                     shaderStageNames(res.stages)));
    else
        FV::Log::log(lv, std::format("ShaderResource: {} (set={:d}, binding={:d}, stages={})",
                                     res.name, res.set, res.binding,
                                     shaderStageNames(res.stages)));

    const char* type = "Unknown (ERROR)";
    switch (res.type)
    {
    case FV::ShaderResource::TypeBuffer:         type = "Buffer"; break;
    case FV::ShaderResource::TypeTexture:	     type = "Texture"; break;
    case FV::ShaderResource::TypeSampler:	     type = "Sampler"; break;
    case FV::ShaderResource::TypeTextureSampler: type = "SampledTexture"; break;
    }
    const char* access = "Unknown (ERROR)";
    switch (res.access)
    {
    case FV::ShaderResource::AccessReadOnly:	access = "ReadOnly"; break;
    case FV::ShaderResource::AccessWriteOnly:	access = "WriteOnly"; break;
    case FV::ShaderResource::AccessReadWrite:	access = "ReadWrite"; break;
    }

    if (res.type == FV::ShaderResource::TypeBuffer)
    {
        FV::Log::log(lv, std::format(" Type:{}, Access:{}, Enabled:{:d}, Size:{:d}",
                                     type,
                                     access,
                                     int(res.enabled),
                                     res.typeInfo.buffer.size));

        if (res.typeInfo.buffer.dataType == FV::ShaderDataType::Struct)
        {
            FV::Log::log(lv, std::format(" Struct \"{}\"", res.name));
            for (auto& mem : res.members)
            {
                printShaderResourceStructMember(mem, "", 1, lv);
            }
        }
    }
    else
    {
        FV::Log::log(lv, std::format(" Type:{}, Access:{}, Enabled:{:d}",
                                     type,
                                     access,
                                     int(res.enabled)));
    }
}

inline void printShaderReflection(const FV::Shader& shader, FV::Log::Level lv = FV::Log::Level::Info)
{
    std::string stage = shaderStageNames((uint32_t)shader.stage());
    if (stage.size() == 0)
        stage = "Unknown";

    FV::Log::log(lv, "=========================================================");
    FV::Log::log(lv, std::format("Shader<{}.SPIR-V>.InputAttributes: {:d}",
                                 stage, shader.inputAttributes().size()));
    for (int i = 0; i < shader.inputAttributes().size(); ++i)
    {
        const FV::ShaderAttribute& attr = shader.inputAttributes().at(i);
        FV::Log::log(lv, std::format("  [in] ShaderAttribute[{:d}]: \"{}\" (type:{}, location:{:d})",
                                     i, attr.name, shaderDataTypeString(attr.type), attr.location));
    }
    FV::Log::log(lv, "---------------------------------------------------------");
    FV::Log::log(lv, std::format("Shader<{}.SPIR-V>.OutputAttributes: {:d}",
                                 stage, shader.outputAttributes().size()));
    for (int i = 0; i < shader.outputAttributes().size(); ++i)
    {
        const FV::ShaderAttribute& attr = shader.outputAttributes().at(i);
        FV::Log::log(lv, std::format("  [out] ShaderAttribute[{:d}]: \"{}\" (type:{}, location:{:d})",
                                     i, attr.name, shaderDataTypeString(attr.type), attr.location));
    }
    FV::Log::log(lv, "---------------------------------------------------------");
    FV::Log::log(lv, std::format("Shader<{}.SPIR-V>.Resources: {:d}",
                                 stage, shader.resources().size()));
    for (auto& arg : shader.resources())
        printShaderResource(arg, lv);
    for (int i = 0; i < shader.pushConstantLayouts().size(); ++i)
    {
        const FV::ShaderPushConstantLayout& layout = shader.pushConstantLayouts().at(i);
        FV::Log::log(lv, std::format(" PushConstant:{:d} \"{}\" (offset:{:d}, size:{:d}, stages:{})",
                                     i, layout.name, layout.offset, layout.size,
                                     shaderStageNames(layout.stages)));
        for (auto& member : layout.members)
            printShaderResourceStructMember(member, "", 1, lv);
    }
    FV::Log::log(lv, "=========================================================");
}

inline void printPipelineReflection(const FV::PipelineReflection& reflection, FV::Log::Level lv = FV::Log::Level::Info)
{
    FV::Log::log(lv, "=========================================================");
    FV::Log::log(lv, std::format("PipelineReflection.InputAttributes: {:d}",
                                 reflection.inputAttributes.size()));
    for (int i = 0; i < reflection.inputAttributes.size(); ++i)
    {
        const FV::ShaderAttribute& attr = reflection.inputAttributes.at(i);
        FV::Log::log(lv, std::format("  [in] ShaderAttribute[{:d}]: \"{}\" (type:{}, location:{:d})",
                                     i, attr.name, shaderDataTypeString(attr.type), attr.location));
    }
    FV::Log::log(lv, "---------------------------------------------------------");
    FV::Log::log(lv, std::format("PipelineReflection.Resources: {:d}",
                                 reflection.resources.size()));
    for (auto& arg : reflection.resources)
        printShaderResource(arg, lv);
    for (int i = 0; i < reflection.pushConstantLayouts.size(); ++i)
    {
        const FV::ShaderPushConstantLayout& layout = reflection.pushConstantLayouts.at(i);
        FV::Log::log(lv, std::format(" PushConstant:{:d} \"{}\" (offset:{:d}, size:{:d}, stages:{})",
                                     i, layout.name, layout.offset, layout.size,
                                     shaderStageNames(layout.stages)));
        for (auto& member : layout.members)
            printShaderResourceStructMember(member, "", 1, lv);
    }
    FV::Log::log(lv, "=========================================================");
}
