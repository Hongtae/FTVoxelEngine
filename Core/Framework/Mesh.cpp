#include <vector>
#include <optional>
#include <tuple>
#include <functional>
#include <algorithm>
#include "Mesh.h"
#include "Scene.h"
#include "Logger.h"
#include "GraphicsDevice.h"

using namespace FV;

VertexDescriptor Submesh::vertexDescriptor() const
{
    if (material == nullptr)
        return {};

    auto vertexFunction = material->shader.function(ShaderStage::Vertex);
    if (vertexFunction == nullptr)
        return {};

    auto& vertexInputs = vertexFunction->stageInputAttributes();
    auto& attributeSemantics = material->shader.inputAttributeSemantics;

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
                .offset = attr.offset,
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

bool Submesh::initResources(GraphicsDevice* device, BufferUsagePolicy policy)
{
    if (material == nullptr)
        return false;
    if (pipelineState == nullptr)
        buildPipelineState(device);
    if (pipelineState == nullptr)
        return false;

    auto alignAddressNPOT = [](uintptr_t ptr, uintptr_t alignment)->uintptr_t
    {
        if (ptr % alignment)
            ptr += alignment - (ptr % alignment);
        return ptr;
    };
    auto alignAddress = [](uintptr_t ptr, uintptr_t alignment)->uintptr_t
    {
        return (ptr + alignment - 1) & ~(alignment - 1);
    };

    size_t numBuffersGenerated = 0;
    size_t totalBytesAllocated = 0;

    if (policy == BufferUsagePolicy::SingleBuffer)
    {
        decltype(bufferResources) resourceMap;

        size_t bufferOffset = 0;
        size_t bufferLength = 0;
        for (const ResourceBindingSet& bset : resourceBindings)
        {
            for (const ResourceBinding& rb : bset.resources)
            {
                if (rb.resource.type == ShaderResource::TypeBuffer)
                {
                    auto buffer = BufferResource{ rb.resource.name };

                    for (uint32_t i = 0; i < rb.resource.count; ++i)
                    {
                        ShaderBindingSet::BufferInfo bufferInfo = { };
                        bufferInfo.length = rb.resource.typeInfo.buffer.size;
                        bufferInfo.offset = bufferOffset;
                        buffer.buffers.push_back(bufferInfo);

                        bufferOffset += rb.resource.stride;
                    }
                    bufferLength = bufferOffset + rb.resource.typeInfo.buffer.size;
                    bufferLength = alignAddress(bufferLength, 16);
                    bufferOffset = alignAddress(bufferOffset, 16);

                    ShaderBindingLocation location
                    {
                        rb.resource.set, rb.resource.binding, 0 
                    };
                    resourceMap.insert_or_assign(location, buffer);
                }
            }
        }
        if (bufferLength > 0)
        {
            auto buffer = device->makeBuffer(bufferLength,
                                             GPUBuffer::StorageModeShared,
                                             CPUCacheModeWriteCombined);
            if (buffer == nullptr)
            {
                Log::error(std::format("failed to make buffer with length:{}",
                                       bufferLength));
                return false;
            }
            numBuffersGenerated++;
            totalBytesAllocated += bufferLength;

            for (auto it = resourceMap.begin(); it != resourceMap.end(); ++it)
            {
                BufferResource rb = it->second;
                for (auto& b : rb.buffers)
                    b.buffer = buffer;
                this->bufferResources.insert_or_assign(it->first, rb);
            }
        }
    }
    else if (policy == BufferUsagePolicy::SingleBufferPerSet)
    {
        decltype(bufferResources) resourceMap;

        for (const ResourceBindingSet& bset : resourceBindings)
        {
            size_t bufferOffset = 0;
            size_t bufferLength = 0;

            for (const ResourceBinding& rb : bset.resources)
            {
                if (rb.resource.type == ShaderResource::TypeBuffer)
                {
                    auto buffer = BufferResource{ rb.resource.name };

                    for (uint32_t i = 0; i < rb.resource.count; ++i)
                    {
                        ShaderBindingSet::BufferInfo bufferInfo = { };
                        bufferInfo.length = rb.resource.typeInfo.buffer.size;
                        bufferInfo.offset = bufferOffset;
                        buffer.buffers.push_back(bufferInfo);

                        bufferOffset += rb.resource.stride;
                    }
                    bufferLength = bufferOffset + rb.resource.typeInfo.buffer.size;
                    bufferLength = alignAddress(bufferLength, 16);
                    bufferOffset = alignAddress(bufferOffset, 16);

                    ShaderBindingLocation location 
                    {
                        rb.resource.set, rb.resource.binding, 0 
                    };
                    resourceMap.insert_or_assign(location, buffer);
                }
            }
            if (bufferLength > 0)
            {
                auto buffer = device->makeBuffer(bufferLength,
                                                 GPUBuffer::StorageModeShared,
                                                 CPUCacheMode::CPUCacheModeWriteCombined);
                if (buffer == nullptr)
                {
                    Log::error(std::format("failed to make buffer with length:{}",
                                           bufferLength));
                    return false;
                }
                numBuffersGenerated++;
                totalBytesAllocated += bufferLength;

                for (auto it = resourceMap.begin(); it != resourceMap.end(); ++it)
                {
                    auto& rb = it->second;
                    for (auto& b : rb.buffers)
                        if (b.buffer == nullptr)
                            b.buffer = buffer;
                }
            }
        }
        for (auto it = resourceMap.begin(); it != resourceMap.end(); ++it)
        {
            this->bufferResources.insert_or_assign(it->first, it->second);
        }
    }
    else if (policy == BufferUsagePolicy::SingleBufferPerResource)
    {
        decltype(bufferResources) resourceMap;

        for (const ResourceBindingSet& bset : resourceBindings)
        {
            for (const ResourceBinding& rb : bset.resources)
            {
                if (rb.resource.type == ShaderResource::TypeBuffer)
                {
                    size_t bufferOffset = 0;
                    size_t bufferLength = 0;

                    auto buffer = BufferResource{ rb.resource.name };

                    for (uint32_t i = 0; i < rb.resource.count; ++i)
                    {
                        ShaderBindingSet::BufferInfo bufferInfo = { };
                        bufferInfo.length = rb.resource.typeInfo.buffer.size;
                        bufferInfo.offset = bufferOffset;
                        buffer.buffers.push_back(bufferInfo);

                        bufferOffset += rb.resource.stride;
                    }
                    bufferLength = bufferOffset + rb.resource.typeInfo.buffer.size;
                    bufferLength = alignAddress(bufferLength, 16);
                    bufferOffset = alignAddress(bufferOffset, 16);

                    if (bufferLength > 0)
                    {
                        auto gpuBuffer = device->makeBuffer(bufferLength,
                                                         GPUBuffer::StorageModeShared,
                                                         CPUCacheMode::CPUCacheModeWriteCombined);
                        if (gpuBuffer == nullptr)
                        {
                            Log::error(std::format("failed to make buffer with length:{}",
                                                   bufferLength));
                            return false;
                        }
                        numBuffersGenerated++;
                        totalBytesAllocated += bufferLength;

                        for (auto& bufferInfo : buffer.buffers)
                        {
                            bufferInfo.buffer = gpuBuffer;
                        }
                        ShaderBindingLocation location
                        {
                            rb.resource.set, rb.resource.binding, 0 
                        };
                        resourceMap.insert_or_assign(location, buffer);
                    }
                }
            }
        }
        this->bufferResources.merge(std::move(resourceMap));
    }
    else
    {
    }
    Log::debug(std::format("initResources generated {:d} buffers, {:d} bytes.",
                           numBuffersGenerated, totalBytesAllocated));
    return true;
}

bool Submesh::buildPipelineState(GraphicsDevice* device)
{
    if (material == nullptr)
    {
        Log::error("Mesh(submesh) has no material.");
        return false;
    }

    auto vertexFunction = material->shader.function(ShaderStage::Vertex);
    auto fragmentFunction = material->shader.function(ShaderStage::Fragment);

    if (vertexFunction == nullptr)
    {
        Log::error("Material has no vertex function.");
        return false;
    }

    VertexDescriptor vertexDescriptor = this->vertexDescriptor();

    if (vertexDescriptor.attributes.empty() || vertexDescriptor.layouts.empty())
    {
        Log::error("Invalid vertex descriptor!");
        return false;
    }

    RenderPipelineDescriptor pipelineDescriptor = {};
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    pipelineDescriptor.colorAttachments = {
        { 0, PixelFormat::RGBA8Unorm, material->blendState }
    };
    pipelineDescriptor.depthStencilAttachmentPixelFormat = PixelFormat::Depth32Float;
    pipelineDescriptor.primitiveTopology = this->primitiveType;

    PipelineReflection reflection = {};
    auto pso = device->makeRenderPipeline(pipelineDescriptor, &reflection);
    if (pso == nullptr)
    {
        Log::error("GraphicsDevice::makeRenderPipeline() failed.");
        return false;
    }

    bool strict = true;

    // setup binding table
    std::vector<ResourceBindingSet> resourceBindings;
    for (const ShaderResource& res : reflection.resources)
    {
        if (auto optDescriptor = material->shader.descriptor
        ({ res.set, res.binding, 0 }, res.stages); optDescriptor)
        {
            ShaderDescriptor& descriptor = optDescriptor.value();
            std::optional<ShaderResource::Type> type = {};
            switch (descriptor.type)
            {
            case ShaderDescriptorType::UniformBuffer:  
            case ShaderDescriptorType::StorageBuffer:
            case ShaderDescriptorType::UniformTexelBuffer:
            case ShaderDescriptorType::StorageTexelBuffer:
                type = ShaderResource::TypeBuffer;
                break;
            case ShaderDescriptorType::StorageTexture:
            case ShaderDescriptorType::Texture:
                type = ShaderResource::TypeTexture;
                break;
            case ShaderDescriptorType::TextureSampler:
                type = ShaderResource::TypeTextureSampler;
                break;
            case ShaderDescriptorType::Sampler:
                type = ShaderResource::TypeSampler;
                break;
            }
            if (type.has_value() && type.value() == res.type)
            {
                ResourceBindingSet* rbset = nullptr;
                for (ResourceBindingSet& s : resourceBindings)
                {
                    if (s.index == res.set)
                    {
                        rbset = &s;
                        break;
                    }
                }
                if (rbset == nullptr)
                {
                    ResourceBindingSet s = { res.set };
                    resourceBindings.push_back(s);
                    rbset = &resourceBindings.back();
                }

                ResourceBinding resource = {};
                resource.resource = res;
                resource.binding = { res.binding };
                resource.binding.type = descriptor.type;
                resource.binding.arrayLength = descriptor.count;
                rbset->resources.push_back(resource);
            }
            else
            {
                Log::error(std::format(
                    "Unable to find shader resource information (set:{}, binding:{}, name:\"{}\")",
                    res.set, res.binding, res.name));
                if (strict) return false;
            }
        }
        else
        {
            Log::error(std::format(
                "Cannot find shader resource descriptor (name:{})", res.name));
            if (strict) return false;
        }
    }
    std::sort(resourceBindings.begin(), resourceBindings.end(),
              [](auto& a, auto& b) { return a.index < b.index; });
    ShaderBindingSetLayout layout;
    for (ResourceBindingSet& rbs : resourceBindings)
    {
        std::sort(rbs.resources.begin(), rbs.resources.end(),
                  [](auto& a, auto& b)
                  {
                      return a.binding.binding < b.binding.binding;
                  });
        // create ResourceBindingSet
        layout.bindings.clear();
        layout.bindings.reserve(rbs.resources.size());
        for (auto& b : rbs.resources)
            layout.bindings.push_back(b.binding);

        rbs.bindingSet = device->makeShaderBindingSet(layout);
        if (rbs.bindingSet == nullptr)
        {
            Log::error("GraphicsDevice::makeShaderBindingSet failed.");
            return false;
        }
    }
    std::vector<PushConstantData> pushConstants;
    pushConstants.reserve(reflection.pushConstantLayouts.size());
    for (auto& layout : reflection.pushConstantLayouts)
    {
        PushConstantData pcd = { layout };
        pushConstants.push_back(pcd);
    }

    this->pipelineState = pso;
    this->pipelineReflection = reflection;
    this->resourceBindings = std::move(resourceBindings);
    this->pushConstants = std::move(pushConstants);
    return true;
}

void Submesh::updateShadingProperties(const SceneState* sceneState)
{
    if (material == nullptr)
        return;

    struct StructMemberBind
    {
        const SceneState* sceneState;
        const Submesh* mesh;
        const ShaderResourceStructMember& member;
        const std::string parentPath;
        uint32_t structArrayIndex;
        uint32_t set;
        uint32_t binding;
        uint32_t offset; // initial offset (struct offset)

        size_t bind(uint8_t* buffer, size_t length)
        {
            uint32_t memberOffset = member.offset + offset;
            if (memberOffset >= length)
                return 0;   // insufficent buffer!
            if (memberOffset + member.size > length)
                return 0;   // insufficent buffer!

            std::string path;
            if (strlen(parentPath.c_str()) > 0 && strlen(member.name.c_str()) > 0)
                path = std::format("{}.{}", parentPath, member.name);
            else
                path = member.name;

            // write to dest
            uint8_t* dest = &(buffer[memberOffset]);
            size_t destLen = length - memberOffset;

            size_t copied = 0;

            if (member.dataType == ShaderDataType::Struct)
            {
                for (const ShaderResourceStructMember& m : member.members)
                {
                    auto s = StructMemberBind(sceneState, mesh, m, path,
                                              structArrayIndex,
                                              set, binding,
                                              offset + m.offset)
                        .bind(buffer, length);
                    if (s == 0)
                    {
                        Log::warning(std::format(
                            "Unable to bind shader uniform struct element: {} name:\"{}\"",
                            ShaderBindingLocation{ set, binding, memberOffset }, path));
                    }
                    copied = member.offset + s;
                }
            }
            else
            {
                // find semantic
                auto material = mesh->material.get();
                auto location = ShaderBindingLocation{ set, binding, memberOffset };
                MaterialShaderMap::Semantic semantic = MaterialSemantic::UserDefined;
                if (auto it = material->shader.resourceSemantics.find(location);
                    it != material->shader.resourceSemantics.end())
                    semantic = it->second;

                if (ShaderUniformSemantic* ss = std::get_if<ShaderUniformSemantic>(&semantic); ss)
                {
                    if (sceneState)
                        copied = mesh->bindShaderUniformBuffer(*ss,
                                                               member.dataType,
                                                               path,
                                                               *sceneState,
                                                               dest, destLen);
                }
                if (copied == 0)
                {
                    MaterialSemantic ms = MaterialSemantic::UserDefined;
                    if (auto p = std::get_if<MaterialSemantic>(&semantic); p)
                        ms = *p;
                    auto offset = member.count * member.stride * structArrayIndex;
                    copied = mesh->bindMaterialProperty(ms, location, 
                                                        member.dataType,
                                                        path, offset,
                                                        dest, destLen);
                }
                if (copied == 0)
                {
                    Log::warning(std::format(
                        "Unable to bind shader uniform struct ({}), arrayIndex:{}, name:\"{}\"",
                        location, structArrayIndex, path));
                }
            }
            return copied;
        }
    };
    auto copyStructProperty = [&](ShaderDataType type, uint32_t set, uint32_t binding,
                                  uint32_t offset, uint32_t size,
                                  uint32_t stride, uint32_t arrayIndex,
                                  const std::vector<ShaderResourceStructMember>& members,
                                  const std::string& name,
                                  uint8_t* buffer, size_t bufferLength) -> size_t
    {
        auto location = ShaderBindingLocation{ set, binding, offset };
        size_t copied = 0;
        if (type == ShaderDataType::Struct)
        {
            for (const ShaderResourceStructMember& member : members)
            {
                if (member.offset < offset)
                    continue;
                if (member.offset >= (offset + size))
                    break;

                auto s = StructMemberBind(sceneState, this, member, name,
                                          arrayIndex,
                                          set, binding, 0)
                    .bind(buffer, bufferLength);

                if (s > 0)
                {
                    copied += s;
                }
                else
                {
                    std::string path;
                    if (strlen(name.c_str()) > 0 && strlen(member.name.c_str()) > 0)
                        path = std::format("{}.{}", name, member.name);
                    else
                        path = member.name;

                    Log::warning(std::format(
                        "Unable to bind shader uniform struct {}, size:{}, name:\"{}\"",
                        ShaderBindingLocation{ set, binding, member.offset }, size, path));
                }
            }
        }
        else
        {
            MaterialShaderMap::Semantic semantic = MaterialSemantic::UserDefined;
            if (auto it = material->shader.resourceSemantics.find(location);
                it != material->shader.resourceSemantics.end())
                semantic = it->second;

            if (ShaderUniformSemantic* ss = std::get_if<ShaderUniformSemantic>(&semantic); ss)
            {
                if (sceneState)
                    copied = bindShaderUniformBuffer(*ss, type, name, *sceneState, buffer, bufferLength);
            }
            if (copied == 0)
            {
                MaterialSemantic ms = MaterialSemantic::UserDefined;
                if (auto p = std::get_if<MaterialSemantic>(&semantic); p)
                    ms = *p;
                auto offset = arrayIndex * stride;
                copied = bindMaterialProperty(ms, location, type,
                                              name, offset, buffer, bufferLength);
            }
            if (copied == 0)
            {
                Log::warning(std::format(
                    "Unable to bind shader uniform struct ({}), arrayIndex:{}, name:\"{}\"",
                    location, arrayIndex, name));
            }
        }

        return copied;
    };

    for (ResourceBindingSet& rbs : resourceBindings)
    {
        for (const ResourceBinding& rb : rbs.resources)
        {
            const ShaderResource& res = rb.resource;
            if (res.type == ShaderResource::TypeBuffer)
            {
                const ShaderResourceBuffer& typeInfo = res.typeInfo.buffer;
                auto loc = ShaderBindingLocation{ res.set, res.binding, 0 };
                if (auto it = bufferResources.find(loc);
                    it != bufferResources.end())
                {
                    auto& buffers = it->second.buffers;
                    std::vector<ShaderBindingSet::BufferInfo> updatedBuffers;

                    uint32_t validBufferCount = std::min(uint32_t(buffers.size()), res.count);
                    updatedBuffers.reserve(validBufferCount);

                    for (uint32_t index = 0; index < validBufferCount; ++index)
                    {
                        const ShaderBindingSet::BufferInfo& bufferInfo = buffers.at(index);

                        if (bufferInfo.buffer &&
                            bufferInfo.offset + bufferInfo.length <= bufferInfo.buffer->length())
                        {
                            if (uint8_t* buffer = (uint8_t*)bufferInfo.buffer->contents(); buffer)
                            {
                                buffer = &buffer[bufferInfo.offset];
                                auto bufferLength = bufferInfo.length;

                                size_t copied = copyStructProperty(
                                    typeInfo.dataType, res.set, res.binding,
                                    0, typeInfo.size,
                                    res.stride, index,
                                    res.members, res.name,
                                    buffer, bufferLength);
                                if (copied > 0)
                                    bufferInfo.buffer->flush();

                                updatedBuffers.push_back(bufferInfo);
                            }
                            else
                            {
                                Log::error(std::format(
                                    "Failed to map buffer for resource set:{}, binding:{} name:\"{}\"",
                                    res.set, res.binding, res.name));
                            }
                        }
                        else
                        {
                            Log::error(std::format(
                                "Buffer is too small for resource set:{}, binding:{} name:\"{}\"",
                                res.set, res.binding, res.name));
                            updatedBuffers.clear();
                            break;
                        }
                    }
                    if (updatedBuffers.empty() == false)
                    {
                        rbs.bindingSet->setBufferArray(
                            res.binding,
                            uint32_t(updatedBuffers.size()),
                            updatedBuffers.data());
                    }
                    else
                    {
                        Log::error(std::format(
                            "failed to bind buffer resource set:{:d}, binding:{:d} name:\"{}\"",
                            res.set, res.binding, res.name));
                    }
                }
            }
            else
            {
                auto location = ShaderBindingLocation{ res.set, res.binding, 0 };
                MaterialShaderMap::Semantic semantic = MaterialSemantic::UserDefined;
                if (auto it = material->shader.resourceSemantics.find(location);
                    it != material->shader.resourceSemantics.end())
                    semantic = it->second;

                size_t bounds = 0;
                if (ShaderUniformSemantic* ss = std::get_if<ShaderUniformSemantic>(&semantic); ss)
                {
                    if (sceneState)
                    {
                        switch (res.type)
                        {
                        case ShaderResource::TypeTexture:
                            bounds = bindShaderUniformTextures(*ss, res.name, *sceneState, rbs.bindingSet.get());
                            break;
                        case ShaderResource::TypeSampler:
                            bounds = bindShaderUniformSamplers(*ss, res.name, *sceneState, rbs.bindingSet.get());
                            break;
                        case ShaderResource::TypeTextureSampler:
                            bounds = std::min(bindShaderUniformTextures(*ss, res.name, *sceneState, rbs.bindingSet.get()),
                                              bindShaderUniformSamplers(*ss, res.name, *sceneState, rbs.bindingSet.get()));
                            break;
                        }
                    }
                }
                if (bounds == 0)
                {
                    MaterialSemantic ms = MaterialSemantic::UserDefined;
                    if (auto p = std::get_if<MaterialSemantic>(&semantic); p)
                        ms = *p;
                    switch (res.type)
                    {
                    case ShaderResource::TypeTexture:
                        bounds = bindMaterialTextures(ms, res, rbs.bindingSet.get());
                        break;
                    case ShaderResource::TypeSampler:
                        bounds = bindMaterialSamplers(ms, res, rbs.bindingSet.get());
                        break;
                    case ShaderResource::TypeTextureSampler:
                        bounds = std::min(bindMaterialTextures(ms, res, rbs.bindingSet.get()),
                                          bindMaterialSamplers(ms, res, rbs.bindingSet.get()));
                        break;
                    }
                }

                if (bounds == 0)
                {
                    Log::error(std::format(
                        "Failed to bind resource: {} (name: {}, type: {})",
                        res.binding, res.name, uint32_t(res.type)));
                }
            }
        }
    }
    for (PushConstantData& pc : pushConstants)
    {
        uint32_t structSize = pc.layout.offset + pc.layout.size;
        for (const ShaderResourceStructMember& member : pc.layout.members)
        {
            structSize = std::max(structSize, member.offset + member.size);
        }
        pc.data.clear();
        pc.data.resize(structSize, 0);

        uint8_t* buffer = pc.data.data();
        size_t bufferLength = pc.data.size();

        auto location = ShaderBindingLocation::pushConstant(pc.layout.offset);
        copyStructProperty(ShaderDataType::Struct,
                           location.set, location.binding,
                           location.offset, pc.layout.size,
                           pc.layout.size, 0, // stride, arrayIndex
                           pc.layout.members, pc.layout.name,
                           buffer, bufferLength);
    }
}

uint32_t Submesh::bindMaterialTextures(MaterialSemantic semantic, const ShaderResource& resource, ShaderBindingSet* bindingSet) const
{
    MaterialProperty::TextureArray textures;

    if (semantic != MaterialSemantic::UserDefined)
    {
        if (auto it = material->properties.find(semantic); it != material->properties.end())
        {
            std::visit(
                [&](auto&& arg)
                {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, MaterialProperty::TextureArray>)
                        textures = arg;
                    else if constexpr (std::is_same_v<T, MaterialProperty::CombinedTextureSamplerArray>)
                        std::transform(arg.begin(), arg.end(), std::back_inserter(textures),
                                       [](auto& combinedImage) {
                                           return combinedImage.texture;
                                       });
                }, it->second.value);
        }
    }

    if (textures.empty())
    {
        auto location = ShaderBindingLocation{ resource.set, resource.binding, 0 };
        if (auto it = material->userDefinedProperties.find(location);
            it != material->userDefinedProperties.end())
        {
            std::visit(
                [&](auto&& arg)
                {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, MaterialProperty::TextureArray>)
                        textures = arg;
                    else if constexpr (std::is_same_v<T, MaterialProperty::CombinedTextureSamplerArray>)
                        std::transform(arg.begin(), arg.end(), std::back_inserter(textures),
                                       [](auto& combinedImage) {
                                           return combinedImage.texture;
                                       });
                }, it->second.value);
        }
    }
    if (textures.empty() == false)
    {
        uint32_t n = std::max(resource.count, uint32_t(textures.size()));
        bindingSet->setTextureArray(resource.binding, n, textures.data());
        return n;
    }
    return 0;
}

