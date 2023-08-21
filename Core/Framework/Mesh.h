#pragma once
#include "../include.h"
#include <vector>
#include <string>
#include <optional>
#include "Material.h"
#include "PipelineReflection.h"
#include "RenderCommandEncoder.h"
#include "VertexDescriptor.h"

namespace FV
{
    struct SceneState;

    struct FVCORE_API Submesh
    {
        std::shared_ptr<Material> material;

        struct VertexAttribute
        {
            VertexAttributeSemantic semantic;
            VertexFormat format;
            uint32_t offset;
            std::string name; /* optional */
        };
        struct VertexBuffer
        {
            uint32_t byteOffset;
            uint32_t byteLength;
            uint32_t byteStride;
            uint32_t vertexCount;
            std::shared_ptr<GPUBuffer> buffer;
            std::vector<VertexAttribute> attributes;
        };
        std::vector<VertexBuffer> vertexBuffers;

        std::shared_ptr<GPUBuffer> indexBuffer;
        uint32_t indexOffset;
        size_t indexCount;

        PrimitiveType primitiveType;

        VertexDescriptor vertexDescriptor() const;

        enum class BufferUsagePolicy
        {
            UseExternalBufferManually = 0,
            SingleBuffer,
            SingleBufferPerSet,
            SingleBufferPerResource,
        };

        bool initResources(GraphicsDevice*, BufferUsagePolicy);
        bool buildPipelineState(GraphicsDevice*);
        void updateShadingProperties(const SceneState*);

        bool draw(RenderCommandEncoder*, const SceneState&, const Matrix4&) const;

    private:
        struct ResourceBinding
        {
            ShaderResource resource;    // from spir-v
            ShaderBinding binding;      // from descriptor-set layout
        };
        struct ResourceBindingSet
        {
            uint32_t index; // binding-set
            std::shared_ptr<ShaderBindingSet> bindingSet;
            std::vector<ResourceBinding> resources;
        };
        struct PushConstantData
        {
            ShaderPushConstantLayout layout;
            std::vector<uint8_t> data;
        };

        std::shared_ptr<RenderPipelineState> pipelineState;
        std::optional<PipelineReflection> pipelineReflection;

        std::vector<ResourceBindingSet> resourceBindings;
        std::vector<PushConstantData> pushConstants;

        struct BufferResource
        {
            std::string name;
            std::vector<ShaderBindingSet::BufferInfo> buffers;
        };
        std::unordered_map<ShaderBindingLocation, BufferResource> bufferResources;
    };

    struct FVCORE_API Mesh
    {
        std::string name;
        std::vector<Submesh> submeshes; // primitives

        void draw(RenderCommandEncoder*, const SceneState&, const Matrix4&) const;
    };
}
