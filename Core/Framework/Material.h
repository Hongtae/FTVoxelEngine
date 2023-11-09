#pragma once
#include "../include.h"
#include <vector>
#include <variant>
#include <unordered_map>
#include "RenderPass.h"
#include "RenderPipeline.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Quaternion.h"
#include "Matrix2.h"
#include "Matrix3.h"
#include "Matrix4.h"
#include "Float16.h"
#include "Texture.h"
#include "Sampler.h"
#include "GPUBuffer.h"
#include "ShaderBindingSet.h"
#include "MaterialSemantics.h"
#include "Logger.h"

namespace FV {
    struct MaterialProperty {
        MaterialSemantic semantic;

        using Buffer = std::vector<char8_t>; // ShaderDataType::Struct
        using Int8Array = std::vector<int8_t>;
        using UInt8Array = std::vector<uint8_t>;
        using Int16Array = std::vector<int16_t>;
        using UInt16Array = std::vector<uint16_t>;
        using Int32Array = std::vector<int32_t>;
        using UInt32Array = std::vector<uint32_t>;
        using Int64Array = std::vector<int32_t>;
        using UInt64Array = std::vector<uint32_t>;
        using HalfArray = std::vector<Float16>;
        using FloatArray = std::vector<float>;
        using DoubleArray = std::vector<double>;

        struct CombinedTextureSampler {
            std::shared_ptr<Texture> texture;
            std::shared_ptr<SamplerState> sampler;
        };
        using TextureArray = std::vector<std::shared_ptr<Texture>>;
        using SamplerArray = std::vector<std::shared_ptr<SamplerState>>;
        using CombinedTextureSamplerArray = std::vector<CombinedTextureSampler>;

        std::variant<std::monostate,
            Buffer,
            Int8Array,
            UInt8Array,
            Int16Array,
            UInt16Array,
            Int32Array,
            UInt32Array,
            HalfArray,
            FloatArray,
            TextureArray,
            SamplerArray,
            CombinedTextureSamplerArray> value;

        template <typename T, size_t Length>
        constexpr MaterialProperty(MaterialSemantic s, const T(&v)[Length])
            : semantic(s), value(std::vector<T>(&v[0], &v[Length])) {
        }
        template <typename T>
        constexpr MaterialProperty(MaterialSemantic s, const T* v, size_t len)
            : semantic(s), value(std::vector<T>(&v[0], &v[len])) {
        }
        template <typename InputIt>
        constexpr MaterialProperty(MaterialSemantic s, InputIt first, InputIt last)
            : semantic(s), value(first, last) {
        }
        template <typename T>
        constexpr MaterialProperty(MaterialSemantic s, const std::vector<T>& vector)
            : semantic(s), value(vector) {
        }

        MaterialProperty(MaterialSemantic s, const void* data, size_t len)
            : MaterialProperty(s, (const char8_t*)data, len) {
        }
        MaterialProperty(MaterialSemantic s, std::shared_ptr<Texture> texture)
            : MaterialProperty(s, &texture, 1) {
        }
        MaterialProperty(MaterialSemantic s, std::shared_ptr<SamplerState> sampler)
            : MaterialProperty(s, &sampler, 1) {
        }
        MaterialProperty(MaterialSemantic s, const CombinedTextureSampler& textureSampler)
            : MaterialProperty(s, &textureSampler, 1) {
        }

        MaterialProperty(MaterialSemantic s, float v)
            : MaterialProperty(s, &v, 1) {
        }
        MaterialProperty(MaterialSemantic s, const Vector2& v)
            : MaterialProperty(s, v.val) {
        }
        MaterialProperty(MaterialSemantic s, const Vector3& v)
            : MaterialProperty(s, v.val) {
        }
        MaterialProperty(MaterialSemantic s, const Vector4& v)
            : MaterialProperty(s, v.val) {
        }
        MaterialProperty(MaterialSemantic s, const Color& v)
            : MaterialProperty(s, v.val) {
        }
        MaterialProperty(MaterialSemantic s, const Quaternion& v)
            : MaterialProperty(s, v.val) {
        }
        MaterialProperty(MaterialSemantic s, const Matrix2& v)
            : MaterialProperty(s, v.val) {
        }
        MaterialProperty(MaterialSemantic s, const Matrix3& v)
            : MaterialProperty(s, v.val) {
        }
        MaterialProperty(MaterialSemantic s, const Matrix4& v)
            : MaterialProperty(s, v.val) {
        }
        MaterialProperty()
            : semantic(MaterialSemantic::UserDefined), value(std::monostate{}) {
        }
           
