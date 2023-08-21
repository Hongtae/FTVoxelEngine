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
                type = ShaderResource::TypeSampler;
                break;
            case ShaderDescriptorType::Sampler:
                type = ShaderResource::TypeTextureSampler;
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
                Log::error(std::format("Invalid shader resource type (name:{})",
                                       res.name));
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

    auto bindTextureResources = [this](const ShaderResource& res,
                                       ShaderBindingSet& bindingSet)->uint32_t {
        return 0;
    };

    auto bindSamplerResources = [this](const ShaderResource& res,
                                       ShaderBindingSet& bindingSet)->uint32_t {
        return 0;
    };
    auto copyMaterialProperty = [this](MaterialSemantic semantic,
                                       Material::BindingLocation location,
                                       uint32_t itemOffset,
                                       uint8_t* buffer, size_t length)->size_t
    {
        struct PropertyData { const void* data; size_t elementSize, count; };
        PropertyData data = { nullptr, 0, 0 };

        auto getPropertyData = [](const MaterialProperty& prop)-> PropertyData
        {
            return std::visit(
                [](auto&& arg) -> PropertyData
                {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, MaterialProperty::Buffer>)
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    else if constexpr (std::is_same_v<T, MaterialProperty::Int8Vector>)
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    else if constexpr (std::is_same_v<T, MaterialProperty::UInt8Vector>)
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    else if constexpr (std::is_same_v<T, MaterialProperty::Int16Vector>)
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    else if constexpr (std::is_same_v<T, MaterialProperty::UInt16Vector>)
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    else if constexpr (std::is_same_v<T, MaterialProperty::Int32Vector>)
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    else if constexpr (std::is_same_v<T, MaterialProperty::UInt32Vector>)
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    else if constexpr (std::is_same_v<T, MaterialProperty::Int64Vector>)
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    else if constexpr (std::is_same_v<T, MaterialProperty::UInt64Vector>)
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    else if constexpr (std::is_same_v<T, MaterialProperty::HalfVector>)
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    else if constexpr (std::is_same_v<T, MaterialProperty::FloatVector>)
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    else if constexpr (std::is_same_v<T, MaterialProperty::DoubleVector>)
                        return { arg.data(), sizeof(T::value_type), arg.size() };
                    return { nullptr, 0, 0 };
                },
                prop.value);
        };

        if (semantic != MaterialSemantic::Undefined && 
            semantic != MaterialSemantic::UserDefined)
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
            size_t dataLength = data.count * data.elementSize;
            if (dataLength > itemOffset)
            {
                size_t s = std::min(dataLength - itemOffset, length);
                memcpy(buffer, (uint8_t*)data.data + itemOffset, s);
                return s;
            }
        }
        return 0;
    };
    auto copyShadingProperty = [sceneState](ShaderUniformSemantic semantic,
                                            const std::string& name,
                                            uint8_t* buffer, size_t length)->size_t
    {
        Log::error("COPY-SHADING-PROPERTY Not implemented.");
        return 0;
    };

    struct StructMemberBind
    {
        const Material* material;
        const ShaderResourceStructMember& member;
        const std::string parentPath;
        uint32_t structArrayIndex;
        uint32_t set;
        uint32_t binding;
        uint32_t offset; // initial offset (struct offset)
        std::function<size_t(ShaderUniformSemantic, const std::string&, uint8_t*, size_t)> copyShadingProperty;
        std::function<size_t(MaterialSemantic, Material::BindingLocation, uint32_t, uint8_t*, size_t)> copyMaterialProperty;

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

            // find semantic
            auto location = ShaderBindingLocation{ set, binding, memberOffset };
            MaterialShaderMap::Semantic semantic = MaterialSemantic::Undefined;
            if (auto it = material->shader.resourceSemantics.find(location);
                it != material->shader.resourceSemantics.end())
                semantic = it->second;

            // write to dest
            uint8_t* dest = &(buffer[memberOffset]);
            size_t destLen = length - memberOffset;

            size_t copied = 0;
            if (ShaderUniformSemantic* ss = std::get_if<ShaderUniformSemantic>(&semantic); ss)
            {
                copied = this->copyShadingProperty(*ss, path, dest, destLen);
            }
            if (copied == 0)
            {
                MaterialSemantic ms = MaterialSemantic::Undefined;
                if (auto p = std::get_if<MaterialSemantic>(&semantic); p)
                    ms = *p;
                auto offset = member.count * member.stride * structArrayIndex;
                copied = this->copyMaterialProperty(ms, location, offset, dest, destLen);
            }
            if (copied == 0)
            {
                //TODO: find resource by path
                //But, optimized SPIR-V does not contain names.
            }

            size_t memberBounds = 0;
            for (const ShaderResourceStructMember& m : member.members)
            {
                memberBounds += StructMemberBind(material, m, path, structArrayIndex,
                                                 set, binding,
                                                 offset + m.offset,
                                                 copyShadingProperty,
                                                 copyMaterialProperty)
                    .bind(buffer, length);
            }
            if (copied > 0)
                return copied;
            return memberBounds;
        }
    };
    auto copyStructProperty = [&](uint32_t set, uint32_t binding, uint32_t offset,
                                  uint32_t stride, uint32_t arrayIndex,
                                  const std::vector<ShaderResourceStructMember>& members,
                                  const std::string& name,
                                  uint8_t* buffer, size_t bufferLength) -> size_t
    {
        auto location = ShaderBindingLocation{ set, binding, offset };
        MaterialShaderMap::Semantic semantic = MaterialSemantic::Undefined;
        if (auto it = material->shader.resourceSemantics.find(location);
            it != material->shader.resourceSemantics.end())
            semantic = it->second;

        size_t copied = 0;
        if (ShaderUniformSemantic* ss = std::get_if<ShaderUniformSemantic>(&semantic); ss)
        {
            copied = copyShadingProperty(*ss, name, buffer, bufferLength);
        }
        if (copied == 0)
        {
            MaterialSemantic ms = MaterialSemantic::Undefined;
            if (auto p = std::get_if<MaterialSemantic>(&semantic); p)
                ms = *p;
            auto offset = arrayIndex * stride;
            copied = copyMaterialProperty(ms, location, offset, buffer, bufferLength);
        }
        if (copied == 0)
        {
            //TODO: find resource by path
            //But, optimized SPIR-V does not contain names.
        }

        size_t memberCopied = 0;
        for (const ShaderResourceStructMember& member : members)
        {
            memberCopied += StructMemberBind(material.get(), member, name, arrayIndex,
                                             set, binding, offset,
                                            copyShadingProperty, copyMaterialProperty)
                .bind(buffer, bufferLength);
        }
        if (copied > 0)
            return copied;

        if (memberCopied == 0)
        {
            Log::debug(std::format(
                "Cannot find resource set:{}, binding:{}, offset:{}, arrayIndex:{}, name:{}",
                set, binding, offset, arrayIndex, name));
        }
        return memberCopied;
    };

    for (ResourceBindingSet& rbs : resourceBindings)
    {
        for (const ResourceBinding& rb : rbs.resources)
        {
            const ShaderResource& res = rb.resource;
            bool bound = false;
            if (res.type == ShaderResource::TypeBuffer)
            {
                //const ShaderResourceBuffer& typeInfo = res.typeInfo.buffer;
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
                                    res.set, res.binding, 0, res.stride,
                                    index,
                                    res.members, res.name,
                                    buffer, bufferLength);
                                if (copied > 0)
                                    bufferInfo.buffer->flush();

                                updatedBuffers.push_back(bufferInfo);
                            }
                            else
                            {
                                Log::error(std::format(
                                    "Failed to map buffer for resource set:{}, binding:{} (name:{})",
                                    res.set, res.binding, res.name));
                            }
                        }
                        else
                        {
                            Log::error(std::format(
                                "Buffer is too small for resource set:{}, binding:{} (name:{})",
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
                            "failed to bind buffer resource set:{:d}, binding:{:d} (name:{})",
                            res.set, res.binding, res.name));
                    }
                }
            }
            else if (res.type == ShaderResource::TypeTexture)
            {
                auto numBounds = bindTextureResources(res, *(rbs.bindingSet));
                if (numBounds > 0)
                    bound = true;
            }
            else if (res.type == ShaderResource::TypeSampler)
            {
                auto numBounds = bindSamplerResources(res, *(rbs.bindingSet));
                if (numBounds > 0)
                    bound = true;
            }
            else if (res.type == ShaderResource::TypeTextureSampler)
            {
                auto numTextureBounds = bindTextureResources(res, *(rbs.bindingSet));
                auto numSamplerBounds = bindSamplerResources(res, *(rbs.bindingSet));
                if (numTextureBounds > 0 && numSamplerBounds > 0)
                    bound = true;
            }

            if (bound == false)
            {
                Log::error(std::format(
                    "Failed to bind resource: {} (name: {}, type: {})",
                    res.binding, res.name, uint32_t(res.type)));
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
        pc.data.resize(structSize);

        uint8_t* buffer = pc.data.data();
        size_t bufferLength = pc.data.size();

        auto location = ShaderBindingLocation::pushConstant(pc.layout.offset);
        copyStructProperty(location.set, location.binding, location.offset,
                           pc.layout.size, 0, pc.layout.members, pc.layout.name,
                           buffer, bufferLength);
    }
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

