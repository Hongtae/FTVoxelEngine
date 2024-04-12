#include <cmath>
#include <algorithm>
#include "../Utils/tinygltf/tiny_gltf.h"
#include "Model.h"
#include "ShaderReflection.h"

using namespace FV;

struct LoaderContext {
    tinygltf::Model model;
    CommandQueue* queue;

    std::shared_ptr<Texture> defaultTexture;
    std::shared_ptr<SamplerState> defaultSampler;

    std::vector<std::shared_ptr<GPUBuffer>> buffers;
    std::vector<std::shared_ptr<Texture>> images;
    std::vector<std::shared_ptr<Material>> materials;

    std::vector<SceneNode> meshes;
    std::vector<SamplerDescriptor> samplerDescriptors;
};

std::shared_ptr<GPUBuffer> makeBuffer(CommandBuffer* cbuffer,
                                      size_t length,
                                      const void* data,
                                      GPUBuffer::StorageMode storageMode = GPUBuffer::StorageModePrivate,
                                      CPUCacheMode cpuCacheMode = CPUCacheModeDefault) {
    FVASSERT(length > 0);

    FVASSERT(cbuffer);
    auto device = cbuffer->device();

    std::shared_ptr<GPUBuffer> buffer;
    if (storageMode == GPUBuffer::StorageModeShared) {
        buffer = device->makeBuffer(length, storageMode, cpuCacheMode);
        FVASSERT(buffer);
        if (auto p = buffer->contents()) {
            memcpy(p, data, length);
            buffer->flush();
        } else {
            Log::error("GPUBuffer map failed.");
            FVERROR_ABORT("GPUBuffer map failed.");
            return nullptr;
        }
    } else {
        auto stgBuffer = device->makeBuffer(length,
                                            GPUBuffer::StorageModeShared,
                                            CPUCacheModeWriteCombined);
        FVASSERT(stgBuffer);
        if (auto p = stgBuffer->contents()) {
            memcpy(p, data, length);
            stgBuffer->flush();
        } else {
            Log::error("GPUBuffer map failed.");
            FVERROR_ABORT("GPUBuffer map failed.");
            return nullptr;
        }

        buffer = device->makeBuffer(length, storageMode, cpuCacheMode);
        FVASSERT(buffer);
        auto encoder = cbuffer->makeCopyCommandEncoder();
        FVASSERT(encoder);
        encoder->copy(stgBuffer, 0, buffer, 0, length);
        encoder->endEncoding();
    }
    return buffer;
}

void loadBuffers(LoaderContext& context) {
    auto cbuffer = context.queue->makeCommandBuffer();
    FVASSERT(cbuffer);

    const auto& model = context.model;
    context.buffers.resize(model.buffers.size(), nullptr);

    for (int index = 0; index < model.buffers.size(); ++index) {
        auto& glTFBuffer = model.buffers.at(index);

        auto& data = glTFBuffer.data;

        auto buffer = makeBuffer(cbuffer.get(), data.size(), data.data());
        FVASSERT(buffer);
        context.buffers.at(index) = buffer;
    }
    FVASSERT(context.buffers.size() == model.buffers.size());
    cbuffer->commit();
}

void loadImages(LoaderContext& context) {
    const auto& model = context.model;
    context.images.resize(model.images.size(), nullptr);

    for (int index = 0; index < model.images.size(); ++index) {
        auto& glTFImage = model.images.at(index);

        auto width = glTFImage.width;
        auto height = glTFImage.height;
        auto component = glTFImage.component;
        auto bits = glTFImage.bits;

        ImagePixelFormat imageFormat = ImagePixelFormat::Invalid;
        switch (component) {
        case 1:
            if (bits == 8)
                imageFormat = ImagePixelFormat::R8;
            else if (bits == 16)
                imageFormat = ImagePixelFormat::R16;
            else if (bits == 32)
                imageFormat = ImagePixelFormat::R32;
            break;
        case 2:
            if (bits == 8)
                imageFormat = ImagePixelFormat::RG8;
            else if (bits == 16)
                imageFormat = ImagePixelFormat::RG16;
            else if (bits == 32)
                imageFormat = ImagePixelFormat::RG32;
            break;
        case 3:
            if (bits == 8)
                imageFormat = ImagePixelFormat::RGB8;
            else if (bits == 16)
                imageFormat = ImagePixelFormat::RGB16;
            else if (bits == 32)
                imageFormat = ImagePixelFormat::RGB32;
            break;
        case 4:
            if (bits == 8)
                imageFormat = ImagePixelFormat::RGBA8;
            else if (bits == 16)
                imageFormat = ImagePixelFormat::RGBA16;
            else if (bits == 32)
                imageFormat = ImagePixelFormat::RGBA32;
            break;
        }
        if (imageFormat == ImagePixelFormat::Invalid) {
            Log::error("Unsupported image pixel format.");
            continue;
        }
        size_t reqLength = (bits >> 3) * width * height * component;
        if (glTFImage.image.size() < reqLength) {
            Log::error("invalid image pixel data.");
            continue;
        }
        auto image = std::make_shared<Image>(width, height, imageFormat, glTFImage.image.data());
        if (auto texture = image->makeTexture(context.queue)) {
            context.images.at(index) = texture;
        } else {
            Log::error("Failed to load image: {}", glTFImage.name);
        }
    }
    FVASSERT(context.images.size() == model.images.size());
}

