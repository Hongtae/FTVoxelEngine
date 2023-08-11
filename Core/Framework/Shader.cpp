#include <algorithm>
#include <fstream>
#include <iterator>
#include "../Libs/SPIRV-Cross/spirv_cross.hpp"
#include "../Libs/SPIRV-Cross/spirv_common.hpp"
#include "Shader.h"
#include "Logger.h"

using namespace FV;

namespace 
{
    ShaderDataType shaderDataTypeFromSPIRType(const spirv_cross::SPIRType& spType)
    {
        ShaderDataType dataType = ShaderDataType::Unknown;
        // get item type
        switch (spType.basetype)
        {
        case spirv_cross::SPIRType::Void:
            dataType = ShaderDataType::None;
            break;
        case spirv_cross::SPIRType::Struct:
            dataType = ShaderDataType::Struct;
            break;
        case spirv_cross::SPIRType::Image:
        case spirv_cross::SPIRType::SampledImage:
            dataType = ShaderDataType::Texture;
            break;
        case spirv_cross::SPIRType::Sampler:
            dataType = ShaderDataType::Sampler;
            break;
        case spirv_cross::SPIRType::Boolean:
            switch (spType.vecsize)
            {
            case 2:		dataType = ShaderDataType::Bool2;   break;
            case 3:		dataType = ShaderDataType::Bool3;   break;
            case 4:		dataType = ShaderDataType::Bool4;   break;
            default:	dataType = ShaderDataType::Bool;    break;
            }
            break;
            //case spirv_cross::SPIRType::Char:
        case spirv_cross::SPIRType::SByte:
            switch (spType.vecsize)
            {
            case 2:		dataType = ShaderDataType::Char2;   break;
            case 3:		dataType = ShaderDataType::Char3;   break;
            case 4:		dataType = ShaderDataType::Char4;   break;
            default:	dataType = ShaderDataType::Char;    break;
            }
            break;
        case spirv_cross::SPIRType::UByte:
            switch (spType.vecsize)
            {
            case 2:		dataType = ShaderDataType::UChar2;  break;
            case 3:		dataType = ShaderDataType::UChar3;  break;
            case 4:		dataType = ShaderDataType::UChar4;  break;
            default:	dataType = ShaderDataType::UChar;   break;
            }
            break;
        case spirv_cross::SPIRType::Short:
            switch (spType.vecsize)
            {
            case 2:		dataType = ShaderDataType::Short2;  break;
            case 3:		dataType = ShaderDataType::Short3;  break;
            case 4:		dataType = ShaderDataType::Short4;  break;
            default:	dataType = ShaderDataType::Short;   break;
            }
            break;
        case spirv_cross::SPIRType::UShort:
            switch (spType.vecsize)
            {
            case 2:		dataType = ShaderDataType::UShort2; break;
            case 3:		dataType = ShaderDataType::UShort3; break;
            case 4:		dataType = ShaderDataType::UShort4; break;
            default:	dataType = ShaderDataType::UShort;  break;
            }
            break;
        case spirv_cross::SPIRType::Int:
            switch (spType.vecsize)
            {
            case 2:		dataType = ShaderDataType::Int2;    break;
            case 3:		dataType = ShaderDataType::Int3;    break;
            case 4:		dataType = ShaderDataType::Int4;    break;
            default:	dataType = ShaderDataType::Int;     break;
            }
            break;
        case spirv_cross::SPIRType::UInt:
            switch (spType.vecsize)
            {
            case 2:		dataType = ShaderDataType::UInt2;   break;
            case 3:		dataType = ShaderDataType::UInt3;   break;
            case 4:		dataType = ShaderDataType::UInt4;   break;
            default:	dataType = ShaderDataType::UInt;    break;
            }
            break;
        case spirv_cross::SPIRType::Int64:
            switch (spType.vecsize)
            {
            case 2:		dataType = ShaderDataType::Long2;   break;
            case 3:		dataType = ShaderDataType::Long3;   break;
            case 4:		dataType = ShaderDataType::Long4;   break;
            default:	dataType = ShaderDataType::Long;    break;
            }
            break;
        case spirv_cross::SPIRType::UInt64:
            switch (spType.vecsize)
            {
            case 2:		dataType = ShaderDataType::ULong2;  break;
            case 3:		dataType = ShaderDataType::ULong3;  break;
            case 4:		dataType = ShaderDataType::ULong4;  break;
            default:	dataType = ShaderDataType::ULong;   break;
            }
            break;
        case spirv_cross::SPIRType::Half:
            switch (spType.vecsize)
            {
            case 2:
                switch (spType.columns)
                {
                case 2:     dataType = ShaderDataType::Half2x2; break;
                case 3:     dataType = ShaderDataType::Half2x3; break;
                case 4:     dataType = ShaderDataType::Half2x4; break;
                default:    dataType = ShaderDataType::Half2;   break;
                }
                break;
            case 3:
                switch (spType.columns)
                {
                case 2:     dataType = ShaderDataType::Half3x2; break;
                case 3:     dataType = ShaderDataType::Half3x3; break;
                case 4:     dataType = ShaderDataType::Half3x4; break;
                default:    dataType = ShaderDataType::Half3;   break;
                }
                break;
            case 4:
                switch (spType.columns)
                {
                case 2:     dataType = ShaderDataType::Half4x2; break;
                case 3:     dataType = ShaderDataType::Half4x3; break;
                case 4:     dataType = ShaderDataType::Half4x4; break;
                default:    dataType = ShaderDataType::Half4;   break;
                }
                break;
            default:
                dataType = ShaderDataType::Half;                break;
            }
            break;
        case spirv_cross::SPIRType::Float:
            switch (spType.vecsize)
            {
            case 2:
                switch (spType.columns)
                {
                case 2:     dataType = ShaderDataType::Float2x2;    break;
                case 3:     dataType = ShaderDataType::Float2x3;    break;
                case 4:     dataType = ShaderDataType::Float2x4;    break;
                default:    dataType = ShaderDataType::Float2;      break;
                }
                break;
            case 3:
                switch (spType.columns)
                {
                case 2:     dataType = ShaderDataType::Float3x2;    break;
                case 3:     dataType = ShaderDataType::Float3x3;    break;
                case 4:     dataType = ShaderDataType::Float3x4;    break;
                default:    dataType = ShaderDataType::Float3;      break;
                }
                break;
            case 4:
                switch (spType.columns)
                {
                case 2:     dataType = ShaderDataType::Float4x2;    break;
                case 3:     dataType = ShaderDataType::Float4x3;    break;
                case 4:     dataType = ShaderDataType::Float4x4;    break;
                default:    dataType = ShaderDataType::Float4;      break;
                }
                break;
            default:
                dataType = ShaderDataType::Float;                   break;
            }
            break;
        case spirv_cross::SPIRType::Double:
            switch (spType.vecsize)
            {
            case 2:
                switch (spType.columns)
                {
                case 2:     dataType = ShaderDataType::Double2x2;   break;
                case 3:     dataType = ShaderDataType::Double2x3;   break;
                case 4:     dataType = ShaderDataType::Double2x4;   break;
                default:    dataType = ShaderDataType::Double2;     break;
                }
                break;
            case 3:
                switch (spType.columns)
                {
                case 2:     dataType = ShaderDataType::Double3x2;   break;
                case 3:     dataType = ShaderDataType::Double3x3;   break;
                case 4:     dataType = ShaderDataType::Double3x4;   break;
                default:    dataType = ShaderDataType::Double3;     break;
                }
                break;
            case 4:
                switch (spType.columns)
                {
                case 2:     dataType = ShaderDataType::Double4x2;   break;
                case 3:     dataType = ShaderDataType::Double4x3;   break;
                case 4:     dataType = ShaderDataType::Double4x4;   break;
                default:    dataType = ShaderDataType::Double4;     break;
                }
                break;
            default:
                dataType = ShaderDataType::Double;                  break;
            }
            break;
        default:
            Log::error("Unsupported stage input attribute type!");
        }
        return dataType;
    }
}

