#pragma once
#include "../include.h"
#include <vector>
#include "GPUBuffer.h"
#include "Texture.h"
#include "Sampler.h"
#include "Shader.h"

namespace FV {
    struct ShaderBinding {
        uint32_t binding;
        ShaderDescriptorType type;
        uint32_t arrayLength; // array size or bytes of inline buffer
        std::shared_ptr<SamplerState> immutableSamplers;
    };

    struct ShaderBindingSetLayout {
        using Binding = ShaderBinding;
        std::vector<Binding> bindings;
    };

    class ShaderBindingSet {
    public:
        virtual ~ShaderBindingSet() = default;

        struct BufferInfo {
            std::shared_ptr<GPUBuffer> buffer;
            uint64_t offset;
            uint64_t length;
        };

        // bind buffers
        virtual void setBuffer(uint32_t binding, std::shared_ptr<GPUBuffer>, uint64_t offset, uint64_t length) = 0;
        virtual void setBufferArray(uint32_t binding, uint32_t numBuffers, BufferInfo*) = 0;

        // bind textures
        virtual void setTexture(uint32_t binding, std::shared_ptr<Texture>) = 0;
        virtual void setTextureArray(uint32_t binding, uint32_t numTextures, std::shared_ptr<Texture>*) = 0;

        // bind samplers
        virtual void setSamplerState(uint32_t binding, std::shared_ptr<SamplerState>) = 0;
        virtual void setSamplerStateArray(uint32_t binding, uint32_t numSamplers, std::shared_ptr<SamplerState>*) = 0;
    };
}