        template <typename R, typename T>
        std::vector<R> map(T&& fn) {
            return std::visit(
                [&](auto&& arg) {
                    using U = std::decay_t<decltype(arg)>;
                    constexpr bool isVector = requires {
                        requires requires(typename U::value_type v) {
                            { fn(v) }->std::convertible_to<R>;
                        };
                        requires std::same_as<U, std::vector<typename U::value_type, typename U::allocator_type>>;
                    };
                    std::vector<R> output;
                    if constexpr (isVector) {
                        output.reserve(arg.size());
                        for (auto v : arg)
                            output.push_back(fn(v));
                    }
                    return output;
                }, value);
        }

        template <typename T>
        std::vector<T> cast() const {
            return std::visit(
                [this](auto&& arg) {
                    using U = std::decay_t<decltype(arg)>;
                    constexpr bool convertible = requires {
                        typename U::value_type;
                        requires std::convertible_to<typename U::value_type, T>;
                    };
                    std::vector<T> output;
                    if constexpr (convertible) {
                        output.reserve(arg.size());
                        for (auto v : arg) output.push_back(static_cast<T>(v));
                    }
                    return output;
                }, value);
        }

        template <typename T>
        bool isConvertible() const {
            return std::visit(
                [this](auto&& arg) {
                    using U = std::decay_t<decltype(arg)>;
                    constexpr bool convertible = requires {
                        requires std::convertible_to<typename U::value_type, T>;
                    };
                    if constexpr (convertible) return true;
                    return false;
                }, value);
        }

        struct UnderlyingData {
            const void* data;
            size_t elementSize;
            size_t count;
        };
        UnderlyingData underlyingData() const {
            return std::visit(
                [](auto&& arg) -> UnderlyingData {
                    using T = std::decay_t<decltype(arg)>;
                    constexpr bool isNumericVector = requires {
                        arg.data();
                        arg.size();
                        typename T::value_type;
                        requires std::is_integral_v<typename T::value_type> || std::is_floating_point_v<typename T::value_type>;
                    };
                    if constexpr (isNumericVector) 
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    return { nullptr, 0, 0 };
                }, value);
        }
    };

    struct MaterialShaderMap {
        using BindingLocation = ShaderBindingLocation;

        struct Function {
            std::shared_ptr<ShaderFunction> function;
            std::vector<ShaderDescriptor> descriptors;
        };
        std::vector<Function> functions;

        using Semantic = std::variant<std::monostate, MaterialSemantic, ShaderUniformSemantic>;
        std::unordered_map<BindingLocation, Semantic> resourceSemantics;
        std::unordered_map<uint32_t, VertexAttributeSemantic> inputAttributeSemantics;

        std::shared_ptr<ShaderFunction> function(ShaderStage stage) const {
            for (auto& fn : functions) {
                if (fn.function && fn.function->stage() == stage)
                    return fn.function;
            }
            return nullptr;
        }

        std::optional<ShaderDescriptor> descriptor(BindingLocation location,
                                                   uint32_t stages) const {
            for (auto& fn : functions) {
                if (fn.function == nullptr)
                    continue;
                if (uint32_t(fn.function->stage()) & stages) {
                    for (auto& descriptor : fn.descriptors) {
                        if (descriptor.set == location.set &&
                            descriptor.binding == location.binding)
                            return descriptor;
                    }
                }
            }
            return {};
        }
    };

    struct Material {
        using Semantic = MaterialSemantic;
        using Property = MaterialProperty;
        using ShaderMap = MaterialShaderMap;
        using BindingLocation = ShaderBindingLocation;

        void setProperty(const Property& prop) {
            properties[prop.semantic] = prop;
        }
        template <typename... Args>
        constexpr void setProperty(Semantic semantic, Args&&... args) {
            properties[semantic] = Property(semantic, std::forward<Args>(args)...);
        }
        void setProperty(const BindingLocation& loc, const Property& prop) {
            userDefinedProperties[loc] = prop;
        }
        template <typename... Args>
        constexpr void setProperty(const BindingLocation& loc, Args&&... args) {
            userDefinedProperties[loc] = Property(Semantic::UserDefined, std::forward<Args>(args)...);
        }

        std::string name;
        struct RenderPassAttachment {
            PixelFormat format;
            BlendState blendState;
        };
        std::vector<RenderPassAttachment> attachments = {
            { PixelFormat::RGBA8Unorm, BlendState::alphaBlend }
        };
        PixelFormat depthFormat = PixelFormat::Depth24Unorm_stencil8;
        TriangleFillMode triangleFillMode = TriangleFillMode::Fill;
        CullMode cullMode = CullMode::None;
        Winding frontFace = Winding::Clockwise;

        std::unordered_map<Semantic, Property> properties;
        std::unordered_map<BindingLocation, Property> userDefinedProperties;

        std::shared_ptr<Texture> defaultTexture;
        std::shared_ptr<SamplerState> defaultSampler;

        ShaderMap shader;
    };
}