uint32_t Submesh::bindMaterialSamplers(MaterialSemantic semantic, const ShaderResource& resource, ShaderBindingSet* bindingSet) const
{
    MaterialProperty::SamplerArray samplers;

    if (semantic != MaterialSemantic::UserDefined)
    {
        if (auto it = material->properties.find(semantic); it != material->properties.end())
        {
            std::visit(
                [&](auto&& arg)
                {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, MaterialProperty::SamplerArray>)
                        samplers = arg;
                    else if constexpr (std::is_same_v<T, MaterialProperty::CombinedTextureSamplerArray>)
                        std::transform(arg.begin(), arg.end(), std::back_inserter(samplers),
                                       [](auto& combinedImage) {
                                           return combinedImage.sampler;
                                       });
                }, it->second.value);
        }
    }

    if (samplers.empty())
    {
        auto location = ShaderBindingLocation{ resource.set, resource.binding, 0 };
        if (auto it = material->userDefinedProperties.find(location);
            it != material->userDefinedProperties.end())
        {
            std::visit(
                [&](auto&& arg)
                {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, MaterialProperty::SamplerArray>)
                        samplers = arg;
                    else if constexpr (std::is_same_v<T, MaterialProperty::CombinedTextureSamplerArray>)
                        std::transform(arg.begin(), arg.end(), std::back_inserter(samplers),
                                       [](auto& combinedImage) {
                                           return combinedImage.sampler;
                                       });
                }, it->second.value);
        }
    }
    if (samplers.empty() == false)
    {
        uint32_t n = std::max(resource.count, uint32_t(samplers.size()));
        bindingSet->setSamplerStateArray(resource.binding, n, samplers.data());
        return n;
    }
    return 0;
}

