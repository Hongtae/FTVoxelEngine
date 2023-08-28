#pragma once
#include "../include.h"
#include "Texture.h"
#include <string>
#include <vector>

namespace FV
{
    enum class ShaderDataType
    {
        Unknown = -1,
        None = 0,

        Struct,
        Texture,
        Sampler,

        Bool,
        Bool2,
        Bool3,
        Bool4,

        Char,
        Char2,
        Char3,
        Char4,

        UChar,
        UChar2,
        UChar3,
        UChar4,

        Short,
        Short2,
        Short3,
        Short4,

        UShort,
        UShort2,
        UShort3,
        UShort4,

        Int,
        Int2,
        Int3,
        Int4,

        UInt,
        UInt2,
        UInt3,
        UInt4,

        Long,
        Long2,
        Long3,
        Long4,

        ULong,
        ULong2,
        ULong3,
        ULong4,

        Half,
        Half2,
        Half3,
        Half4,
        Half2x2,
        Half3x2,
        Half4x2,
        Half2x3,
        Half3x3,
        Half4x3,
        Half2x4,
        Half3x4,
        Half4x4,

        Float,
        Float2,
        Float3,
        Float4,
        Float2x2,
        Float3x2,
        Float4x2,
        Float2x3,
        Float3x3,
        Float4x3,
        Float2x4,
        Float3x4,
        Float4x4,

        Double,
        Double2,
        Double3,
        Double4,
        Double2x2,
        Double3x2,
        Double4x2,
        Double2x3,
        Double3x3,
        Double4x3,
        Double2x4,
        Double3x4,
        Double4x4,
    };

    struct ShaderDataTypeSize
    {
        uint32_t width;
        uint32_t rows;
        uint32_t columns;

        uint32_t Bytes() const noexcept { return width * rows * columns; }

        constexpr ShaderDataTypeSize(uint32_t w, uint32_t r, uint32_t c)
            : width(w), rows(r), columns(c) {}

        constexpr ShaderDataTypeSize(ShaderDataType type = ShaderDataType::None)
            : ShaderDataTypeSize(
                [type]() constexpr -> ShaderDataTypeSize
                {
                    switch (type)
                    {
                    case ShaderDataType::None:
                    case ShaderDataType::Struct:
                    case ShaderDataType::Texture:
                    case ShaderDataType::Sampler:
                        return { 0, 0, 0 };

                    case ShaderDataType::Bool:      return { 1, 1, 1 };
                    case ShaderDataType::Bool2:     return { 1, 2, 1 };
                    case ShaderDataType::Bool3:     return { 1, 3, 1 };
                    case ShaderDataType::Bool4:     return { 1, 4, 1 };

                    case ShaderDataType::Char:      return { 1, 1, 1 };
                    case ShaderDataType::Char2:     return { 1, 2, 1 };
                    case ShaderDataType::Char3:     return { 1, 3, 1 };
                    case ShaderDataType::Char4:     return { 1, 4, 1 };

                    case ShaderDataType::UChar:     return { 1, 1, 1 };
                    case ShaderDataType::UChar2:    return { 1, 2, 1 };
                    case ShaderDataType::UChar3:    return { 1, 3, 1 };
                    case ShaderDataType::UChar4:    return { 1, 4, 1 };

                    case ShaderDataType::Short:     return { 2, 1, 1 };
                    case ShaderDataType::Short2:    return { 2, 2, 1 };
                    case ShaderDataType::Short3:    return { 2, 3, 1 };
                    case ShaderDataType::Short4:    return { 2, 4, 1 };

                    case ShaderDataType::UShort:    return { 2, 1, 1 };
                    case ShaderDataType::UShort2:   return { 2, 2, 1 };
                    case ShaderDataType::UShort3:   return { 2, 3, 1 };
                    case ShaderDataType::UShort4:   return { 2, 4, 1 };

                    case ShaderDataType::Int:       return { 4, 1, 1 };
                    case ShaderDataType::Int2:      return { 4, 2, 1 };
                    case ShaderDataType::Int3:      return { 4, 3, 1 };
                    case ShaderDataType::Int4:      return { 4, 4, 1 };

                    case ShaderDataType::UInt:      return { 4, 1, 1 };
                    case ShaderDataType::UInt2:     return { 4, 2, 1 };
                    case ShaderDataType::UInt3:     return { 4, 3, 1 };
                    case ShaderDataType::UInt4:     return { 4, 4, 1 };

                    case ShaderDataType::Long:      return { 8, 1, 1 };
                    case ShaderDataType::Long2:     return { 8, 2, 1 };
                    case ShaderDataType::Long3:     return { 8, 3, 1 };
                    case ShaderDataType::Long4:     return { 8, 4, 1 };

                    case ShaderDataType::ULong:     return { 8, 1, 1 };
                    case ShaderDataType::ULong2:    return { 8, 2, 1 };
                    case ShaderDataType::ULong3:    return { 8, 3, 1 };
                    case ShaderDataType::ULong4:    return { 8, 4, 1 };

                    case ShaderDataType::Half:      return { 2, 1, 1 };
                    case ShaderDataType::Half2:     return { 2, 2, 1 };
                    case ShaderDataType::Half3:     return { 2, 3, 1 };
                    case ShaderDataType::Half4:     return { 2, 4, 1 };
                    case ShaderDataType::Half2x2:   return { 2, 2, 2 };
                    case ShaderDataType::Half3x2:   return { 2, 3, 2 };
                    case ShaderDataType::Half4x2:   return { 2, 4, 2 };
                    case ShaderDataType::Half2x3:   return { 2, 2, 3 };
                    case ShaderDataType::Half3x3:   return { 2, 3, 3 };
                    case ShaderDataType::Half4x3:   return { 2, 4, 3 };
                    case ShaderDataType::Half2x4:   return { 2, 2, 4 };
                    case ShaderDataType::Half3x4:   return { 2, 3, 4 };
                    case ShaderDataType::Half4x4:   return { 2, 4, 4 };

                    case ShaderDataType::Float:     return { 4, 1, 1 };
                    case ShaderDataType::Float2:    return { 4, 2, 1 };
                    case ShaderDataType::Float3:    return { 4, 3, 1 };
                    case ShaderDataType::Float4:    return { 4, 4, 1 };
                    case ShaderDataType::Float2x2:  return { 4, 2, 2 };
                    case ShaderDataType::Float3x2:  return { 4, 3, 2 };
                    case ShaderDataType::Float4x2:  return { 4, 4, 2 };
                    case ShaderDataType::Float2x3:  return { 4, 2, 3 };
                    case ShaderDataType::Float3x3:  return { 4, 3, 3 };
                    case ShaderDataType::Float4x3:  return { 4, 4, 3 };
                    case ShaderDataType::Float2x4:  return { 4, 2, 4 };
                    case ShaderDataType::Float3x4:  return { 4, 3, 4 };
                    case ShaderDataType::Float4x4:  return { 4, 4, 4 };

                    case ShaderDataType::Double:    return { 8, 1, 1 };
                    case ShaderDataType::Double2:   return { 8, 2, 1 };
                    case ShaderDataType::Double3:   return { 8, 3, 1 };
                    case ShaderDataType::Double4:   return { 8, 4, 1 };
                    case ShaderDataType::Double2x2: return { 8, 2, 2 };
                    case ShaderDataType::Double3x2: return { 8, 3, 2 };
                    case ShaderDataType::Double4x2: return { 8, 4, 2 };
                    case ShaderDataType::Double2x3: return { 8, 2, 3 };
                    case ShaderDataType::Double3x3: return { 8, 3, 3 };
                    case ShaderDataType::Double4x3: return { 8, 4, 3 };
                    case ShaderDataType::Double2x4: return { 8, 2, 4 };
                    case ShaderDataType::Double3x4: return { 8, 3, 4 };
                    case ShaderDataType::Double4x4: return { 8, 4, 4 };

                    default:
                        FVASSERT_DESC_DEBUG(0, "Unknown data type");
                        break;
                    }
                    return { 0, 0, 0 };
                }())
        {
        }
    };

