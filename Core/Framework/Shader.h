#pragma once
#include "../include.h"
#include <vector>
#include <cstdint>
#include <filesystem>
#include "ShaderResource.h"

namespace FV
{
    struct ShaderAttribute
    {
        std::string name;
        uint32_t location;
        ShaderDataType type;
        bool enabled;
    };

    enum class ShaderDescriptorType
    {
        UniformBuffer,
        StorageBuffer,
        StorageTexture,
        UniformTexelBuffer, // readonly texture 'buffer'
        StorageTexelBuffer, // writable texture 'buffer'
        TextureSampler, // texture, sampler combined
        Texture,
        Sampler,
    };

    struct ShaderDescriptor
    {
        uint32_t set;
        uint32_t binding;
        uint32_t count; // array size
        ShaderDescriptorType type;
    };

    class FVCORE_API Shader
    {
    public:
        Shader();
        Shader(const std::filesystem::path& path);
        Shader(const std::vector<uint32_t>&);
        Shader(const uint32_t* ir, size_t words);
        ~Shader();

        bool validate();
        bool isValid() const;

        ShaderStage stage() const { return _stage;  }
        const std::vector<uint32_t>& data() const { return _data; }

        // entry point functions
        const std::vector<std::string>& functions() const { return _functions; }

        const std::vector<ShaderAttribute>& inputAttributes() const { return _inputAttributes; }
        const std::vector<ShaderAttribute>& outputAttributes() const { return _outputAttributes; }
        const std::vector<ShaderResource>& resources() const { return _resources; }

        const std::vector<ShaderPushConstantLayout>& pushConstantLayouts() const { return _pushConstantLayouts; }

        // for descriptor set layout bindings
        const std::vector<ShaderDescriptor>& descriptors() const { return _descriptors; }

        // compute-shader threadgroup
        auto threadgroupSize() const { return _threadgroupSize; }

    private:
        bool compile();

        ShaderStage _stage;
        std::vector<uint32_t> _data;

        std::vector<std::string> _functions;

        std::vector<ShaderAttribute> _inputAttributes;
        std::vector<ShaderAttribute> _outputAttributes;
        std::vector<ShaderResource> _resources;

        std::vector<ShaderPushConstantLayout> _pushConstantLayouts;
        std::vector<ShaderDescriptor> _descriptors;
        struct { uint32_t x, y, z; } _threadgroupSize;
    };
}
