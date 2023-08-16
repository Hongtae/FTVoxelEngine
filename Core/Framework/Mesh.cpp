#include <vector>
#include <optional>
#include <tuple>
#include "Mesh.h"
#include "Scene.h"
#include "Logger.h"

using namespace FV;

VertexDescriptor Submesh::vertexDescriptor(const MaterialShaderMap& shaderMap) const
{
    auto vertexShader = shaderMap.function(ShaderStage::Vertex);
    if (vertexShader == nullptr)
        return {};

    auto& vertexInputs = vertexShader->stageInputAttributes();
    auto& attributeSemantics = shaderMap.inputs;

    auto findBufferIndexAttribute = [this](VertexAttributeSemantic s)->
        std::optional<std::tuple<uint32_t, VertexAttribute>>
    {
        for (uint32_t index = 0; index < vertexBuffers.size(); ++index)
        {
            auto& vb = vertexBuffers.at(index);
            if (vb.buffer == nullptr) 
                continue;
            for (auto& attr : vb.attributes)
            {
                if (attr.semantic == s)
                {
                    return std::make_tuple(index, attr);
                }
            }
        }
        return {};
    };
    auto findBufferIndexAttributeByName = [this](const std::string& name)->
        std::optional<std::tuple<uint32_t, VertexAttribute>>
    {
        for (uint32_t index = 0; index < vertexBuffers.size(); ++index)
        {
            auto& vb = vertexBuffers.at(index);
            if (vb.buffer == nullptr)
                continue;
            for (auto& attr : vb.attributes)
            {
                if (attr.name == name)
                {
                    return std::make_tuple(index, attr);
                }
            }
        }
        return {};
    };

    std::vector<VertexAttributeDescriptor> attributes;
    attributes.reserve(vertexInputs.size());
    for (auto& input : vertexInputs)
    {
        if (input.enabled == false)
            continue;

        auto it = attributeSemantics.find(input.location);        
        auto semantic = (it != attributeSemantics.end()) ? it->second : VertexAttributeSemantic::UserDefined;

        std::optional<std::tuple<uint32_t, VertexAttribute>> bufferIndexAttr = {};
        if (semantic == VertexAttributeSemantic::UserDefined && input.name.empty() == false)
            bufferIndexAttr = findBufferIndexAttributeByName(input.name);

        if (bufferIndexAttr.has_value() == false)
            bufferIndexAttr = findBufferIndexAttribute(semantic);

        if (bufferIndexAttr)
        {
            auto [bufferIndex, attr] = bufferIndexAttr.value();
            auto& buffer = vertexBuffers.at(bufferIndex);

            VertexAttributeDescriptor descriptor =
            {
                .format = attr.format,
                .offset = buffer.byteOffset + attr.offset,
                .bufferIndex = bufferIndex,
                .location = input.location
            };
            attributes.push_back(descriptor);
        }
        else
        {
            Log::error(std::format("Cannot bind vertex buffer at location: {} (name:{})",
                                   input.location, input.name));
        }        
    }

    std::vector<VertexBufferLayoutDescriptor> layouts;
    layouts.reserve(vertexBuffers.size());
    for (uint32_t bufferIndex = 0; bufferIndex < vertexBuffers.size(); ++bufferIndex)
    {
        auto& buffer = vertexBuffers.at(bufferIndex);

        VertexBufferLayoutDescriptor descriptor =
        {
            .step = VertexStepRate::Vertex,
            .stride = buffer.byteStride,
            .bufferIndex = bufferIndex
        };
        layouts.push_back(descriptor);
    }

    return { attributes, layouts };
}

bool Submesh::draw(RenderCommandEncoder* encoder, const SceneState& state, const Matrix4& tm) const
{
    return false;
}

void Mesh::draw(RenderCommandEncoder* encoder, const SceneState& state, const Matrix4& tm) const
{
    for (auto& mesh : submeshes)
    {
        mesh.draw(encoder, state, tm);
    }
}

