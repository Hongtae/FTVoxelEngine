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

        std::shared_ptr<RenderPipelineState> pipelineState;
        std::optional<PipelineReflection> pipelineReflection;

        VertexDescriptor vertexDescriptor(const MaterialShaderMap&) const;
        bool draw(RenderCommandEncoder*, const SceneState&, const Matrix4&) const;
    };

    struct FVCORE_API Mesh
    {
        std::string name;
        std::vector<Submesh> submeshes; // primitives

        void draw(RenderCommandEncoder*, const SceneState&, const Matrix4&) const;
    };
}