    enum class ShaderStage
    {
        Unknown = 0,
        Vertex = 1U,
        TessellationControl = 1U << 1,
        TessellationEvaluation = 1U << 2,
        Geometry = 1U << 3,
        Fragment = 1U << 4,
        Compute = 1U << 5,
    };

    struct ShaderResourceBuffer
    {
        ShaderDataType dataType;
        uint32_t alignment;
        uint32_t size;
    };

    struct ShaderResourceTexture
    {
        ShaderDataType dataType;
        TextureType textureType;
    };

    struct ShaderResourceThreadgroup
    {
        uint32_t alignment;
        uint32_t size;
    };

    struct ShaderResourceStructMember
    {
        ShaderDataType dataType;
        std::string name;
        uint32_t offset;
        uint32_t size;  // declared size
        uint32_t count; // array length
        uint32_t stride; // stride between array elements

        std::vector<ShaderResourceStructMember> members;
    };

    struct ShaderResource
    {
        enum Type
        {
            TypeBuffer,
            TypeTexture,
            TypeSampler,
            TypeTextureSampler, // texture and sampler (combined)
        };
        enum Access
        {
            AccessReadOnly,
            AccessWriteOnly,
            AccessReadWrite,
        };

        uint32_t set;
        uint32_t binding;
        std::string name;
        Type type;
        uint32_t stages;

        uint32_t count; // array length
        uint32_t stride; // stride between array elements

        bool enabled;
        Access access;

        union
        {
            // only one of these is valid depending on the type.
            ShaderResourceBuffer buffer;
            ShaderResourceTexture texture;
            ShaderResourceThreadgroup threadgroup;
        } typeInfo;

        // struct members (struct only)
        std::vector<ShaderResourceStructMember> members;
    };

    struct ShaderPushConstantLayout
    {
        std::string name;
        uint32_t offset;
        uint32_t size;
        uint32_t stages;
        std::vector<ShaderResourceStructMember> members;
    };
}
