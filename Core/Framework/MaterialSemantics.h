#pragma once
#include "../include.h"
#include <unordered_map>

namespace FV {
    enum class MaterialSemantic {
        UserDefined,
        BaseColor,
        BaseColorTexture,
        Metallic,
        Roughness,
        MetallicRoughnessTexture,
        NormalScaleFactor,
        NormalTexture,
        OcclusionScale,
        OcclusionTexture,
        EmissiveFactor,
        EmissiveTexture,
    };

    enum class ShaderUniformSemantic {
        ModelMatrix,
        ViewMatrix,
        ProjectionMatrix,
        ViewProjectionMatrix,
        ModelViewProjectionMatrix,
        InverseModelMatrix,
        InverseViewMatrix,
        InverseProjectionMatrix,
        InverseViewProjectionMatrix,
        InverseModelViewProjectionMatrix,
        TransformMatrixArray,
        DirectionalLightIndex,
        DirectionalLightDirection,
        DirectionalLightDiffuseColor,
        SpotLightIndex,
        SpotLightPosition,
        SpotLightAttenuation,
        SpotLightColor,
    };

    enum class VertexAttributeSemantic {
        UserDefined,
        Position,
        Normal,
        Color,
        TextureCoordinates,
        Tangent,
        Bitangent,
        BlendIndices,
        BlendWeights,
    };

    struct ShaderBindingLocation {
        uint32_t set;
        uint32_t binding;
        uint32_t offset;

        bool isPushConstant() const {
            return set == uint32_t(-1) && binding == uint32_t(-1);
        }
        static auto pushConstant(uint32_t offset) -> ShaderBindingLocation {
            return { uint32_t(-1), uint32_t(-1), offset };
        }
    };
}

namespace std {
    template <> struct hash<FV::ShaderBindingLocation> {
        FORCEINLINE size_t _leftRotate(size_t x, int c) const {
            return (((x) << (c)) | ((x) >> (sizeof(x) - (c))));
        }
        template <typename T>
        inline size_t hash_combine(const T& val, size_t h) const {
            return h ^ (_leftRotate(h, 5) + hash<T>{}(val));
        }
        size_t operator()(const FV::ShaderBindingLocation& loc) const {
            size_t s = 0x5be0cd19137e2179;
            s = hash_combine(loc.set, s);
            s = hash_combine(loc.binding, s);
            s = hash_combine(loc.offset, s);
            return s;
        }
    };
    template <> struct equal_to<FV::ShaderBindingLocation> {
        bool operator()(const FV::ShaderBindingLocation& a, const FV::ShaderBindingLocation& b) const {
            return a.set == b.set && a.binding == b.binding && a.offset == b.offset;
        }
    };

    template <> struct formatter<FV::ShaderBindingLocation> : formatter<string> {
        auto format(const FV::ShaderBindingLocation& arg, format_context& ctx) {
            std::string str;
            if (arg.isPushConstant())
                str = std::format("pushConstant, offset:{}", arg.offset);
            else
                str = std::format("set:{}, binding:{}, offset:{}", arg.set, arg.binding, arg.offset);
            return formatter<string>::format(str, ctx);
        }
    };
}
