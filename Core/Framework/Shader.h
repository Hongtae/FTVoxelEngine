#pragma once
#include "../include.h"
#include <vector>
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
        Shader(const uint32_t* data, size_t length); // share data
        ~Shader();

        bool compile();
        bool validate();

        ShaderStage stage;
        std::vector<uint8_t> data;

        // entry point functions
        std::vector<std::string> functions;

        std::vector<ShaderAttribute> inputAttributes;
        std::vector<ShaderAttribute> outputAttributes;
        std::vector<ShaderResource> resources;

        std::vector<ShaderPushConstantLayout> pushConstantLayouts;

        // for descriptor set layout bindings
        std::vector<ShaderDescriptor> descriptors;

        // compute-shader threadgroup
        struct { uint32_t x, y, z; } threadgroupSize;
    };
}
