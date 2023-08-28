#pragma once
#include "../include.h"
#include <vector>

namespace FV
{
    enum class VertexFormat
    {
        Invalid = 0,

        UChar2,
        UChar3,
        UChar4,

        Char2,
        Char3,
        Char4,

        UChar2Normalized,
        UChar3Normalized,
        UChar4Normalized,

        Char2Normalized,
        Char3Normalized,
        Char4Normalized,

        UShort2,
        UShort3,
        UShort4,

        Short2,
        Short3,
        Short4,

        UShort2Normalized,
        UShort3Normalized,
        UShort4Normalized,

        Short2Normalized,
        Short3Normalized,
        Short4Normalized,

        Half2,
        Half3,
        Half4,

        Float,
        Float2,
        Float3,
        Float4,

        Int,
        Int2,
        Int3,
        Int4,

        UInt,
        UInt2,
        UInt3,
        UInt4,

        Int1010102Normalized,
        UInt1010102Normalized,
    };

    struct VertexFormatInfo
    {
        uint32_t typeSize;
        uint32_t components;
        bool normalized;

        size_t bytes() const noexcept { return typeSize * components; }

        constexpr VertexFormatInfo(uint32_t s, uint32_t c = 1, bool n = false)
            : typeSize(s), components(c), normalized(n) {}

        constexpr VertexFormatInfo(VertexFormat format)
            : VertexFormatInfo(
                [format]() constexpr -> VertexFormatInfo
                {
                    switch (format)
                    {
                    case VertexFormat::UChar2:                return { 1, 2 };
                    case VertexFormat::UChar3:                return { 1, 3 };
                    case VertexFormat::UChar4:                return { 1, 4 };

                    case VertexFormat::Char2:                 return { 1, 2 };
                    case VertexFormat::Char3:                 return { 1, 3 };
                    case VertexFormat::Char4:                 return { 1, 4 };

                    case VertexFormat::UChar2Normalized:      return { 1, 2, true };
                    case VertexFormat::UChar3Normalized:      return { 1, 3, true };
                    case VertexFormat::UChar4Normalized:      return { 1, 4, true };

                    case VertexFormat::Char2Normalized:       return { 1, 2, true };
                    case VertexFormat::Char3Normalized:       return { 1, 3, true };
                    case VertexFormat::Char4Normalized:       return { 1, 4, true };

                    case VertexFormat::UShort2:               return { 2, 2 };
                    case VertexFormat::UShort3:               return { 2, 3 };
                    case VertexFormat::UShort4:               return { 2, 4 };

                    case VertexFormat::Short2:                return { 2, 2 };
                    case VertexFormat::Short3:                return { 2, 3 };
                    case VertexFormat::Short4:                return { 2, 4 };

                    case VertexFormat::UShort2Normalized:     return { 2, 2, true };
                    case VertexFormat::UShort3Normalized:     return { 2, 3, true };
                    case VertexFormat::UShort4Normalized:     return { 2, 4, true };

                    case VertexFormat::Short2Normalized:      return { 2, 2, true };
                    case VertexFormat::Short3Normalized:      return { 2, 3, true };
                    case VertexFormat::Short4Normalized:      return { 2, 4, true };

                    case VertexFormat::Half2:                 return { 2, 2 };
                    case VertexFormat::Half3:                 return { 2, 3 };
                    case VertexFormat::Half4:                 return { 2, 4 };

                    case VertexFormat::Float:                 return { 4, 1 };
                    case VertexFormat::Float2:                return { 4, 2 };
                    case VertexFormat::Float3:                return { 4, 3 };
                    case VertexFormat::Float4:                return { 4, 4 };

                    case VertexFormat::Int:                   return { 4, 1 };
                    case VertexFormat::Int2:                  return { 4, 2 };
                    case VertexFormat::Int3:                  return { 4, 3 };
                    case VertexFormat::Int4:                  return { 4, 4 };

                    case VertexFormat::UInt:                  return { 4, 1 };
                    case VertexFormat::UInt2:                 return { 4, 2 };
                    case VertexFormat::UInt3:                 return { 4, 3 };
                    case VertexFormat::UInt4:                 return { 4, 4 };

                    case VertexFormat::Int1010102Normalized:  return { 4, 1, true };
                    case VertexFormat::UInt1010102Normalized: return { 4, 1, true };

                    default:
                        FVASSERT_DESC_DEBUG(0, "Unknown format");
                        break;
                    }
                    return { 0, 0 };
                }())
        {
        }
    };

	enum class VertexStepRate
	{
		Vertex = 0,
		Instance,
	};

	struct VertexBufferLayoutDescriptor
	{
		VertexStepRate step;
		uint32_t stride;
		uint32_t bufferIndex;
	};

	struct VertexAttributeDescriptor
	{
		VertexFormat format;
		uint32_t offset;
		uint32_t bufferIndex;
		uint32_t location;
	};

	struct VertexDescriptor
	{
		std::vector<VertexAttributeDescriptor> attributes;
		std::vector<VertexBufferLayoutDescriptor> layouts;
	};
}
