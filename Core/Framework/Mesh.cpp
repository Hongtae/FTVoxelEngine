#include <vector>
#include <optional>
#include <tuple>
#include <functional>
#include <algorithm>
#include <numeric>
#include <condition_variable>
#include "Mesh.h"
#include "Scene.h"
#include "Logger.h"
#include "GraphicsDevice.h"

using namespace FV;

VertexDescriptor Mesh::vertexDescriptor() const {
    if (material == nullptr)
        return {};

    auto vertexFunction = material->shader.function(ShaderStage::Vertex);
    if (vertexFunction == nullptr)
        return {};

    auto& vertexInputs = vertexFunction->stageInputAttributes();
    auto& attributeSemantics = material->shader.inputAttributeSemantics;

    auto findBufferIndexAttribute = [this](VertexAttributeSemantic s)->
        std::optional<std::tuple<uint32_t, VertexAttribute>> {
        for (uint32_t index = 0; index < vertexBuffers.size(); ++index) {
            auto& vb = vertexBuffers.at(index);
            if (vb.buffer == nullptr)
                continue;
            for (auto& attr : vb.attributes) {
                if (attr.semantic == s) {
                    return std::make_tuple(index, attr);
                }
            }
        }
        return {};
    };
    auto findBufferIndexAttributeByName = [this](const std::string& name)->
        std::optional<std::tuple<uint32_t, VertexAttribute>> {
        for (uint32_t index = 0; index < vertexBuffers.size(); ++index) {
            auto& vb = vertexBuffers.at(index);
            if (vb.buffer == nullptr)
                continue;
            for (auto& attr : vb.attributes) {
                if (attr.name == name) {
                    return std::make_tuple(index, attr);
                }
            }
        }
        return {};
    };

    std::vector<VertexAttributeDescriptor> attributes;
    attributes.reserve(vertexInputs.size());
    for (auto& input : vertexInputs) {
        if (input.enabled == false)
            continue;

        auto it = attributeSemantics.find(input.location);
        auto semantic = (it != attributeSemantics.end()) ? it->second : VertexAttributeSemantic::UserDefined;

        std::optional<std::tuple<uint32_t, VertexAttribute>> bufferIndexAttr = {};
        if (semantic == VertexAttributeSemantic::UserDefined && input.name.empty() == false)
            bufferIndexAttr = findBufferIndexAttributeByName(input.name);

        if (!bufferIndexAttr)
            bufferIndexAttr = findBufferIndexAttribute(semantic);

        if (bufferIndexAttr) {
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
        } else {
            Log::error("Cannot bind vertex buffer at location: {} (name:{})",
                       input.location, input.name);
        }
    }

    std::vector<VertexBufferLayoutDescriptor> layouts;
    layouts.reserve(vertexBuffers.size());
    for (uint32_t bufferIndex = 0; bufferIndex < vertexBuffers.size(); ++bufferIndex) {
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

bool Mesh::initResources(GraphicsDevice* device, BufferUsagePolicy policy) {
    if (material == nullptr)
        return false;
    if (pipelineState == nullptr)
        buildPipelineState(device);
    if (pipelineState == nullptr)
        return false;

    auto alignAddressNPOT = [](uintptr_t ptr, uintptr_t alignment)->uintptr_t {
        if (ptr % alignment)
            ptr += alignment - (ptr % alignment);
        return ptr;
    };
    auto alignAddress = [](uintptr_t ptr, uintptr_t alignment)->uintptr_t {
        return (ptr + alignment - 1) & ~(alignment - 1);
    };

    size_t numBuffersGenerated = 0;
    size_t totalBytesAllocated = 0;

    if (policy == BufferUsagePolicy::SingleBuffer) {
        decltype(bufferResources) resourceMap;

        size_t bufferOffset = 0;
        size_t bufferLength = 0;
        for (const ResourceBindingSet& bset : resourceBindings) {
            for (const ResourceBinding& rb : bset.resources) {
                if (rb.resource.type == ShaderResource::TypeBuffer) {
                    auto buffer = BufferResource{ rb.resource.name };

                    for (uint32_t i = 0; i < rb.resource.count; ++i) {
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
        if (bufferLength > 0) {
            auto buffer = device->makeBuffer(bufferLength,
                                             GPUBuffer::StorageModeShared,
                                             CPUCacheModeWriteCombined);
            if (buffer == nullptr) {
                Log::error("failed to make buffer with length:{}", bufferLength);
                return false;
            }
            numBuffersGenerated++;
            totalBytesAllocated += bufferLength;

            for (auto& it : resourceMap) {
                BufferResource rb = it.second;
                for (auto& b : rb.buffers)
                    b.buffer = buffer;
                this->bufferResources.insert_or_assign(it.first, rb);
            }
        }
    } else if (policy == BufferUsagePolicy::SingleBufferPerSet) {
        decltype(bufferResources) resourceMap;

        for (const ResourceBindingSet& bset : resourceBindings) {
            size_t bufferOffset = 0;
            size_t bufferLength = 0;

            for (const ResourceBinding& rb : bset.resources) {
                if (rb.resource.type == ShaderResource::TypeBuffer) {
                    auto buffer = BufferResource{ rb.resource.name };

                    for (uint32_t i = 0; i < rb.resource.count; ++i) {
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
            if (bufferLength > 0) {
                auto buffer = device->makeBuffer(bufferLength,
                                                 GPUBuffer::StorageModeShared,
                                                 CPUCacheMode::CPUCacheModeWriteCombined);
                if (buffer == nullptr) {
                    Log::error("failed to make buffer with length:{}", bufferLength);
                    return false;
                }
                numBuffersGenerated++;
                totalBytesAllocated += bufferLength;

                for (auto& it : resourceMap) {
                    auto& rb = it.second;
                    for (auto& b : rb.buffers)
                        if (b.buffer == nullptr)
                            b.buffer = buffer;
                }
            }
        }
        for (auto& it : resourceMap) {
            this->bufferResources.insert_or_assign(it.first, it.second);
        }
    } else if (policy == BufferUsagePolicy::SingleBufferPerResource) {
        decltype(bufferResources) resourceMap;

        for (const ResourceBindingSet& bset : resourceBindings) {
            for (const ResourceBinding& rb : bset.resources) {
                if (rb.resource.type == ShaderResource::TypeBuffer) {
                    size_t bufferOffset = 0;
                    size_t bufferLength = 0;

                    auto buffer = BufferResource{ rb.resource.name };

                    for (uint32_t i = 0; i < rb.resource.count; ++i) {
                        ShaderBindingSet::BufferInfo bufferInfo = { };
                        bufferInfo.length = rb.resource.typeInfo.buffer.size;
                        bufferInfo.offset = bufferOffset;
                        buffer.buffers.push_back(bufferInfo);

                        bufferOffset += rb.resource.stride;
                    }
                    bufferLength = bufferOffset + rb.resource.typeInfo.buffer.size;
                    bufferLength = alignAddress(bufferLength, 16);
                    bufferOffset = alignAddress(bufferOffset, 16);

                    if (bufferLength > 0) {
                        auto gpuBuffer = device->makeBuffer(bufferLength,
                                                            GPUBuffer::StorageModeShared,
                                                            CPUCacheMode::CPUCacheModeWriteCombined);
                        if (gpuBuffer == nullptr) {
                            Log::error("failed to make buffer with length:{}",
                                       bufferLength);
                            return false;
                        }
                        numBuffersGenerated++;
                        totalBytesAllocated += bufferLength;

                        for (auto& bufferInfo : buffer.buffers) {
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
    } else {
    }
    Log::debug("initResources generated {:d} buffers, {:d} bytes.",
               numBuffersGenerated, totalBytesAllocated);
    return true;
}

bool Mesh::buildPipelineState(GraphicsDevice* device, PipelineReflection* rep) {
    if (material == nullptr) {
        Log::error("Mesh(submesh) has no material.");
        return false;
    }

    auto vertexFunction = material->shader.function(ShaderStage::Vertex);
    auto fragmentFunction = material->shader.function(ShaderStage::Fragment);

    if (vertexFunction == nullptr) {
        Log::error("Material has no vertex function.");
        return false;
    }

    VertexDescriptor vertexDescriptor = this->vertexDescriptor();

    if (vertexDescriptor.attributes.empty() || vertexDescriptor.layouts.empty()) {
        Log::error("Invalid vertex descriptor!");
        return false;
    }

    RenderPipelineDescriptor pipelineDescriptor = {};
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    pipelineDescriptor.colorAttachments = [&] {
        std::vector<RenderPipelineColorAttachmentDescriptor> attachments;
        for (uint32_t index = 0; index < material->attachments.size(); ++index) {
            auto& att = material->attachments.at(index);
            attachments.push_back({ index, att.format, att.blendState });
        }
        return attachments;
    }();
    pipelineDescriptor.depthStencilAttachmentPixelFormat = material->depthFormat;
    pipelineDescriptor.primitiveTopology = this->primitiveType;
    pipelineDescriptor.triangleFillMode = material->triangleFillMode;
    pipelineDescriptor.rasterizationEnabled = true;

    PipelineReflection reflection = {};
    auto pso = device->makeRenderPipelineState(pipelineDescriptor, &reflection);
    if (pso == nullptr) {
        Log::error("GraphicsDevice::makeRenderPipelineState() failed.");
        return false;
    }

    bool strict = true;

    // setup binding table
    std::vector<ResourceBindingSet> resourceBindings;
    for (const ShaderResource& res : reflection.resources) {
        if (auto optDescriptor = material->shader.descriptor
        ({ res.set, res.binding, 0 }, res.stages)) {
            ShaderDescriptor& descriptor = optDescriptor.value();
            std::optional<ShaderResource::Type> type = {};
            switch (descriptor.type) {
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
            if (type && type.value() == res.type) {
                ResourceBindingSet* rbset = nullptr;
                for (ResourceBindingSet& s : resourceBindings) {
                    if (s.index == res.set) {
                        rbset = &s;
                        break;
                    }
                }
                if (rbset == nullptr) {
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
            } else {
                Log::error(
                    "Unable to find shader resource information (set:{}, binding:{}, name:\"{}\")",
                    res.set, res.binding, res.name);
                if (strict) return false;
            }
        } else {
            Log::error(
                "Cannot find shader resource descriptor (name:{})", res.name);
            if (strict) return false;
        }
    }
    std::sort(resourceBindings.begin(), resourceBindings.end(),
              [](auto& a, auto& b) { return a.index < b.index; });
    ShaderBindingSetLayout layout;
    for (ResourceBindingSet& rbs : resourceBindings) {
        std::sort(rbs.resources.begin(), rbs.resources.end(),
                  [](auto& a, auto& b) {
                      return a.binding.binding < b.binding.binding;
                  });
        // create ResourceBindingSet
        layout.bindings.clear();
        layout.bindings.reserve(rbs.resources.size());
        for (auto& b : rbs.resources)
            layout.bindings.push_back(b.binding);

        rbs.bindingSet = device->makeShaderBindingSet(layout);
        if (rbs.bindingSet == nullptr) {
            Log::error("GraphicsDevice::makeShaderBindingSet failed.");
            return false;
        }
    }
    std::vector<PushConstantData> pushConstants;
    pushConstants.reserve(reflection.pushConstantLayouts.size());
    for (auto& layout : reflection.pushConstantLayouts) {
        PushConstantData pcd = { layout };
        pushConstants.push_back(pcd);
    }

    if (rep)
        *rep = reflection;

    this->pipelineState = pso;
    this->pipelineReflection = reflection;
    this->resourceBindings = std::move(resourceBindings);
    this->pushConstants = std::move(pushConstants);
    return true;
}

void Mesh::updateShadingProperties(const SceneState* sceneState) {
    if (material == nullptr)
        return;

    struct StructMemberBind {
        const SceneState* sceneState;
        const Mesh* mesh;
        const ShaderResourceStructMember& member;
        const std::string parentPath;
        uint32_t structArrayIndex;
        uint32_t set;
        uint32_t binding;
        uint32_t offset; // initial offset (struct offset)

        size_t bind(uint8_t* buffer, size_t length) {
            uint32_t bindingOffset = member.offset + offset;

            FVASSERT_DEBUG(member.size <= length);

            std::string path;
            if (strlen(parentPath.c_str()) > 0 && strlen(member.name.c_str()) > 0)
                path = std::format("{}.{}", parentPath, member.name);
            else
                path = member.name;

            size_t copied = 0;
            if (member.dataType == ShaderDataType::Struct) {
                for (const ShaderResourceStructMember& m : member.members) {
                    if (m.offset >= length)
                        continue;
                    if (m.offset + m.size > length)
                        continue;

                    auto s = StructMemberBind(sceneState, mesh, m, path,
                                              structArrayIndex,
                                              set, binding,
                                              bindingOffset)
                        .bind(buffer + m.offset, length - m.offset);
                    if (s == 0) {
                        Log::warning(
                            "Unable to bind shader uniform struct element: {} name:\"{}\"",
                            ShaderBindingLocation{ set, binding, bindingOffset },
                            path);
                    }
                    copied = member.offset + s;
                }
            } else {
                // find semantic
                auto material = mesh->material.get();
                auto location = ShaderBindingLocation{ set, binding, bindingOffset };
                MaterialShaderMap::Semantic semantic = MaterialSemantic::UserDefined;
                if (auto it = material->shader.resourceSemantics.find(location);
                    it != material->shader.resourceSemantics.end())
                    semantic = it->second;

                if (ShaderUniformSemantic* ss = std::get_if<ShaderUniformSemantic>(&semantic)) {
                    if (sceneState)
                        copied = mesh->bindShaderUniformBuffer(*ss,
                                                               member.dataType,
                                                               path,
                                                               *sceneState,
                                                               buffer, length);
                }
                if (copied == 0) {
                    MaterialSemantic ms = MaterialSemantic::UserDefined;
                    if (auto p = std::get_if<MaterialSemantic>(&semantic))
                        ms = *p;
                    auto offset = member.count * member.stride * structArrayIndex;
                    copied = mesh->bindMaterialProperty(ms, location,
                                                        member.dataType,
                                                        path, offset,
                                                        buffer, length);
                }
                if (copied == 0) {
                    Log::warning(
                        "Unable to bind shader uniform struct ({}), arrayIndex:{}, name:\"{}\"",
                        location, structArrayIndex, path);
                }
            }
            return copied;
        }
    };

    auto copyStructProperty = [&](
        ShaderDataType type, uint32_t set, uint32_t binding,
        uint32_t offset, uint32_t size,
        uint32_t stride, uint32_t arrayIndex,
        const std::vector<ShaderResourceStructMember>& members,
        const std::string& name,
        uint8_t* buffer, size_t bufferLength) -> size_t {
            auto location = ShaderBindingLocation{ set, binding, offset };
            size_t copied = 0;
            if (type == ShaderDataType::Struct) {
                for (const ShaderResourceStructMember& member : members) {
                    if (member.offset < offset)
                        continue;
                    if (member.offset >= (offset + size))
                        break;
                    if (member.offset + member.size > offset + size)
                        break;

                    std::string path;
                    if (strlen(name.c_str()) > 0 && strlen(member.name.c_str()) > 0)
                        path = std::format("{}.{}", name, member.name);
                    else
                        path = member.name;

                    if (member.offset + member.size - offset > bufferLength) {
                        Log::error(
                            "Insufficient buffer for shader uniform struct {}, size:{}, name:\"{}\"",
                            ShaderBindingLocation{ set, binding, member.offset },
                            size, path);
                        break;
                    }

                    auto d = member.offset - offset;
                    auto s = StructMemberBind(sceneState, this, member, name,
                                              arrayIndex,
                                              set, binding, 0)
                        .bind(buffer + d, bufferLength - d);

                    if (s > 0) {
                        copied += s;
                    } else {
                        Log::warning(
                            "Unable to bind shader uniform struct {}, size:{}, name:\"{}\"",
                            ShaderBindingLocation{ set, binding, member.offset },
                            size, path);
                    }
                }
            } else {
                MaterialShaderMap::Semantic semantic = MaterialSemantic::UserDefined;
                if (auto it = material->shader.resourceSemantics.find(location);
                    it != material->shader.resourceSemantics.end())
                    semantic = it->second;

                if (ShaderUniformSemantic* ss = std::get_if<ShaderUniformSemantic>(&semantic)) {
                    if (sceneState)
                        copied = bindShaderUniformBuffer(*ss, type, name, *sceneState, buffer, bufferLength);
                }
                if (copied == 0) {
                    MaterialSemantic ms = MaterialSemantic::UserDefined;
                    if (auto p = std::get_if<MaterialSemantic>(&semantic))
                        ms = *p;
                    auto offset = arrayIndex * stride;
                    copied = bindMaterialProperty(ms, location, type,
                                                  name, offset, buffer, bufferLength);
                }
                if (copied == 0) {
                    Log::warning(
                        "Unable to bind shader uniform struct ({}), arrayIndex:{}, name:\"{}\"",
                        location, arrayIndex, name);
                }
            }

            return copied;
    };

    for (ResourceBindingSet& rbs : resourceBindings) {
        for (const ResourceBinding& rb : rbs.resources) {
            const ShaderResource& res = rb.resource;
            if (res.type == ShaderResource::TypeBuffer) {
                const ShaderResourceBuffer& typeInfo = res.typeInfo.buffer;
                auto loc = ShaderBindingLocation{ res.set, res.binding, 0 };
                if (auto it = bufferResources.find(loc);
                    it != bufferResources.end()) {
                    auto& buffers = it->second.buffers;
                    std::vector<ShaderBindingSet::BufferInfo> updatedBuffers;

                    uint32_t validBufferCount = std::min(uint32_t(buffers.size()), res.count);
                    updatedBuffers.reserve(validBufferCount);

                    for (uint32_t index = 0; index < validBufferCount; ++index) {
                        const ShaderBindingSet::BufferInfo& bufferInfo = buffers.at(index);

                        if (bufferInfo.buffer &&
                            bufferInfo.offset + bufferInfo.length <= bufferInfo.buffer->length()) {
                            if (uint8_t* buffer = (uint8_t*)bufferInfo.buffer->contents()) {
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
                            } else {
                                Log::error(
                                    "Failed to map buffer for resource set:{}, binding:{} name:\"{}\"",
                                    res.set, res.binding, res.name);
                            }
                        } else {
                            Log::error(
                                "Buffer is too small for resource set:{}, binding:{} name:\"{}\"",
                                res.set, res.binding, res.name);
                            updatedBuffers.clear();
                            break;
                        }
                    }
                    if (updatedBuffers.empty() == false) {
                        rbs.bindingSet->setBufferArray(
                            res.binding,
                            uint32_t(updatedBuffers.size()),
                            updatedBuffers.data());
                    } else {
                        Log::error(
                            "failed to bind buffer resource set:{:d}, binding:{:d} name:\"{}\"",
                            res.set, res.binding, res.name);
                    }
                }
            } else {
                auto location = ShaderBindingLocation{ res.set, res.binding, 0 };
                MaterialShaderMap::Semantic semantic = MaterialSemantic::UserDefined;
                if (auto it = material->shader.resourceSemantics.find(location);
                    it != material->shader.resourceSemantics.end())
                    semantic = it->second;

                size_t bounds = 0;
                if (ShaderUniformSemantic* ss = std::get_if<ShaderUniformSemantic>(&semantic)) {
                    if (sceneState) {
                        switch (res.type) {
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
                if (bounds == 0) {
                    MaterialSemantic ms = MaterialSemantic::UserDefined;
                    if (auto p = std::get_if<MaterialSemantic>(&semantic))
                        ms = *p;
                    switch (res.type) {
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

                if (bounds == 0) {
                    Log::error("Failed to bind resource: {} (name: {}, type: {})",
                        res.binding, res.name, uint32_t(res.type));
                }
            }
        }
    }
    for (PushConstantData& pc : pushConstants) {
        if (pc.layout.size == 0)
            continue;

        pc.data.clear();
        pc.data.resize(pc.layout.size, 0);

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

uint32_t Mesh::bindMaterialTextures(MaterialSemantic semantic, const ShaderResource& resource, ShaderBindingSet* bindingSet) const {
    auto material = this->material.get();
    FVASSERT_DEBUG(material);

    MaterialProperty::TextureArray textures;

    if (semantic != MaterialSemantic::UserDefined) {
        if (auto it = material->properties.find(semantic); it != material->properties.end()) {
            std::visit(
                [&](auto&& arg) {
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

    if (textures.empty()) {
        auto location = ShaderBindingLocation{ resource.set, resource.binding, 0 };
        if (auto it = material->userDefinedProperties.find(location);
            it != material->userDefinedProperties.end()) {
            std::visit(
                [&](auto&& arg) {
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

    if (textures.empty() && material->defaultTexture)
        textures.push_back(material->defaultTexture);

    if (textures.empty() == false) {
        uint32_t n = std::min(resource.count, uint32_t(textures.size()));
        bindingSet->setTextureArray(resource.binding, n, textures.data());
        return n;
    }
    return 0;
}

uint32_t Mesh::bindMaterialSamplers(MaterialSemantic semantic, const ShaderResource& resource, ShaderBindingSet* bindingSet) const {
    auto material = this->material.get();
    FVASSERT_DEBUG(material);

    MaterialProperty::SamplerArray samplers;

    if (semantic != MaterialSemantic::UserDefined) {
        if (auto it = material->properties.find(semantic); it != material->properties.end()) {
            std::visit(
                [&](auto&& arg) {
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

    if (samplers.empty()) {
        auto location = ShaderBindingLocation{ resource.set, resource.binding, 0 };
        if (auto it = material->userDefinedProperties.find(location);
            it != material->userDefinedProperties.end()) {
            std::visit(
                [&](auto&& arg) {
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

    if (samplers.empty() && material->defaultSampler)
        samplers.push_back(material->defaultSampler);

    if (samplers.empty() == false) {
        uint32_t n = std::min(resource.count, uint32_t(samplers.size()));
        bindingSet->setSamplerStateArray(resource.binding, n, samplers.data());
        return n;
    }
    return 0;
}

uint32_t Mesh::bindMaterialProperty(MaterialSemantic semantic,
                                    ShaderBindingLocation location,
                                    ShaderDataType dataType,
                                    const std::string& name,
                                    uint32_t itemOffset,
                                    uint8_t* buffer, size_t length) const {
    auto material = this->material.get();
    FVASSERT_DEBUG(material);

    MaterialProperty::UnderlyingData data = {};

    if (semantic != MaterialSemantic::UserDefined) {
        if (auto it = material->properties.find(semantic);
            it != material->properties.end()) {
            data = it->second.underlyingData();
        }
    }
    if (data.count == 0) {
        if (auto it = material->userDefinedProperties.find(location);
            it != material->userDefinedProperties.end()) {
            data = it->second.underlyingData();
        }
    }

    if (data.count > 0 && data.elementSize > 0) {
        uint32_t dataLength = uint32_t(data.count * data.elementSize);
        if (dataLength > itemOffset) {
            uint32_t s = std::min(dataLength - itemOffset, uint32_t(length));
            memcpy(buffer, (uint8_t*)data.data + itemOffset, s);
            return s;
        }
    }
    return 0;
}

uint32_t Mesh::bindShaderUniformTextures(ShaderUniformSemantic semantic,
                                         const std::string& name,
                                         const SceneState& sceneState,
                                         ShaderBindingSet* bindingSet) const {
    Log::warning("No textures for ShaderUniformSemantic:{} name:\"{}\"",
                 (uint32_t)semantic, name);
    return 0;
}

uint32_t Mesh::bindShaderUniformSamplers(ShaderUniformSemantic semantic,
                                         const std::string& name,
                                         const SceneState& sceneState,
                                         ShaderBindingSet* bindingSet) const {
    Log::warning("No samplers for ShaderUniformSemantic:{} name:\"{}\"",
                 (uint32_t)semantic, name);
    return 0;
}

uint32_t Mesh::bindShaderUniformBuffer(ShaderUniformSemantic semantic,
                                       ShaderDataType dataType,
                                       const std::string& name,
                                       const SceneState& sceneState,
                                       uint8_t* buffer, size_t length) const {
    auto bindMatrix4 = [&](const Matrix4& matrix)->uint32_t {
        if (dataType == ShaderDataType::Float4x4) {
            memcpy(buffer, matrix.val, sizeof(matrix));
            return sizeof(matrix);
        } else if (dataType == ShaderDataType::Float3x3) {
            Matrix3 mat = {
                matrix._11, matrix._12, matrix._13,
                matrix._21, matrix._22, matrix._23,
                matrix._31, matrix._32, matrix._33
            };
            memcpy(buffer, matrix.val, sizeof(matrix));
            return sizeof(matrix);
        }
        return 0;
    };

    switch (semantic) {
    case ShaderUniformSemantic::ModelMatrix:
        return bindMatrix4(sceneState.model);
        break;
    case ShaderUniformSemantic::ViewMatrix:
        return bindMatrix4(sceneState.view.matrix4());
        break;
    case ShaderUniformSemantic::ProjectionMatrix:
        return bindMatrix4(sceneState.projection.matrix);
        break;
    case ShaderUniformSemantic::ViewProjectionMatrix:
        return bindMatrix4(sceneState.view.matrix4()
                           .concatenating(sceneState.projection.matrix));
        break;
    case ShaderUniformSemantic::ModelViewProjectionMatrix:
        return bindMatrix4(sceneState.model
                           .concatenating(sceneState.view.matrix4())
                           .concatenating(sceneState.projection.matrix));
        break;
    case ShaderUniformSemantic::InverseModelMatrix:
        return bindMatrix4(sceneState.model.inverted());
        break;
    case ShaderUniformSemantic::InverseViewMatrix:
        return bindMatrix4(sceneState.view.matrix4().inverted());
        break;
    case ShaderUniformSemantic::InverseProjectionMatrix:
        return bindMatrix4(sceneState.projection.matrix.inverted());
        break;
    case ShaderUniformSemantic::InverseViewProjectionMatrix:
        return bindMatrix4(sceneState.view.matrix4()
                           .concatenating(sceneState.projection.matrix)
                           .inverted());
        break;
    case ShaderUniformSemantic::InverseModelViewProjectionMatrix:
        return bindMatrix4(sceneState.model
                           .concatenating(sceneState.view.matrix4())
                           .concatenating(sceneState.projection.matrix)
                           .inverted());
        break;
    default:
        Log::error("Not implemented yet!");
        break;
    }
    return 0;
}

bool Mesh::encodeRenderCommand(RenderCommandEncoder* encoder,
                               uint32_t numInstances,
                               uint32_t baseInstance) const {
    if (pipelineState && vertexBuffers.empty() == false && material) {
        encoder->setRenderPipelineState(pipelineState);
        encoder->setFrontFacing(material->frontFace);
        encoder->setCullMode(material->cullMode);

        for (auto& rb : resourceBindings) {
            encoder->setResource(rb.index, rb.bindingSet);
        }
        if (pushConstants.empty() == false) {
            // VUID-vkCmdPushConstants-offset-01796
            uint32_t begin = pushConstants.front().layout.offset;
            uint32_t end = begin;
            uint32_t stages = 0;

            for (auto& pc : pushConstants) {
                begin = std::min(begin, pc.layout.offset);
                end = std::max(end, pc.layout.offset + pc.layout.size);
                stages = stages | pc.layout.stages;
            }
            auto bufferSize = end - begin;
            if (bufferSize > 0 && stages != 0) {
                std::vector<uint8_t> buffer(bufferSize, 0);
                for (auto& pc : pushConstants) {
                    if (pc.data.size() < pc.layout.size) {
                        Log::error(
                            "PushConstant (name:\"{}\", offset:{}, size:{}) data is missing!",
                            pc.layout.name, pc.layout.offset, pc.layout.size);
                        continue;
                    }
                    memcpy(&(buffer.data()[pc.layout.offset]), pc.data.data(), pc.layout.size);
                }

                encoder->pushConstant(stages, begin, bufferSize, buffer.data());
            }
        }
        for (uint32_t index = 0; index < vertexBuffers.size(); ++index) {
            auto& buffer = vertexBuffers.at(index);
            encoder->setVertexBuffer(buffer.buffer, buffer.byteOffset, index);
        }
        auto vertexCount = std::reduce(vertexBuffers.begin(),
                                       vertexBuffers.end(),
                                       vertexBuffers.front().vertexCount,
                                       [](auto pr, auto& b) {
                                           return std::min(pr, b.vertexCount);
                                       });
        if (vertexCount > 0) {
            if (indexBuffer) {
                encoder->drawIndexed(indexCount, indexType, indexBuffer,
                                     indexBufferByteOffset,
                                     numInstances,
                                     indexBufferBaseVertexIndex,
                                     baseInstance);
            } else {
                if (vertexCount > vertexStart)
                    encoder->draw(vertexStart, vertexCount - vertexStart, numInstances, baseInstance);
            }
        }
        return true;
    }
    return false;
}

bool Mesh::enumerateVertexBufferContent(VertexAttributeSemantic semantic,
                                        GraphicsDeviceContext* context,
                                        std::function<bool(const void*, VertexFormat, uint32_t)> handler) const {
    // find semantic
    const VertexAttribute* attrib = nullptr;
    const VertexBuffer* vertexBuffer = nullptr;

    for (auto& vb : vertexBuffers) {
        for (auto& attr : vb.attributes) {
            if (attr.semantic == semantic) {
                // lock buffer!
                attrib = &attr;
                vertexBuffer = &vb;
                break;
            }
        }
        if (vertexBuffer)
            break;
    }
    if (attrib && vertexBuffer) {
        if (bool(handler) == false) // function is empty, nothing more to do. 
            return true; // because we found a attribute with semantics.

        auto buffer = context->makeCPUAccessible(vertexBuffer->buffer);
        if (buffer == nullptr)
            return false;

        const uint8_t* mapped = (const uint8_t*)buffer->contents();
        if (mapped) {
            mapped += vertexBuffer->byteOffset + attrib->offset;
            for (uint32_t i = 0; i < vertexBuffer->vertexCount; ++i) {
                // stop enumeration if the handler returns false.
                if (handler(mapped, attrib->format, i) == false)
                    return true;
                mapped += vertexBuffer->byteStride;
            }
            return true;
        }
    }
    return false;
}

bool Mesh::enumerateIndexBufferContent(GraphicsDeviceContext* context,
                                       std::function<bool(uint32_t index)> handler) const {
    if (indexBuffer) {
        if (bool(handler) == false || indexCount == 0)
            return true;

        auto buffer = context->makeCPUAccessible(indexBuffer);
        if (buffer == nullptr)
            return false;

        const uint8_t* mapped = (const uint8_t*)buffer->contents();
        if (mapped == nullptr) {
            return false;
        }
        if (mapped) {
            mapped += indexBufferByteOffset;
            uint32_t base = indexBufferBaseVertexIndex;

            switch (indexType) {
            case IndexType::UInt16:
                for (uint32_t i = 0; i < indexCount; ++i) {
                    uint32_t index = reinterpret_cast<const uint16_t*>(mapped)[i];
                    if (handler(index + base) == false)
                        return true;
                }
                break;
            case IndexType::UInt32:
                for (uint32_t i = 0; i < indexCount; ++i) {
                    uint32_t index = reinterpret_cast<const uint32_t*>(mapped)[i];
                    if (handler(index + base) == false)
                        return true;
                }
                break;
            }
            return true;
        }
    }
    return false;
}
