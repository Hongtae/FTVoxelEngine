#pragma once
#include "../include.h"
#include <vector>
#include <string>
#include <optional>
#include "Material.h"
#include "PipelineReflection.h"
#include "RenderCommandEncoder.h"
#include "VertexDescriptor.h"
#include "GraphicsDeviceContext.h"
#include "AABB.h"

namespace FV {
    struct SceneState;

    struct FVCORE_API Mesh {
        std::shared_ptr<Material> material;
        AABB aabb;

        struct VertexAttribute {
            VertexAttributeSemantic semantic;
            VertexFormat format;
            uint32_t offset;
            std::string name; /* optional */
        };
        struct VertexBuffer {
            uint32_t byteOffset;
            uint32_t byteStride;
            uint32_t vertexCount;
            std::shared_ptr<GPUBuffer> buffer;
            std::vector<VertexAttribute> attributes;
        };
        std::vector<VertexBuffer> vertexBuffers;

        std::shared_ptr<GPUBuffer> indexBuffer;
        uint32_t indexBufferByteOffset;
        uint32_t indexBufferBaseVertexIndex;
        uint32_t vertexStart;
        uint32_t indexCount;
        IndexType indexType;

        PrimitiveType primitiveType;

        VertexDescriptor vertexDescriptor() const;

        enum class BufferUsagePolicy {
            UseExternalBufferManually = 0,
            SingleBuffer,
            SingleBufferPerSet,
            SingleBufferPerResource,
        };

        bool initResources(GraphicsDevice*, BufferUsagePolicy);
        bool buildPipelineState(GraphicsDevice*, PipelineReflection* = nullptr);
        void updateShadingProperties(const SceneState*);

        bool encodeRenderCommand(RenderCommandEncoder* encoder, uint32_t numInstances, uint32_t baseInstance) const;

        // Enumerates the vertex buffer contents, and for each vertex,
        // the given handler is called, with a vertex attribute as an argument.
        // If the handler returns false, stop enumerating.
        bool enumerateVertexBufferContent(VertexAttributeSemantic semantic,
                                          CommandQueue* queue,
                                          std::function<bool(const void* data, VertexFormat format, uint32_t index)> handler) const;

    private:
        struct ResourceBinding {
            ShaderResource resource;    // from spir-v
            ShaderBinding binding;      // from descriptor-set layout
        };
        struct ResourceBindingSet {
            uint32_t index; // binding-set
            std::shared_ptr<ShaderBindingSet> bindingSet;
            std::vector<ResourceBinding> resources;
        };
        struct PushConstantData {
            ShaderPushConstantLayout layout;
            std::vector<uint8_t> data;
        };

        std::shared_ptr<RenderPipelineState> pipelineState;
        std::optional<PipelineReflection> pipelineReflection;

        std::vector<ResourceBindingSet> resourceBindings;
        std::vector<PushConstantData> pushConstants;

        struct BufferResource {
            std::string name;
            std::vector<ShaderBindingSet::BufferInfo> buffers;
        };
        std::unordered_map<ShaderBindingLocation, BufferResource> bufferResources;

        uint32_t bindMaterialTextures(MaterialSemantic semantic, const ShaderResource& resource, ShaderBindingSet* bindingSet) const;
        uint32_t bindMaterialSamplers(MaterialSemantic semantic, const ShaderResource& resource, ShaderBindingSet* bindingSet) const;
        uint32_t bindMaterialProperty(MaterialSemantic semantic, ShaderBindingLocation location, ShaderDataType type, const std::string& name, uint32_t itemOffset, uint8_t* buffer, size_t length) const;

        uint32_t bindShaderUniformTextures(ShaderUniformSemantic semantic, const std::string& name, const SceneState& sceneState, ShaderBindingSet* bindingSet) const;
        uint32_t bindShaderUniformSamplers(ShaderUniformSemantic semantic, const std::string& name, const SceneState& sceneState, ShaderBindingSet* bindingSet) const;
        uint32_t bindShaderUniformBuffer(ShaderUniformSemantic semantic, ShaderDataType type, const std::string& name, const SceneState&, uint8_t* buffer, size_t length) const;
    };
}