uint32_t Submesh::bindMaterialProperty(MaterialSemantic semantic,
                                       ShaderBindingLocation location,
                                       ShaderDataType dataType,
                                       const std::string& name,
                                       uint32_t itemOffset,
                                       uint8_t* buffer, size_t length) const
{
    auto material = this->material.get();
    if (material == nullptr)
        return 0;

    struct PropertyData
    {
        const void* data;
        size_t elementSize;
        size_t count;
    } data = { nullptr, 0, 0 };

    auto getPropertyData = [](const MaterialProperty& prop)-> PropertyData
    {
        return std::visit(
            [](auto&& arg) -> PropertyData
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, MaterialProperty::Buffer>)
                    return { arg.data(), sizeof(T::value_type), arg.size() };
                else if constexpr (std::is_same_v<T, MaterialProperty::Int8Array>)
                    return { arg.data(), sizeof(T::value_type), arg.size() };
                else if constexpr (std::is_same_v<T, MaterialProperty::UInt8Array>)
                    return { arg.data(), sizeof(T::value_type), arg.size() };
                else if constexpr (std::is_same_v<T, MaterialProperty::Int16Array>)
                    return { arg.data(), sizeof(T::value_type), arg.size() };
                else if constexpr (std::is_same_v<T, MaterialProperty::UInt16Array>)
                    return { arg.data(), sizeof(T::value_type), arg.size() };
                else if constexpr (std::is_same_v<T, MaterialProperty::Int32Array>)
                    return { arg.data(), sizeof(T::value_type), arg.size() };
                else if constexpr (std::is_same_v<T, MaterialProperty::UInt32Array>)
                    return { arg.data(), sizeof(T::value_type), arg.size() };
                else if constexpr (std::is_same_v<T, MaterialProperty::Int64Array>)
                    return { arg.data(), sizeof(T::value_type), arg.size() };
                else if constexpr (std::is_same_v<T, MaterialProperty::UInt64Array>)
                    return { arg.data(), sizeof(T::value_type), arg.size() };
                else if constexpr (std::is_same_v<T, MaterialProperty::HalfArray>)
                    return { arg.data(), sizeof(T::value_type), arg.size() };
                else if constexpr (std::is_same_v<T, MaterialProperty::FloatArray>)
                    return { arg.data(), sizeof(T::value_type), arg.size() };
                else if constexpr (std::is_same_v<T, MaterialProperty::DoubleArray>)
                    return { arg.data(), sizeof(T::value_type), arg.size() };
                return { nullptr, 0, 0 };
            }, prop.value);
    };

    if (semantic != MaterialSemantic::UserDefined)
    {
        if (auto it = material->properties.find(semantic);
            it != material->properties.end())
        {
            data = getPropertyData(it->second);
        }
    }
    if (data.count == 0)
    {
        if (auto it = material->userDefinedProperties.find(location);
            it != material->userDefinedProperties.end())
        {
            data = getPropertyData(it->second);
        }
    }

    if (data.count > 0 && data.elementSize > 0)
    {
        uint32_t dataLength = uint32_t(data.count * data.elementSize);
        if (dataLength > itemOffset)
        {
            uint32_t s = std::min(dataLength - itemOffset, uint32_t(length));
            memcpy(buffer, (uint8_t*)data.data + itemOffset, s);
            return s;
        }
    }
    return 0;
}