void loadSamplerDescriptors(LoaderContext& context) {
    const auto& model = context.model;
    context.samplerDescriptors.resize(model.samplers.size());

    for (int index = 0; index < model.samplers.size(); ++index) {
        auto& glTFSampler = model.samplers.at(index);

        SamplerDescriptor desc = {};
        switch (glTFSampler.minFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            desc.minFilter = SamplerMinMagFilter::Nearest;
            desc.mipFilter = SamplerMipFilter::Nearest;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            desc.minFilter = SamplerMinMagFilter::Linear;
            desc.mipFilter = SamplerMipFilter::Nearest;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            desc.minFilter = SamplerMinMagFilter::Linear;
            desc.mipFilter = SamplerMipFilter::Nearest;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            desc.minFilter = SamplerMinMagFilter::Linear;
            desc.mipFilter = SamplerMipFilter::Linear;
            break;
        }
        if (glTFSampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
            desc.magFilter = SamplerMinMagFilter::Nearest;
        else
            desc.magFilter = SamplerMinMagFilter::Linear;

        auto samplerAddressMode = [](int wrap) {
            switch (wrap) {
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
                return SamplerAddressMode::Repeat;
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                return SamplerAddressMode::ClampToEdge;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                return SamplerAddressMode::MirrorRepeat;
            default:
                Log::error("Unknown address mode!");
                return SamplerAddressMode::Repeat;
            }
        };

        desc.addressModeU = samplerAddressMode(glTFSampler.wrapS);
        desc.addressModeV = samplerAddressMode(glTFSampler.wrapT);
        desc.addressModeV = samplerAddressMode(TINYGLTF_TEXTURE_WRAP_REPEAT);
        desc.lodMaxClamp = 256;

        context.samplerDescriptors.at(index) = desc;
    }
}

void loadMaterials(LoaderContext& context) {
    const auto& model = context.model;
    context.materials.resize(model.materials.size(), nullptr);
    for (int index = 0; index < model.materials.size(); ++index) {
        auto& glTFMaterial = model.materials.at(index);

        auto material = std::make_shared<Material>();
        material->name = glTFMaterial.name;
        material->defaultTexture = context.defaultTexture;
        material->defaultSampler = context.defaultSampler;

        if (_stricmp(glTFMaterial.alphaMode.c_str(), "BLEND") == 0) {
            material->attachments.front().blendState = BlendState::alphaBlend;
        } else {
            material->attachments.front().blendState = BlendState::opaque;
        }

        //if (glTFMaterial.doubleSided)
        //    material->cullMode = CullMode::None;
        //else
        //    material->cullMode = CullMode::Back;
        material->frontFace = Winding::CounterClockwise;
        material->cullMode = CullMode::None;

        material->setProperty(MaterialSemantic::BaseColor,
                              Color(
                                  float(glTFMaterial.pbrMetallicRoughness.baseColorFactor[0]),
                                  float(glTFMaterial.pbrMetallicRoughness.baseColorFactor[1]),
                                  float(glTFMaterial.pbrMetallicRoughness.baseColorFactor[2]),
                                  float(glTFMaterial.pbrMetallicRoughness.baseColorFactor[3])));

        auto textureSampler = [&](int index)->MaterialProperty::CombinedTextureSampler {
            if (index >= 0 && index < model.textures.size()) {
                const auto& texture = model.textures.at(index);
                std::shared_ptr<Texture> image;
                if (texture.source >= 0 && texture.source < context.images.size())
                    image = context.images.at(texture.source);
                std::shared_ptr<SamplerState> sampler;
                if (texture.sampler >= 0 && texture.sampler < context.samplerDescriptors.size()) {
                    auto samplerDesc = context.samplerDescriptors.at(texture.sampler);
                    sampler = context.queue->device()->makeSamplerState(samplerDesc);
                }
                if (sampler == nullptr)
                    sampler = context.defaultSampler;
                return { image, sampler };
            }
            return {};
        };

        if (auto ts = textureSampler(glTFMaterial.pbrMetallicRoughness.baseColorTexture.index); ts.texture) {
            material->setProperty(MaterialSemantic::BaseColorTexture, ts);
        }
        if (auto ts = textureSampler(glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index); ts.texture) {
            material->setProperty(MaterialSemantic::MetallicRoughnessTexture, ts);
        }
        material->setProperty(MaterialSemantic::Metallic, float(glTFMaterial.pbrMetallicRoughness.metallicFactor));
        material->setProperty(MaterialSemantic::Roughness, float(glTFMaterial.pbrMetallicRoughness.roughnessFactor));
        if (auto ts = textureSampler(glTFMaterial.normalTexture.index); ts.texture) {
            material->setProperty(MaterialSemantic::NormalTexture, ts);
        }
        material->setProperty(MaterialSemantic::NormalScaleFactor, float(glTFMaterial.normalTexture.scale));
        if (auto ts = textureSampler(glTFMaterial.occlusionTexture.index); ts.texture) {
            material->setProperty(MaterialSemantic::OcclusionTexture, ts);
        }
        material->setProperty(MaterialSemantic::OcclusionScale, float(glTFMaterial.occlusionTexture.strength));

        material->setProperty(MaterialSemantic::EmissiveFactor, Vector3(
            float(glTFMaterial.emissiveFactor[0]),
            float(glTFMaterial.emissiveFactor[1]),
            float(glTFMaterial.emissiveFactor[2])
        ));
        if (auto ts = textureSampler(glTFMaterial.emissiveTexture.index); ts.texture) {
            material->setProperty(MaterialSemantic::EmissiveTexture, ts);
        }
        context.materials.at(index) = material;
    }
}

void loadMeshes(LoaderContext& context) {
    auto device = context.queue->device();
    auto cbuffer = context.queue->makeCommandBuffer();
    FVASSERT(cbuffer);

    const auto& model = context.model;
    context.meshes.resize(model.meshes.size());
    for (int index = 0; index < model.meshes.size(); ++index) {
        auto& glTFMesh = model.meshes.at(index);

        SceneNode node = {};
        node.name = glTFMesh.name;

        std::vector<Vector3> positions;
        std::vector<uint32_t> indices;

        bool hasVertexNormal = false;
        bool hasVertexColor = false;

        for (auto& glTFPrimitive : glTFMesh.primitives) {
            Mesh mesh = {};
            std::unordered_map<int, Mesh::VertexBuffer> vertexBuffers;

            for (auto& [attributeName, accessorIndex] : glTFPrimitive.attributes) {
                auto& glTFAccessor = model.accessors[accessorIndex];
                auto& glTFBufferView = model.bufferViews[glTFAccessor.bufferView];
                auto& glTFBuffer = model.buffers[glTFBufferView.buffer];

                uint32_t vertexStride = glTFAccessor.ByteStride(glTFBufferView);
                uint32_t bufferOffset = glTFBufferView.byteOffset;
                uint32_t attribOffset = 0;
                if (glTFAccessor.byteOffset < vertexStride) {
                    attribOffset = glTFAccessor.byteOffset; // interleaved
                } else {
                    bufferOffset += glTFAccessor.byteOffset;
                }

                Mesh::VertexBuffer buffer =
                {
                    .byteOffset = bufferOffset,
                    .byteStride = vertexStride,
                    .vertexCount = uint32_t(glTFAccessor.count),
                    .buffer = context.buffers.at(glTFBufferView.buffer)
                };

                Mesh::VertexAttribute attribute = {};
                attribute.name = attributeName;
                attribute.offset = attribOffset;
                attribute.semantic = VertexAttributeSemantic::UserDefined;
                attribute.format = VertexFormat::Invalid;
                if (glTFAccessor.componentType == TINYGLTF_COMPONENT_TYPE_DOUBLE) {
                    Log::error("Vertex component type for Double(Float64) is not supported!");
                    continue;
                }

                switch (glTFAccessor.type) {
                case TINYGLTF_TYPE_SCALAR:
                    switch (glTFAccessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_BYTE:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::CharNormalized : VertexFormat::Char;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::UCharNormalized : VertexFormat::UChar;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_SHORT:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::ShortNormalized : VertexFormat::Short;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::UShortNormalized : VertexFormat::UShort;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_INT:
                        attribute.format = VertexFormat::Int;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        attribute.format = VertexFormat::UInt;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_FLOAT:
                        attribute.format = VertexFormat::Float;
                        break;
                    }
                    break;
                case TINYGLTF_TYPE_VEC2:
                    switch (glTFAccessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_BYTE:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::Char2Normalized : VertexFormat::Char2;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::UChar2Normalized : VertexFormat::UChar2;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_SHORT:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::Short2Normalized : VertexFormat::Short2;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::UShort2Normalized : VertexFormat::UShort2;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_INT:
                        attribute.format = VertexFormat::Int2;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        attribute.format = VertexFormat::UInt2;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_FLOAT:
                        attribute.format = VertexFormat::Float2;
                        break;
                    }
                    break;
                case TINYGLTF_TYPE_VEC3:
                    switch (glTFAccessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_BYTE:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::Char3Normalized : VertexFormat::Char3;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::UChar3Normalized : VertexFormat::UChar3;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_SHORT:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::Short3Normalized : VertexFormat::Short3;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::UShort3Normalized : VertexFormat::UShort3;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_INT:
                        attribute.format = VertexFormat::Int3;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        attribute.format = VertexFormat::UInt3;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_FLOAT:
                        attribute.format = VertexFormat::Float3;
                        break;
                    }
                    break;
                case TINYGLTF_TYPE_VEC4:
                    switch (glTFAccessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_BYTE:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::Char4Normalized : VertexFormat::Char4;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::UChar4Normalized : VertexFormat::UChar4;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_SHORT:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::Short4Normalized : VertexFormat::Short4;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        attribute.format = glTFAccessor.normalized ? VertexFormat::UShort4Normalized : VertexFormat::UShort4;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_INT:
                        attribute.format = VertexFormat::Int4;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        attribute.format = VertexFormat::UInt4;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_FLOAT:
                        attribute.format = VertexFormat::Float4;
                        break;
                    }
                    break;
                }
                if (attribute.format == VertexFormat::Invalid) {
                    Log::error("Unhandled vertex attribute type: {}", glTFAccessor.type);
                    continue;
                }

                // Note: 
                // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes

                if (_stricmp(attributeName.c_str(), "POSITION") == 0) {
                    attribute.semantic = VertexAttributeSemantic::Position;
                    if (attribute.format == VertexFormat::Float3) {
                        // get AABB
                        const uint8_t* ptr = glTFBuffer.data.data();
                        ptr += bufferOffset + attribOffset;
                        AABB aabb = {};
                        positions.clear();
                        positions.reserve(glTFAccessor.count);
                        for (size_t i = 0; i < glTFAccessor.count; ++i) {
                            const Vector3* p = reinterpret_cast<const Vector3*>(ptr);
                            positions.push_back(*p);
                            aabb.expand(*p);
                            ptr += vertexStride;
                        }
                        mesh.aabb = aabb;
                    }
                } else if (_stricmp(attributeName.c_str(), "NORMAL") == 0) {
                    attribute.semantic = VertexAttributeSemantic::Normal;
                    hasVertexNormal = true;
                } else if (_stricmp(attributeName.c_str(), "TANGENT") == 0)
                    attribute.semantic = VertexAttributeSemantic::Tangent;
                else if (_stricmp(attributeName.c_str(), "TEXCOORD_0") == 0)
                    attribute.semantic = VertexAttributeSemantic::TextureCoordinates;
                else if (_stricmp(attributeName.c_str(), "COLOR_0") == 0) {
                    attribute.semantic = VertexAttributeSemantic::Color;
                    hasVertexColor = true;
                }  else {
                    Log::warning("Unhandled vertex buffer attribute: {}", attributeName);
                    continue;
                }
                buffer.attributes.push_back(attribute);
                mesh.vertexBuffers.push_back(buffer);
            }

            switch (glTFPrimitive.mode) {
            case TINYGLTF_MODE_POINTS:
                mesh.primitiveType = PrimitiveType::Point;
                break;
            case TINYGLTF_MODE_LINE:
            case TINYGLTF_MODE_LINE_LOOP:
                mesh.primitiveType = PrimitiveType::Line;
                break;
            case TINYGLTF_MODE_LINE_STRIP:
                mesh.primitiveType = PrimitiveType::LineStrip;
                break;
            case TINYGLTF_MODE_TRIANGLES:
                mesh.primitiveType = PrimitiveType::Triangle;
                break;
            case TINYGLTF_MODE_TRIANGLE_STRIP:
                mesh.primitiveType = PrimitiveType::TriangleStrip;
                break;
            default:
                //throw std::runtime_error("Unknown primitive type");
                Log::error("Unsupported primitive type: {}", glTFPrimitive.mode);
                continue;
            }
            
            if (glTFPrimitive.indices >= 0) {
                auto& glTFAccessor = model.accessors[glTFPrimitive.indices];
                auto& glTFBufferView = model.bufferViews[glTFAccessor.bufferView];
                auto& glTFBuffer = model.buffers[glTFBufferView.buffer];

                mesh.indexBufferByteOffset = uint32_t(glTFBufferView.byteOffset + glTFAccessor.byteOffset);
                mesh.indexCount = glTFAccessor.count;
                mesh.indexBuffer = context.buffers[glTFBufferView.buffer];
                mesh.indexBufferBaseVertexIndex = 0;

                indices.reserve(glTFAccessor.count);
                uint8_t* p = ((uint8_t*)glTFBuffer.data.data()) + mesh.indexBufferByteOffset;

                switch (glTFAccessor.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: // convert to uint16
                    do {
                        FVASSERT(glTFAccessor.count > 0);
                        std::vector<uint16_t> indexData;
                        indexData.reserve(glTFAccessor.count);
                        for (int i = 0; i < glTFAccessor.count; ++i) {
                            indexData.push_back(p[i]);
                            indices.push_back(p[i]);
                        }

                        auto buffer = makeBuffer(cbuffer.get(), indexData.size() * 2,
                                                 indexData.data());
                        FVASSERT(buffer);
                        mesh.indexBuffer = buffer;
                        mesh.indexType = IndexType::UInt16;
                        mesh.indexBufferByteOffset = 0;

                    } while (0);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    mesh.indexType = IndexType::UInt16;
                    for (int i = 0; i < glTFAccessor.count; ++i) {
                        uint16_t index = ((uint16_t*)p)[i];
                        indices.push_back(index);
                    }
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    mesh.indexType = IndexType::UInt32;
                    for (int i = 0; i < glTFAccessor.count; ++i) {
                        uint32_t index = ((uint32_t*)p)[i];
                        indices.push_back(index);
                    }
                    break;
                }
            } else {
                indices.reserve(positions.size());
                for (int i = 0; i < positions.size(); ++i)
                    indices.push_back(i);
            }

            if (hasVertexNormal == false) {
                std::vector<Vector3> normals(positions.size(), Vector3(0, 0, 0));

                auto generateFaceNormal = [&](uint32_t i0, uint32_t i1, uint32_t i2) {
                    const Vector3& p0 = positions.at(i0);
                    const Vector3& p1 = positions.at(i1);
                    const Vector3& p2 = positions.at(i2);
                    auto u = p1 - p0;
                    auto v = p2 - p0;
                    auto normal = Vector3::cross(u, v).normalized();
                    normals.at(i0) += normal;
                    normals.at(i1) += normal;
                    normals.at(i2) += normal;
                };

                switch (mesh.primitiveType) {
                case PrimitiveType::Triangle:
                    for (int i = 0; (i+2) < indices.size(); i += 3) {
                        generateFaceNormal(indices.at(i),
                                           indices.at(i + 1),
                                           indices.at(i + 2));
                    }
                    break;
                case PrimitiveType::TriangleStrip:
                    for (int i = 0; (i+2) < indices.size(); ++i) {
                        if (i % 2) {
                            generateFaceNormal(indices.at(i + 1),
                                               indices.at(i),
                                               indices.at(i + 2));
                        } else {
                            generateFaceNormal(indices.at(i),
                                               indices.at(i + 1),
                                               indices.at(i + 2));
                        }
                    }
                    break;
                }
                for (auto& normal : normals)
                    normal.normalize();

                auto buffer = makeBuffer(cbuffer.get(), normals.size() * sizeof(Vector3), normals.data());
                Mesh::VertexAttribute attribute = {
                    VertexAttributeSemantic::Normal,
                    VertexFormat::Float3,
                    0,
                    "Normal"
                };
                Mesh::VertexBuffer vb = {
                    0, sizeof(Vector3), normals.size(),
                    buffer, { attribute }
                };
                mesh.vertexBuffers.push_back(vb);
            }
            if (hasVertexColor == false) {
                std::vector<Vector4> colors(positions.size(), Vector4(1, 1, 1, 1));
                auto buffer = makeBuffer(cbuffer.get(), colors.size() * sizeof(Vector4), colors.data());
                Mesh::VertexAttribute attribute = {
                    VertexAttributeSemantic::Color,
                    VertexFormat::Float4,
                    0,
                    "Color"
                };
                Mesh::VertexBuffer vb = {
                    0, sizeof(Vector4), colors.size(),
                    buffer, { attribute }
                };
                mesh.vertexBuffers.push_back(vb);
            }

            if (glTFPrimitive.material >= 0) {
                mesh.material = context.materials[glTFPrimitive.material];
            } else {
                mesh.material = std::make_shared<Material>("default");
                mesh.material->defaultTexture = context.defaultTexture;
                mesh.material->defaultSampler = context.defaultSampler;
                mesh.material->setProperty(MaterialSemantic::BaseColor, Color::white);
                mesh.material->setProperty(MaterialSemantic::Metallic, 1.0f);
                mesh.material->setProperty(MaterialSemantic::Roughness, 1.0f);
            }

            SceneNode meshNode = { .name = glTFMesh.name, .mesh = mesh };
            node.children.push_back(meshNode);
        }

        while (node.mesh.has_value() == false && node.children.size() == 1) {
            node = node.children.front();
        }

        context.meshes.at(index) = node;
    }
    cbuffer->commit();
}

SceneNode loadNode(const tinygltf::Node& node, const Matrix4& baseTM, LoaderContext& context) {
    const auto& model = context.model;
    SceneNode output = { .name = node.name };
    if (node.mesh >= 0) {
        auto mesh = context.meshes.at(node.mesh);
        while (mesh.mesh.has_value() == false && mesh.children.size() == 1) {
            mesh = mesh.children.front();
        }
        if (mesh.mesh.has_value() && mesh.children.empty())
            output.mesh = mesh.mesh;
        else
            output.children.push_back(mesh);
    }

    Matrix4 nodeTM = Matrix4::identity;

    if (node.matrix.size() == 16) {
        for (int i = 0; i < 16; ++i)
            nodeTM.val[i] = float(node.matrix[i]);
    } else {
        Quaternion rotation = Quaternion::identity;
        Vector3 scale = { 1, 1, 1 };
        Vector3 translation = Vector3::zero;
        if (node.rotation.size() == 4) {
            for (int i = 0; i < 4; ++i)
                rotation.val[i] = float(node.rotation[i]);
        }
        if (node.scale.size() == 3) {
            for (int i = 0; i < 3; ++i)
                scale.val[i] = float(node.scale[i]);
        }
        if (node.translation.size() == 3) {
            for (int i = 0; i < 3; ++i)
                translation.val[i] = float(node.translation[i]);
        }
        // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#transformations
        nodeTM = AffineTransform3::identity
            .scaled(scale)
            .rotated(rotation)
            .translated(translation)
            .matrix4();
    }

    Matrix4 worldTM = nodeTM.concatenating(baseTM);

    // remove scale component.
    auto decompose = [](const Matrix4& mat) -> std::tuple<Transform, Vector3> {
        auto affine = AffineTransform3(mat);
        Vector3 scale = { 1, 1, 1 };
        Quaternion quat = Quaternion::identity;
        if (affine.decompose(scale, quat))
            return { Transform(quat, affine.translation), scale };
        return { Transform::identity, {0, 0, 0} };
    };

    auto base = decompose(baseTM);
    Transform baseTrans = std::get<0>(base);

    auto ts = decompose(worldTM);
    output.transform = std::get<0>(ts).concatenating(baseTrans.inverted());
    output.scale = std::get<1>(ts);

    // update mesh node transform
    for (auto& node : output.children) {
        ForEachNode{ node }([&](auto& node) {
            node.scale = std::get<1>(ts);
        });
    }

    output.children.reserve(output.children.size() + node.children.size());
    for (auto index : node.children) {
        auto& child = model.nodes.at(index);
        auto node = loadNode(child, worldTM, context);
        output.children.push_back(node);
    }
    return output;
}

Model::Scene loadScene(const tinygltf::Scene& scene, LoaderContext& context) {
    const auto& model = context.model;
    Model::Scene output = {};
    output.name = scene.name;

    output.nodes.reserve(scene.nodes.size());
    for (auto index : scene.nodes) {
        auto& glTFNode = model.nodes.at(index);
        auto node = loadNode(glTFNode, Matrix4::identity, context);
        output.nodes.push_back(node);
    }
    return output;
}

std::shared_ptr<Model> loadModel(std::filesystem::path path, CommandQueue* queue) {
    LoaderContext context = { .queue = queue };
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    std::string lowercasedPath = path.string();
    std::transform(lowercasedPath.begin(), lowercasedPath.end(), lowercasedPath.begin(),
                   [](auto c) { return std::tolower(c); });
    bool result = false;
    if (lowercasedPath.ends_with(".gltf")) {
        result = loader.LoadASCIIFromFile(&context.model, &err, &warn, path.generic_string());
    } else {
        result = loader.LoadBinaryFromFile(&context.model, &err, &warn, path.generic_string());
    }
    if (warn.empty() == false)
        Log::warning("glTF warning: {}", warn);
    if (err.empty() == false)
        Log::error("glTF error: {}", err);

    if (result) {
        tinygltf::Model& model = context.model;

        auto defaultImage = Image(1, 1, ImagePixelFormat::RGBA8, Color(1, 0, 1, 1).rgba8().bytes);
        context.defaultTexture = defaultImage.makeTexture(queue, TextureUsageSampled | TextureUsageStorage);
        context.defaultSampler = queue->device()->makeSamplerState(
            {
                SamplerAddressMode::Repeat,
                SamplerAddressMode::Repeat,
                SamplerAddressMode::Repeat,
            });

        loadBuffers(context);
        loadImages(context);
        loadSamplerDescriptors(context);
        loadMaterials(context);
        loadMeshes(context);

        auto output = std::make_shared<Model>();

        for (auto& glTFScene : context.model.scenes) {
            auto scene = loadScene(glTFScene, context);
            output->scenes.push_back(scene);
        }
        output->defaultSceneIndex = context.model.defaultScene;

        return output;
    }
    return nullptr;
}

std::vector<Triangle> Model::triangleList(int sceneIndex, GraphicsDeviceContext* graphicsContext) const {
    auto getMeshTriangles = [&](const Mesh& mesh, const Matrix4& transform) -> std::vector<Triangle> {
        if (mesh.primitiveType != PrimitiveType::Triangle &&
            mesh.primitiveType != PrimitiveType::TriangleStrip)
            return {};

        std::vector<Triangle> triangles;

        std::vector<Vector3> positions;
        mesh.enumerateVertexBufferContent(
            VertexAttributeSemantic::Position,
            graphicsContext,
            [&](const void* data, VertexFormat format, uint32_t index)->bool {
                if (format == VertexFormat::Float3) {
                    Vector3 v = *(const Vector3*)data;
                    positions.push_back(v.applying(transform, 1.0));
                    return true;
                }
                return false;
            });
        std::vector<uint32_t> indices;
        if (mesh.indexBuffer) {
            indices.reserve(mesh.indexCount);
            mesh.enumerateIndexBufferContent(
                graphicsContext,
                [&](uint32_t index)->bool {
                    indices.push_back(index);
                    return true;
                });
        } else {
            indices.reserve(positions.size());
            for (int i = 0; i < positions.size(); ++i)
                indices.push_back(i);
        }

        if (mesh.primitiveType == PrimitiveType::TriangleStrip) {
            auto numTris = indices.size() > 2 ? indices.size() - 2 : 0;
            triangles.reserve(numTris);

            for (int i = 0; i < numTris; ++i) {
                uint32_t idx[3] = {
                indices.at(i),
                indices.at(i + 1),
                indices.at(i + 2)
                };
                if (i % 2)
                    std::swap(idx[0], idx[1]);

                Triangle t = {
                positions.at(idx[0]),
                positions.at(idx[1]),
                positions.at(idx[2])
                };
                triangles.push_back(t);
            }
        } else {
            auto numTris = indices.size() / 3;
            triangles.reserve(numTris);
            for (int i = 0; i < numTris; ++i) {
                uint32_t idx[3] = {
                indices.at(i * 3),
                indices.at(i * 3 + 1),
                indices.at(i * 3 + 2)
                };
                Triangle t = {
                positions.at(idx[0]),
                positions.at(idx[1]),
                positions.at(idx[2])
                };
                triangles.push_back(t);
            }
        }
        return triangles;
    };

    if (sceneIndex >= 0 && sceneIndex < this->scenes.size()) {
        std::vector<Triangle> triangles;
        auto& scene = this->scenes.at(sceneIndex);
        for (auto& node : scene.nodes)
            ForEachNodeConst{ node }(
                [&](auto& node, const Transform& trans) {
                    if (node.mesh.has_value()) {
                        const Mesh& mesh = node.mesh.value();
                        Matrix4 tm = AffineTransform3::identity
                            .scaled(node.scale).matrix4()
                            .concatenating(trans.matrix4());

                        auto tris = getMeshTriangles(mesh, tm);
                        triangles.insert(triangles.end(), tris.begin(), tris.end());
                    }
                });
        return triangles;
    }
    return {};
}

std::vector<MaterialFace> Model::faceList(int sceneIndex, GraphicsDeviceContext* graphicsContext) const {
    auto getMeshFaces = [&](const Mesh& mesh, const Matrix4& transform) -> std::vector<MaterialFace> {
        if (mesh.primitiveType != PrimitiveType::Triangle &&
            mesh.primitiveType != PrimitiveType::TriangleStrip)
            return {};

        std::vector<MaterialFace> faces;

        std::vector<Vector3> positions;
        std::vector<Vector2> uvs;
        std::vector<Vector4> colors;

        // positions
        mesh.enumerateVertexBufferContent(
            VertexAttributeSemantic::Position, graphicsContext,
            [&](const void* data, VertexFormat format, uint32_t index)->bool {
                if (format == VertexFormat::Float3) {
                    Vector3 v = *(const Vector3*)data;
                    positions.push_back(v.applying(transform, 1.0));
                    return true;
                }
                return false;
            });
        // tex uvs
        mesh.enumerateVertexBufferContent(
            VertexAttributeSemantic::TextureCoordinates, graphicsContext,
            [&](const void* data, VertexFormat format, uint32_t index)->bool {
                if (format == VertexFormat::Float2) {
                    uvs.push_back(*(const Vector2*)data);
                    return true;
                }
                return false;
            });
        // vertex colors
        mesh.enumerateVertexBufferContent(
            VertexAttributeSemantic::Color, graphicsContext,
            [&](const void* data, VertexFormat format, uint32_t index)->bool {

                auto getVector4 = []<typename T>(const T* value, int num) -> Vector4 {
                    Vector4 v = { 0, 0, 0, 1 };
                    constexpr float inv = 1.0f / float(std::numeric_limits<T>::max());
                    for (int i = 0; i < num && i < 4; ++i)
                        v.val[i] = float(value[i]) * inv;
                    return v;
                };

                switch (format) {
                case VertexFormat::Char3:
                case VertexFormat::Char3Normalized:
                    colors.push_back(getVector4((const int8_t*)data, 3));
                    return true;
                    break;
                case VertexFormat::Char4:
                case VertexFormat::Char4Normalized:
                    colors.push_back(getVector4((const int8_t*)data, 4));
                    return true;
                    break;
                case VertexFormat::UChar3:
                case VertexFormat::UChar3Normalized:
                    colors.push_back(getVector4((const uint8_t*)data, 3));
                    return true;
                    break;
                case VertexFormat::UChar4:
                case VertexFormat::UChar4Normalized:
                    colors.push_back(getVector4((const uint8_t*)data, 4));
                    return true;
                    break;
                case VertexFormat::Short3:
                case VertexFormat::Short3Normalized:
                    colors.push_back(getVector4((const int16_t*)data, 3));
                    return true;
                    break;
                case VertexFormat::Short4:
                case VertexFormat::Short4Normalized:
                    colors.push_back(getVector4((const int16_t*)data, 4));
                    return true;
                    break;
                case VertexFormat::UShort3:
                case VertexFormat::UShort3Normalized:
                    colors.push_back(getVector4((const uint16_t*)data, 3));
                    return true;
                    break;
                case VertexFormat::UShort4:
                case VertexFormat::UShort4Normalized:
                    colors.push_back(getVector4((const uint16_t*)data, 4));
                    return true;
                    break;
                case VertexFormat::Int3:
                    colors.push_back(getVector4((const int32_t*)data, 3));
                    return true;
                    break;
                case VertexFormat::Int4:
                    colors.push_back(getVector4((const int32_t*)data, 4));
                    return true;
                    break;
                case VertexFormat::UInt3:
                    colors.push_back(getVector4((const uint32_t*)data, 3));
                    return true;
                    break;
                case VertexFormat::UInt4:
                    colors.push_back(getVector4((const uint32_t*)data, 4));
                    return true;
                    break;
                case VertexFormat::Float3:
                    colors.push_back({ *(const Vector3*)data, 1.0f});
                    return true;
                    break;
                case VertexFormat::Float4:
                    colors.push_back(*(const Vector4*)data);
                    return true;
                    break;
                }
                return false;
            });
  
        if (uvs.size() < positions.size())
            uvs.resize(positions.size(), Vector2::zero);
        if (colors.size() < positions.size())
            colors.resize(positions.size(), Vector4(1, 1, 1, 1));

        std::vector<uint32_t> indices;
        if (mesh.indexBuffer) {
            indices.reserve(mesh.indexCount);
            mesh.enumerateIndexBufferContent(
                graphicsContext,
                [&](uint32_t index)->bool {
                    indices.push_back(index);
                    return true;
                });
        } else {
            indices.reserve(positions.size());
            for (int i = 0; i < positions.size(); ++i)
                indices.push_back(i);
        }

        auto material = mesh.material.get();
        if (mesh.primitiveType == PrimitiveType::TriangleStrip) {
            auto numTris = indices.size() > 2 ? indices.size() - 2 : 0;
            faces.reserve(numTris);

            for (int i = 0; i < numTris; ++i) {
                uint32_t idx[3] = {
                    indices.at(i),
                    indices.at(i + 1), 
                    indices.at(i + 2)
                };
                if (i % 2)
                    std::swap(idx[0], idx[1]);

                MaterialFace face = {
                    .vertex = {
                        { positions.at(idx[0]), uvs.at(idx[0]), colors.at(idx[0]) },
                        { positions.at(idx[1]), uvs.at(idx[1]), colors.at(idx[1]) },
                        { positions.at(idx[2]), uvs.at(idx[2]), colors.at(idx[2]) },
                    },
                    .material = material,
                };
                faces.push_back(face);
            }
        } else {
            auto numTris = indices.size() / 3;
            faces.reserve(numTris);
            for (int i = 0; i < numTris; ++i) {
                uint32_t idx[3] = {
                    indices.at(i * 3),
                    indices.at(i * 3 + 1),
                    indices.at(i * 3 + 2)
                };

                MaterialFace face = {
                    .vertex = {
                        { positions.at(idx[0]), uvs.at(idx[0]), colors.at(idx[0]) },
                        { positions.at(idx[1]), uvs.at(idx[1]), colors.at(idx[1]) },
                        { positions.at(idx[2]), uvs.at(idx[2]), colors.at(idx[2]) },
                    },
                    .material = material,
                };
                faces.push_back(face);
            }
        }
        return faces;
    };

    if (sceneIndex >= 0 && sceneIndex < this->scenes.size()) {
        std::vector<MaterialFace> faces;
        auto& scene = this->scenes.at(sceneIndex);
        for (auto& node : scene.nodes)
            ForEachNodeConst{ node }(
                [&](auto& node, const Transform& trans) {
                    if (node.mesh.has_value()) {
                        const Mesh& mesh = node.mesh.value();
                        Matrix4 tm = AffineTransform3::identity
                            .scaled(node.scale).matrix4()
                            .concatenating(trans.matrix4());

                        auto f = getMeshFaces(mesh, tm);
                        faces.insert(faces.end(), f.begin(), f.end());
                    }
                });
        return faces;
    }
    return {};
}

std::shared_ptr<VoxelOctreeBuilder> Model::voxelBuilder(
    int sceneIndex, GraphicsDeviceContext* graphicsContext) const {
    class Builder : public VoxelOctreeBuilder {
    public:
        Builder(GraphicsDeviceContext* gc) : graphicsContext(gc) {}
        AABB aabb() override {
            return volume;
        }
        bool volumeTest(const AABB& aabb, VolumeID vid, VolumeID group) override {
            auto overlapTest = [&](uint64_t index) {
                auto& face = faces.at(index);
                Triangle tri = {
                face.vertex[0].pos,
                face.vertex[1].pos,
                face.vertex[2].pos
                };
                return aabb.overlapTest(tri);
            };

            if (group == nullptr) {
                std::vector<uint64_t> overlapped;
                overlapped.reserve(faces.size());

                for (uint64_t index = 0; index < faces.size(); ++index) {
                    if (overlapTest(index)) {
                        overlapped.push_back(index);
                    }
                }
                if (overlapped.empty() == false) {
                    overlapped.shrink_to_fit();
                    std::unique_lock lock(mutex);
                    overlappedFaces.insert_or_assign(vid, std::move(overlapped));
                    return true;
                }
            } else {
                std::unique_lock lock(mutex);
                if (auto it = overlappedFaces.find(group);
                    it != overlappedFaces.end()) {
                    std::vector<uint64_t>& group = it->second;
                    lock.unlock();
                    std::vector<uint64_t> overlapped;
                    overlapped.reserve(group.size());

                    for (auto index : group) {
                        if (overlapTest(index)) {
                            overlapped.push_back(index);
                        }
                    }
                    if (overlapped.empty() == false) {
                        overlapped.shrink_to_fit();
                        lock.lock();
                        overlappedFaces.insert_or_assign(vid, std::move(overlapped));
                        return true;
                    }
                }
            }
            return false;
        }
        Voxel value(const AABB& aabb, VolumeID vid) override {
            std::unique_lock lock(mutex);
            if (auto it = overlappedFaces.find(vid);
                it != overlappedFaces.end()) {
                const auto& overlapped = it->second;
                lock.unlock();
                if (overlapped.empty() == false) {
                    Vector4 colors = { 0, 0, 0, 0 };
                    Vector3 pt = aabb.center();

                    for (auto i : overlapped) {
                        const auto& face = faces.at(i);
                        auto& verts = face.vertex;

                        auto plane = Plane(verts[0].pos,
                                           verts[1].pos,
                                           verts[2].pos);
                        Vector3 hitpoint = pt;
                        Vector3 normal = plane.normal();
                        if (auto r = plane.rayTest(pt, normal); r >= 0.0f) {
                            hitpoint = pt + normal * r;
                        } else if (auto r = plane.rayTest(pt, -normal); r >= 0.0f) {
                            hitpoint = pt - normal * r;
                        }

                        auto uvw = Triangle{
                            verts[0].pos,
                            verts[1].pos,
                            verts[2].pos
                        }.barycentric(hitpoint);

                        Vector4 vertexColor =
                            verts[0].color * uvw.x +
                            verts[1].color * uvw.y +
                            verts[2].color * uvw.z;

                        Image* textureImage = nullptr;
                        Vector4 baseColor = { 1, 1, 1, 1 };


                        if (face.material) {
                            if (auto it = face.material->properties.find(MaterialSemantic::BaseColor);
                                it != face.material->properties.end()) {
                                auto floats = it->second.cast<float>();
                                if (floats.size() >= 4) {
                                    baseColor = { floats[0], floats[1], floats[2], floats[3] };
                                } else if (floats.size() == 3) {
                                    baseColor = { floats[0], floats[1], floats[2], 1.0f };
                                }
                            }
                            // find texture
                            std::shared_ptr<Texture> texture = nullptr;
                            if (auto iter = face.material->properties.find(MaterialSemantic::BaseColorTexture);
                                iter != face.material->properties.end()) {
                                texture = std::visit(
                                    [&](auto&& args)->std::shared_ptr<Texture> {
                                        using T = std::decay_t<decltype(args)>;
                                        if constexpr (std::is_same_v<T, MaterialProperty::TextureArray>) {
                                            return args.front();
                                        } else if constexpr (std::is_same_v<T, MaterialProperty::CombinedTextureSamplerArray>) {
                                            return args.front().texture;
                                        }
                                        return nullptr;
                                    }, iter->second.value);
                            }
                            //if (texture == nullptr)
                            //    texture = face.material->defaultTexture;

                            if (texture) {
                                std::shared_ptr<Image> image = nullptr;
                                lock.lock();
                                if (auto iter = cpuAccessibleImages.find(texture.get()); iter != cpuAccessibleImages.end()) {
                                    image = iter->second;
                                } else {
                                    // download image
                                    auto buffer = graphicsContext->makeCPUAccessible(texture);
                                    if (buffer) {
                                        image = Image::fromTextureBuffer(
                                            buffer,
                                            texture->width(), texture->height(),
                                            texture->pixelFormat());
                                    }
                                    cpuAccessibleImages[texture.get()] = image;
                                }
                                lock.unlock();
                                textureImage = image.get();
                            }
                        }
                        if (textureImage) {
                            Vector2 uv =
                                face.vertex[0].uv * uvw.x +
                                face.vertex[1].uv * uvw.y +
                                face.vertex[2].uv * uvw.z;
                            auto x = (uv.x - floor(uv.x)) * float(textureImage->width - 1);
                            auto y = (uv.y - floor(uv.y)) * float(textureImage->height - 1);
                            auto pixel = textureImage->readPixel(x, y);
                            Vector4 c = { float(pixel.r), float(pixel.g), float(pixel.b), float(pixel.a) };
                            colors += c * baseColor;
                        } else {
                            colors += vertexColor * baseColor;
                        }
                    }
                    colors = colors / float(overlapped.size());
                    Voxel voxel = {};
                    voxel.color = Color(colors).rgba8();
                    return voxel;
                }
            }
            return {};
        }
        void clear(VolumeID vid) override {
            std::unique_lock lock(mutex);
            overlappedFaces.erase(vid);
        }
        AABB volume;
        std::vector<MaterialFace> faces;
        std::unordered_map<VolumeID, std::vector<uint64_t>> overlappedFaces;
    private:
        std::mutex mutex;
        GraphicsDeviceContext* graphicsContext;
        std::unordered_map<Texture*, std::shared_ptr<Image>> cpuAccessibleImages;
    };

    auto builder = std::make_shared<Builder>(graphicsContext);
    auto faces = faceList(sceneIndex, graphicsContext);
    AABB aabb;
    for (auto& f : faces) {
        aabb.expand({ f.vertex[0].pos, f.vertex[1].pos, f.vertex[2].pos });
    }
    builder->faces = std::move(faces);
    builder->volume = aabb;
    return builder;
}
