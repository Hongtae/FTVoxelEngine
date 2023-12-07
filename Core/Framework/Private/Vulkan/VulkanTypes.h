#pragma once
#include "../../Shader.h"
#include "../../ShaderResource.h"
#include "../../VertexDescriptor.h"
#include "../../Logger.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV {
    inline ShaderDescriptorType getDescriptorType(VkDescriptorType type) {
        switch (type) {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            return ShaderDescriptorType::Sampler;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            return ShaderDescriptorType::TextureSampler;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            return ShaderDescriptorType::Texture;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            return ShaderDescriptorType::StorageTexture;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            return ShaderDescriptorType::UniformTexelBuffer;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            return ShaderDescriptorType::StorageTexelBuffer;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            return ShaderDescriptorType::UniformBuffer;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            return ShaderDescriptorType::StorageBuffer;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            FVASSERT_DESC_DEBUG(0, "Not implemented yet");
            break;
        default:
            Log::error("Unknown DescriptorType!!");
            FVASSERT_DESC_DEBUG(0, "Unknown descriptor type!");
        }
        return {};
    }
    inline VkDescriptorType getVkDescriptorType(ShaderDescriptorType type) {
        switch (type) {
        case ShaderDescriptorType::UniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case ShaderDescriptorType::StorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case ShaderDescriptorType::StorageTexture:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case ShaderDescriptorType::TextureSampler:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case ShaderDescriptorType::Texture:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case ShaderDescriptorType::UniformTexelBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case ShaderDescriptorType::StorageTexelBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case ShaderDescriptorType::Sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        default:
            Log::error("Unknown DescriptorType!!");
            FVASSERT_DESC_DEBUG(0, "Unknown descriptor type!");
        }
        return {};
    }

    inline VkFormat getVkFormat(VertexFormat fmt) {
        switch (fmt) {
        case VertexFormat::UChar:                   return VK_FORMAT_R8_UINT;
        case VertexFormat::UChar2:				    return VK_FORMAT_R8G8_UINT;
        case VertexFormat::UChar3: 				    return VK_FORMAT_R8G8B8_UINT;
        case VertexFormat::UChar4: 				    return VK_FORMAT_R8G8B8A8_UINT;
        case VertexFormat::Char:                    return VK_FORMAT_R8_SINT;
        case VertexFormat::Char2: 				    return VK_FORMAT_R8G8_SINT;
        case VertexFormat::Char3: 				    return VK_FORMAT_R8G8B8_SINT;
        case VertexFormat::Char4: 				    return VK_FORMAT_R8G8B8A8_SINT;
        case VertexFormat::UCharNormalized:         return VK_FORMAT_R8_UNORM;
        case VertexFormat::UChar2Normalized: 		return VK_FORMAT_R8G8_UNORM;
        case VertexFormat::UChar3Normalized: 		return VK_FORMAT_R8G8B8_UNORM;
        case VertexFormat::UChar4Normalized: 		return VK_FORMAT_R8G8B8A8_UNORM;
        case VertexFormat::CharNormalized:          return VK_FORMAT_R8_SNORM;
        case VertexFormat::Char2Normalized: 		return VK_FORMAT_R8G8_SNORM;
        case VertexFormat::Char3Normalized: 		return VK_FORMAT_R8G8B8_SNORM;
        case VertexFormat::Char4Normalized: 		return VK_FORMAT_R8G8B8A8_SNORM;
        case VertexFormat::UShort:                  return VK_FORMAT_R16_UINT;
        case VertexFormat::UShort2: 				return VK_FORMAT_R16G16_UINT;
        case VertexFormat::UShort3: 				return VK_FORMAT_R16G16B16_UINT;
        case VertexFormat::UShort4: 				return VK_FORMAT_R16G16B16A16_UINT;
        case VertexFormat::Short:                   return VK_FORMAT_R16_SINT;
        case VertexFormat::Short2: 				    return VK_FORMAT_R16G16_SINT;
        case VertexFormat::Short3: 				    return VK_FORMAT_R16G16B16_SINT;
        case VertexFormat::Short4: 				    return VK_FORMAT_R16G16B16A16_SINT;
        case VertexFormat::UShortNormalized:        return VK_FORMAT_R16_UNORM;
        case VertexFormat::UShort2Normalized: 	    return VK_FORMAT_R16G16_UNORM;
        case VertexFormat::UShort3Normalized: 	    return VK_FORMAT_R16G16B16_UNORM;
        case VertexFormat::UShort4Normalized: 	    return VK_FORMAT_R16G16B16A16_UNORM;
        case VertexFormat::ShortNormalized:         return VK_FORMAT_R16_SNORM;
        case VertexFormat::Short2Normalized: 		return VK_FORMAT_R16G16_SNORM;
        case VertexFormat::Short3Normalized: 		return VK_FORMAT_R16G16B16_SNORM;
        case VertexFormat::Short4Normalized: 		return VK_FORMAT_R16G16B16A16_SNORM;
        case VertexFormat::Half:                    return VK_FORMAT_R16_SFLOAT;
        case VertexFormat::Half2: 				    return VK_FORMAT_R16G16_SFLOAT;
        case VertexFormat::Half3: 				    return VK_FORMAT_R16G16B16_SFLOAT;
        case VertexFormat::Half4: 				    return VK_FORMAT_R16G16B16A16_SFLOAT;
        case VertexFormat::Float: 				    return VK_FORMAT_R32_SFLOAT;
        case VertexFormat::Float2: 				    return VK_FORMAT_R32G32_SFLOAT;
        case VertexFormat::Float3: 				    return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexFormat::Float4: 				    return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VertexFormat::Int: 					return VK_FORMAT_R32_SINT;
        case VertexFormat::Int2: 					return VK_FORMAT_R32G32_SINT;
        case VertexFormat::Int3: 					return VK_FORMAT_R32G32B32_SINT;
        case VertexFormat::Int4: 					return VK_FORMAT_R32G32B32A32_SINT;
        case VertexFormat::UInt: 					return VK_FORMAT_R32_UINT;
        case VertexFormat::UInt2: 				    return VK_FORMAT_R32G32_UINT;
        case VertexFormat::UInt3: 				    return VK_FORMAT_R32G32B32_UINT;
        case VertexFormat::UInt4: 				    return VK_FORMAT_R32G32B32A32_UINT;
        case VertexFormat::Int1010102Normalized: 	return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
        case VertexFormat::UInt1010102Normalized:   return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        }
        FVASSERT_DESC_DEBUG(0, "Unknown type! (or not implemented yet)");
        return VK_FORMAT_UNDEFINED;
    }

    inline VkFormat getVkFormat(PixelFormat fmt) {
        switch (fmt) {
        case PixelFormat::R8Unorm:			return VK_FORMAT_R8_UNORM;
        case PixelFormat::R8Snorm:			return VK_FORMAT_R8_SNORM;
        case PixelFormat::R8Uint:			return VK_FORMAT_R8_UINT;
        case PixelFormat::R8Sint:			return VK_FORMAT_R8_SINT;

        case PixelFormat::R16Unorm:			return VK_FORMAT_R16_UNORM;
        case PixelFormat::R16Snorm:			return VK_FORMAT_R16_SNORM;
        case PixelFormat::R16Uint:			return VK_FORMAT_R16_UINT;
        case PixelFormat::R16Sint:			return VK_FORMAT_R16_SINT;
        case PixelFormat::R16Float:			return VK_FORMAT_R16_SFLOAT;

        case PixelFormat::RG8Unorm:			return VK_FORMAT_R8G8_UNORM;
        case PixelFormat::RG8Snorm:			return VK_FORMAT_R8G8_SNORM;
        case PixelFormat::RG8Uint:			return VK_FORMAT_R8G8_UINT;
        case PixelFormat::RG8Sint:			return VK_FORMAT_R8G8_SINT;

        case PixelFormat::R32Uint:			return VK_FORMAT_R32_UINT;
        case PixelFormat::R32Sint:			return VK_FORMAT_R32_SINT;
        case PixelFormat::R32Float:			return VK_FORMAT_R32_SFLOAT;

        case PixelFormat::RG16Unorm:		return VK_FORMAT_R16G16_UNORM;
        case PixelFormat::RG16Snorm:		return VK_FORMAT_R16G16_SNORM;
        case PixelFormat::RG16Uint:			return VK_FORMAT_R16G16_UINT;
        case PixelFormat::RG16Sint:			return VK_FORMAT_R16G16_SINT;
        case PixelFormat::RG16Float:		return VK_FORMAT_R16G16_SFLOAT;

        case PixelFormat::RGBA8Unorm:		return VK_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::RGBA8Unorm_srgb:	return VK_FORMAT_R8G8B8A8_SRGB;
        case PixelFormat::RGBA8Snorm:		return VK_FORMAT_R8G8B8A8_SNORM;
        case PixelFormat::RGBA8Uint:		return VK_FORMAT_R8G8B8A8_UINT;
        case PixelFormat::RGBA8Sint:		return VK_FORMAT_R8G8B8A8_SINT;

        case PixelFormat::BGRA8Unorm:		return VK_FORMAT_B8G8R8A8_UNORM;
        case PixelFormat::BGRA8Unorm_srgb:	return VK_FORMAT_B8G8R8A8_SRGB;

        case PixelFormat::RGB10A2Unorm:		return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case PixelFormat::RGB10A2Uint:		return VK_FORMAT_A2B10G10R10_UINT_PACK32;

        case PixelFormat::RG11B10Float:		return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case PixelFormat::RGB9E5Float:		return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;

        case PixelFormat::RG32Uint:			return VK_FORMAT_R32G32_UINT;
        case PixelFormat::RG32Sint:			return VK_FORMAT_R32G32_SINT;
        case PixelFormat::RG32Float:		return VK_FORMAT_R32G32_SFLOAT;

        case PixelFormat::RGBA16Unorm:		return VK_FORMAT_R16G16B16A16_UNORM;
        case PixelFormat::RGBA16Snorm:		return VK_FORMAT_R16G16B16A16_SNORM;
        case PixelFormat::RGBA16Uint:		return VK_FORMAT_R16G16B16A16_UINT;
        case PixelFormat::RGBA16Sint:		return VK_FORMAT_R16G16B16A16_SINT;
        case PixelFormat::RGBA16Float:		return VK_FORMAT_R16G16B16A16_SFLOAT;

        case PixelFormat::RGBA32Uint:		return VK_FORMAT_R32G32B32A32_UINT;
        case PixelFormat::RGBA32Sint:		return VK_FORMAT_R32G32B32A32_SINT;
        case PixelFormat::RGBA32Float:		return VK_FORMAT_R32G32B32A32_SFLOAT;

        case PixelFormat::Depth32Float:     return VK_FORMAT_D32_SFLOAT;
        case PixelFormat::Stencil8:         return VK_FORMAT_S8_UINT;

        case PixelFormat::Depth24Unorm_stencil8:    return VK_FORMAT_D24_UNORM_S8_UINT;
        case PixelFormat::Depth32Float_stencil8:    return VK_FORMAT_D32_SFLOAT_S8_UINT;
        }
        return VK_FORMAT_UNDEFINED;
    }

    inline PixelFormat getPixelFormat(VkFormat fmt) {
        switch (fmt) {
        case VK_FORMAT_R8_UNORM:					return PixelFormat::R8Unorm;
        case VK_FORMAT_R8_SNORM:					return PixelFormat::R8Snorm;
        case VK_FORMAT_R8_UINT:						return PixelFormat::R8Uint;
        case VK_FORMAT_R8_SINT:						return PixelFormat::R8Sint;

        case VK_FORMAT_R16_UNORM:					return PixelFormat::R16Unorm;
        case VK_FORMAT_R16_SNORM:					return PixelFormat::R16Snorm;
        case VK_FORMAT_R16_UINT:					return PixelFormat::R16Uint;
        case VK_FORMAT_R16_SINT:					return PixelFormat::R16Sint;
        case VK_FORMAT_R16_SFLOAT:					return PixelFormat::R16Float;

        case VK_FORMAT_R8G8_UNORM:					return PixelFormat::RG8Unorm;
        case VK_FORMAT_R8G8_SNORM:					return PixelFormat::RG8Snorm;
        case VK_FORMAT_R8G8_UINT:					return PixelFormat::RG8Uint;
        case VK_FORMAT_R8G8_SINT:					return PixelFormat::RG8Sint;

        case VK_FORMAT_R32_UINT:					return PixelFormat::R32Uint;
        case VK_FORMAT_R32_SINT:					return PixelFormat::R32Sint;
        case VK_FORMAT_R32_SFLOAT:					return PixelFormat::R32Float;

        case VK_FORMAT_R16G16_UNORM:				return PixelFormat::RG16Unorm;
        case VK_FORMAT_R16G16_SNORM:				return PixelFormat::RG16Snorm;
        case VK_FORMAT_R16G16_UINT:					return PixelFormat::RG16Uint;
        case VK_FORMAT_R16G16_SINT:					return PixelFormat::RG16Sint;
        case VK_FORMAT_R16G16_SFLOAT:				return PixelFormat::RG16Float;

        case VK_FORMAT_R8G8B8A8_UNORM:				return PixelFormat::RGBA8Unorm;
        case VK_FORMAT_R8G8B8A8_SRGB:				return PixelFormat::RGBA8Unorm_srgb;
        case VK_FORMAT_R8G8B8A8_SNORM:				return PixelFormat::RGBA8Snorm;
        case VK_FORMAT_R8G8B8A8_UINT:				return PixelFormat::RGBA8Uint;
        case VK_FORMAT_R8G8B8A8_SINT:				return PixelFormat::RGBA8Sint;

        case VK_FORMAT_B8G8R8A8_UNORM:				return PixelFormat::BGRA8Unorm;
        case VK_FORMAT_B8G8R8A8_SRGB:				return PixelFormat::BGRA8Unorm_srgb;

        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:	return PixelFormat::RGB10A2Unorm;
        case VK_FORMAT_A2B10G10R10_UINT_PACK32:		return PixelFormat::RGB10A2Uint;

        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:		return PixelFormat::RG11B10Float;
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:		return PixelFormat::RGB9E5Float;

        case VK_FORMAT_R32G32_UINT:					return PixelFormat::RG32Uint;
        case VK_FORMAT_R32G32_SINT:					return PixelFormat::RG32Sint;
        case VK_FORMAT_R32G32_SFLOAT:				return PixelFormat::RG32Float;

        case VK_FORMAT_R16G16B16A16_UNORM:			return PixelFormat::RGBA16Unorm;
        case VK_FORMAT_R16G16B16A16_SNORM:			return PixelFormat::RGBA16Snorm;
        case VK_FORMAT_R16G16B16A16_UINT:			return PixelFormat::RGBA16Uint;
        case VK_FORMAT_R16G16B16A16_SINT:			return PixelFormat::RGBA16Sint;
        case VK_FORMAT_R16G16B16A16_SFLOAT:			return PixelFormat::RGBA16Float;

        case VK_FORMAT_R32G32B32A32_UINT:			return PixelFormat::RGBA32Uint;
        case VK_FORMAT_R32G32B32A32_SINT:			return PixelFormat::RGBA32Sint;
        case VK_FORMAT_R32G32B32A32_SFLOAT:			return PixelFormat::RGBA32Float;

        case VK_FORMAT_D32_SFLOAT:					return PixelFormat::Depth32Float;
        case VK_FORMAT_S8_UINT:                     return PixelFormat::Stencil8;

        case VK_FORMAT_D24_UNORM_S8_UINT:           return PixelFormat::Depth24Unorm_stencil8;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:			return PixelFormat::Depth32Float_stencil8;

        }
        return PixelFormat::Invalid;
    }

    inline void appendNextChain(void* target, void* next) {
        VkBaseOutStructure* p = (VkBaseOutStructure*)target;
        while (p->pNext != nullptr) {
            p = p->pNext;
        }
        p->pNext = (VkBaseOutStructure*)next;
    }

    template <typename T>
    inline void enumerateNextChain(const void* target, T&& callback) {
        for (auto p = (const VkBaseInStructure*)target; p; p = p->pNext) {
            callback(p->sType, (const void*)p);
            p = p->pNext;
        }
    }
}
#endif