uint32_t Submesh::bindShaderUniformTextures(ShaderUniformSemantic semantic,
                                            const std::string& name,
                                            const SceneState& sceneState,
                                            ShaderBindingSet* bindingSet) const
{
    Log::warning(std::format(
        "No textures for ShaderUniformSemantic:{} name:\"{}\"",
        (uint32_t)semantic, name));
    return 0;
}

uint32_t Submesh::bindShaderUniformSamplers(ShaderUniformSemantic semantic,
                                            const std::string& name,
                                            const SceneState& sceneState,
                                            ShaderBindingSet* bindingSet) const
{
    Log::warning(std::format(
        "No samplers for ShaderUniformSemantic:{} name:\"{}\"",
        (uint32_t)semantic, name));
    return 0;
}

uint32_t Submesh::bindShaderUniformBuffer(ShaderUniformSemantic semantic,
                                          ShaderDataType dataType,
                                          const std::string& name,
                                          const SceneState& sceneState,
                                          uint8_t* buffer, size_t length) const
{
    switch (semantic)
    {
    case ShaderUniformSemantic::ModelViewProjectionMatrix:
        if (dataType == ShaderDataType::Float4x4)
        {
            Matrix4 matrix = sceneState.view.matrix();
            memcpy(buffer, matrix.val, sizeof(matrix));
            return sizeof(matrix);
        }
        else if (dataType == ShaderDataType::Float3x3)
        {
            auto m = sceneState.view.matrix();
            Matrix3 matrix = {
                m._11, m._12, m._13,
                m._21, m._22, m._23,
                m._31, m._32, m._33
            };
            memcpy(buffer, matrix.val, sizeof(matrix));
            return sizeof(matrix);
        }
        break;
    default:
        Log::error("Not implemented yet!");
        break;
    }
    return 0;
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