Shader::Shader()
    : _stage(ShaderStage::Unknown)
    , _threadgroupSize({ 1, 1, 1 })
{
}

Shader::Shader(const std::filesystem::path& path)
    : Shader()
{
    std::ifstream fs(path, std::ifstream::binary | std::ifstream::in);
    if (fs.good())
    {
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(fs)),
                                  std::istreambuf_iterator<char>());
        if (data.empty() == false)
        {
            auto words = data.size() / sizeof(uint32_t);
            auto ir = reinterpret_cast<const uint32_t*>(data.data());
            _data = std::vector<uint32_t>(&ir[0], &ir[words]);
            if (compile() == false)
            {
                Log::error("shader compilation failed.");
                _stage = ShaderStage::Unknown;
                _data = {};
            }
        }
    }
}

Shader::Shader(const std::vector<uint32_t>& spv)
    : Shader(spv.data(), spv.size())
{
}

Shader::Shader(const uint32_t* ir, size_t words)
    : Shader()
{
    _data = std::vector<uint32_t>(&ir[0], &ir[words]);
    if (compile() == false)
    {
        Log::error("shader compilation failed.");
        _stage = ShaderStage::Unknown;
        _data = {};
    }
}

Shader::~Shader()
{
}

bool Shader::compile()
{
    if (_data.empty())
        return false;

    try
    {
        spirv_cross::Compiler compiler(_data);

        switch (spv::ExecutionModel exec = compiler.get_execution_model(); exec)
        {
        case spv::ExecutionModelVertex:
            this->_stage = ShaderStage::Vertex; break;
        case spv::ExecutionModelTessellationControl:
            this->_stage = ShaderStage::TessellationControl; break;
        case spv::ExecutionModelTessellationEvaluation:
            this->_stage = ShaderStage::TessellationEvaluation; break;
        case spv::ExecutionModelGeometry:
            this->_stage = ShaderStage::Geometry; break;
        case spv::ExecutionModelFragment:
            this->_stage = ShaderStage::Fragment; break;
        case spv::ExecutionModelGLCompute:
            this->_stage = ShaderStage::Compute; break;
        default:
            throw std::runtime_error("Unknown shader type");
            break;
        }

        this->_functions.clear();
        this->_resources.clear();
        this->_inputAttributes.clear();
        this->_outputAttributes.clear();
        this->_pushConstantLayouts.clear();
        this->_descriptors.clear();
        this->_threadgroupSize = { 1,1,1 };

        // get thread group size
        if (auto model = compiler.get_execution_model(); model == spv::ExecutionModelGLCompute)
        {
            uint32_t localSizeX = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0);
            uint32_t localSizeY = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1);
            uint32_t localSizeZ = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2);

            spirv_cross::SpecializationConstant wg_x, wg_y, wg_z;
            uint32_t constantID = compiler.get_work_group_size_specialization_constants(wg_x, wg_y, wg_z);
            if (wg_x.id)
                localSizeX = compiler.get_constant(wg_x.id).scalar();
            if (wg_y.id)
                localSizeY = compiler.get_constant(wg_y.id).scalar();
            if (wg_z.id)
                localSizeZ = compiler.get_constant(wg_z.id).scalar();

            Log::debug(std::format("ComputeShader.constantID: {:d}", constantID));
            Log::debug(std::format("ComputeShader.LocalSize.X: {:d} (specialized: {:d}, specializationID: {:d})", localSizeX, (uint32_t)wg_x.id, wg_x.constant_id));
            Log::debug(std::format("ComputeShader.LocalSize.Y: {:d} (specialized: {:d}, specializationID: {:d})", localSizeY, (uint32_t)wg_y.id, wg_y.constant_id));
            Log::debug(std::format("ComputeShader.LocalSize.Z: {:d} (specialized: {:d}, specializationID: {:d})", localSizeZ, (uint32_t)wg_z.id, wg_z.constant_id));

            this->_threadgroupSize.x = std::max(localSizeX, 1U);
            this->_threadgroupSize.y = std::max(localSizeY, 1U);
            this->_threadgroupSize.z = std::max(localSizeZ, 1U);
        }

        auto getStructMembers = [&compiler](const spirv_cross::SPIRType& spType)->std::vector<ShaderResourceStructMember>
        {
            struct StructMemberExtractor
            {
                spirv_cross::Compiler& compiler;
                auto operator () (const spirv_cross::SPIRType& spType) -> std::vector<ShaderResourceStructMember>
                {
                    std::vector<ShaderResourceStructMember> members;
                    members.reserve(spType.member_types.size());
                    for (uint32_t i = 0; i < spType.member_types.size(); ++i)
                    {
                        ShaderResourceStructMember member = {};

                        uint32_t type = spType.member_types[i];
                        const spirv_cross::SPIRType& memberType = compiler.get_type(type);

                        member.dataType = shaderDataTypeFromSPIRType(memberType);
                        FVASSERT_THROW(member.dataType != ShaderDataType::Unknown);
                        FVASSERT_THROW(member.dataType != ShaderDataType::None);

                        member.name = compiler.get_member_name(spType.self, i).c_str();
                        // member offset within this struct.
                        member.offset = compiler.type_struct_member_offset(spType, i);
                        member.size = (uint32_t)compiler.get_declared_struct_member_size(spType, i);
                        FVASSERT_THROW(member.size > 0);

                        if (member.dataType == ShaderDataType::Struct)
                        {
                            member.members = StructMemberExtractor{ compiler }(memberType);
                            member.members.shrink_to_fit();
                        }

                        member.count = 1;
                        for (auto n : memberType.array)
                            member.count = member.count * n;

                        if (member.count > 1)
                            member.stride = compiler.type_struct_member_array_stride(spType, i);

                        members.push_back(member);
                    }
                    return members;
                }
            };
            return StructMemberExtractor{ compiler }(spType);
        };

        auto active = compiler.get_active_interface_variables();
        uint32_t stage = static_cast<uint32_t>(this->_stage);
        auto getResource = [&compiler, &active, stage, &getStructMembers](const spirv_cross::Resource& resource, ShaderResource::Access access)->ShaderResource
        {
            ShaderResource out = {};
            out.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
            out.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
            out.name = compiler.get_name(resource.id).c_str();
            out.stride = compiler.get_decoration(resource.id, spv::DecorationArrayStride);
            out.enabled = active.find(resource.id) != active.end();
            out.stages = stage;
            out.access = access;

            const spirv_cross::SPIRType& type = compiler.get_type(resource.type_id);
            out.count = 1;
            for (auto n : type.array)
                out.count = out.count * n;

            auto getTextureType = [](spv::Dim dim) {
                switch (dim) {
                case spv::Dim1D:    return TextureType1D;
                case spv::Dim2D:    return TextureType2D;
                case spv::Dim3D:    return TextureType3D;
                case spv::DimCube:  return TextureTypeCube;
                default:
                    Log::warning("Unknown texture type!");
                    break;
                }
                return TextureTypeUnknown;
            };

            switch (type.basetype)
            {
            case spirv_cross::SPIRType::Image:
                out.type = ShaderResource::TypeTexture;
                out.typeInfo.texture.dataType = ShaderDataType::Texture;
                out.typeInfo.texture.textureType = getTextureType(type.image.dim);
                break;
            case spirv_cross::SPIRType::SampledImage:
                out.type = ShaderResource::TypeTextureSampler;
                out.typeInfo.texture.dataType = ShaderDataType::Texture;
                out.typeInfo.texture.textureType = getTextureType(type.image.dim);
                break;
            case spirv_cross::SPIRType::Sampler:
                out.type = ShaderResource::TypeSampler;
                break;
            case spirv_cross::SPIRType::Struct:
                out.type = ShaderResource::TypeBuffer;
                out.typeInfo.buffer.dataType = ShaderDataType::Struct;
                out.typeInfo.buffer.alignment = compiler.get_decoration(resource.id, spv::DecorationAlignment);
                out.typeInfo.buffer.size = (uint32_t)compiler.get_declared_struct_size(type);
                break;
            default:
                Log::error("Unsupported SPIR-V type!");
                FVERROR_THROW("Not implemented");
            }

            if (out.type == ShaderResource::TypeBuffer)
            {
                out.members = getStructMembers(compiler.get_type(resource.base_type_id));
                out.members.shrink_to_fit();
            }
            return out;
        };

        auto getDescriptor = [&compiler](const spirv_cross::Resource& resource, ShaderDescriptorType type)->ShaderDescriptor
        {
            ShaderDescriptor desc = {};
            desc.type = type;
            desc.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
            desc.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

            const spirv_cross::SPIRType& spType = compiler.get_type_from_variable(resource.id);

            // get item count! (array size)
            desc.count = 1;
            if (spType.array.size() > 0)
            {
                for (auto i : spType.array)
                    desc.count *= i;
            }
            return desc;
        };


        spirv_cross::ShaderResources resources = compiler.get_shader_resources();
        // https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide
        // uniform_buffers
        for (const spirv_cross::Resource& resource : resources.uniform_buffers)
        {
            this->_resources.push_back(getResource(resource, ShaderResource::AccessReadOnly));
            this->_descriptors.push_back(getDescriptor(resource, ShaderDescriptorType::UniformBuffer));
        }
        // storage_buffers
        for (const spirv_cross::Resource& resource : resources.storage_buffers)
        {
            this->_resources.push_back(getResource(resource, ShaderResource::AccessReadWrite));
            this->_descriptors.push_back(getDescriptor(resource, ShaderDescriptorType::StorageBuffer));
        }
        // storage_images
        for (const spirv_cross::Resource& resource : resources.storage_images)
        {
            const spirv_cross::SPIRType& spType = compiler.get_type_from_variable(resource.id);
            ShaderDescriptorType type = ShaderDescriptorType::StorageTexture;
            if (spType.image.dim == spv::DimBuffer)
            {
                type = ShaderDescriptorType::StorageTexelBuffer;
            }
            this->_resources.push_back(getResource(resource, ShaderResource::AccessReadWrite));
            this->_descriptors.push_back(getDescriptor(resource, type));
        }
        // sampled_images (sampler2D)
        for (const spirv_cross::Resource& resource : resources.sampled_images)
        {
            this->_resources.push_back(getResource(resource, ShaderResource::AccessReadOnly));
            this->_descriptors.push_back(getDescriptor(resource, ShaderDescriptorType::TextureSampler));
        }
        // separate_images
        for (const spirv_cross::Resource& resource : resources.separate_images)
        {
            const spirv_cross::SPIRType& spType = compiler.get_type_from_variable(resource.id);
            ShaderDescriptorType type = ShaderDescriptorType::Texture;
            if (spType.image.dim == spv::DimBuffer)
            {
                type = ShaderDescriptorType::UniformTexelBuffer;
            }
            this->_resources.push_back(getResource(resource, ShaderResource::AccessReadOnly));
            this->_descriptors.push_back(getDescriptor(resource, type));
        }
        // separate_samplers
        for (const spirv_cross::Resource& resource : resources.separate_samplers)
        {
            this->_resources.push_back(getResource(resource, ShaderResource::AccessReadOnly));
            this->_descriptors.push_back(getDescriptor(resource, ShaderDescriptorType::Sampler));
        }

        auto getAttributes = [&compiler, &active](const spirv_cross::Resource& resource)->ShaderAttribute
        {
            uint32_t location = compiler.get_decoration(resource.id, spv::DecorationLocation);
            std::string name = "";
            if (resource.name.size() > 0)
                name = resource.name;
            else
                name = compiler.get_fallback_name(resource.id);

            const spirv_cross::SPIRType& spType = compiler.get_type(resource.type_id);

            ShaderDataType dataType = shaderDataTypeFromSPIRType(spType);
            FVASSERT_THROW(dataType != ShaderDataType::Unknown);

            // get item count! (array size)
            uint32_t count = 1;
            if (spType.array.size() > 0)
            {
                for (auto i : spType.array)
                    count *= i;
            }

            ShaderAttribute attr = {};
            attr.location = location;
            attr.name = name;
            attr.type = dataType;
            attr.enabled = active.find(resource.id) != active.end();
            return attr;
        };
        // stage inputs
        this->_inputAttributes.reserve(resources.stage_inputs.size());
        for (const spirv_cross::Resource& resource : resources.stage_inputs)
        {
            this->_inputAttributes.push_back(getAttributes(resource));
        }
        // stage outputs
        this->_outputAttributes.reserve(resources.stage_outputs.size());
        for (const spirv_cross::Resource& resource : resources.stage_outputs)
        {
            this->_outputAttributes.push_back(getAttributes(resource));
        }

        // pushConstant range.
        this->_pushConstantLayouts.reserve(resources.push_constant_buffers.size());
        for (const spirv_cross::Resource& resource : resources.push_constant_buffers)
        {
            spirv_cross::SmallVector<spirv_cross::BufferRange> ranges = compiler.get_active_buffer_ranges(resource.id);
            FVASSERT_THROW(ranges.size());

            size_t pushConstantOffset = ranges[0].offset;
            size_t pushConstantSize = 0;

            for (spirv_cross::BufferRange& range : ranges)
            {
                pushConstantOffset = std::min(range.offset, pushConstantOffset);
                pushConstantSize = std::max(range.offset + range.range, pushConstantSize);
            }
            FVASSERT_THROW(pushConstantSize > pushConstantOffset);
            FVASSERT_THROW((pushConstantOffset % 4) == 0);
            FVASSERT_THROW((pushConstantSize % 4) == 0);
            FVASSERT_THROW(pushConstantSize > 0);

            ShaderPushConstantLayout layout = {};
            layout.stages = stage;
            layout.offset = uint32_t(pushConstantOffset);
            layout.size = uint32_t(pushConstantSize - pushConstantOffset);
            layout.name = compiler.get_name(resource.id).c_str();

            layout.members = getStructMembers(compiler.get_type(resource.base_type_id));
            layout.members.shrink_to_fit();

            //const spirv_cross::SPIRType& spType = compiler.get_type_from_variable(resource.id);
            this->_pushConstantLayouts.push_back(layout);
        }

        // get module entry points
        spirv_cross::SmallVector<spirv_cross::EntryPoint> entryPoints = compiler.get_entry_points_and_stages();
        for (spirv_cross::EntryPoint& ep : entryPoints)
        {
            this->_functions.push_back(ep.name);
        }

        // specialization constants
        spirv_cross::SmallVector<spirv_cross::SpecializationConstant> spConstants = compiler.get_specialization_constants();
        for (spirv_cross::SpecializationConstant& sc : spConstants)
        {
            auto spvID = sc.id;
            auto constantID = sc.constant_id;
            // 
        }

        // sort bindings
        std::sort(this->_descriptors.begin(), this->_descriptors.end(),
                  [](const auto& a, const auto& b)
                  {
                      if (a.set == b.set)
                          return a.binding < b.binding;
                      return a.set < b.set;
                  });
        std::sort(this->_resources.begin(), this->_resources.end(),
                  [](const auto& a, const auto& b)
                  {
                      if (a.type == b.type)
                      {
                          if (a.set == b.set)
                              return a.binding < b.binding;
                          return a.set < b.set;
                      }
                      return static_cast<int>(a.type) < static_cast<int>(b.type);
                  });
        std::sort(this->_inputAttributes.begin(), this->_inputAttributes.end(),
                  [](const auto& a, const auto& b)
                  {
                      return a.location < b.location;
                  });
        std::sort(this->_outputAttributes.begin(), this->_outputAttributes.end(),
                  [](const auto& a, const auto& b)
                  {
                      return a.location < b.location;
                  });

        this->_descriptors.shrink_to_fit();
        this->_resources.shrink_to_fit();
        this->_inputAttributes.shrink_to_fit();
        this->_outputAttributes.shrink_to_fit();
        return true;
    }
    catch (const spirv_cross::CompilerError& err)
    {
        Log::error(std::format("Compiler error: {}", err.what()));
    }
    catch (const std::exception& err)
    {
        Log::error(std::format("internal error: {}", err.what()));
    }
    catch (...)
    {
        throw;
    }

    _stage = ShaderStage::Unknown;
    _functions.clear();
    _resources.clear();
    _inputAttributes.clear();
    _outputAttributes.clear();
    _pushConstantLayouts.clear();
    _descriptors.clear();
    _threadgroupSize = { 1,1,1 };
    return false;
}

bool Shader::validate()
{
    return isValid();
}

bool Shader::isValid() const
{
    return _stage != ShaderStage::Unknown && _data.empty() == false;
}
