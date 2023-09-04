#include <cmath>
#include <algorithm>
#include "../Utils/tinygltf/tiny_gltf.h"
#include "Model.h"
#include "ShaderReflection.h"

using namespace FV;

struct LoaderContext {
    tinygltf::Model model;
    CommandQueue* queue;
    MaterialShaderMap shader;

    std::vector<std::shared_ptr<GPUBuffer>> buffers;
    std::vector<std::shared_ptr<Texture>> images;
    std::vector<std::shared_ptr<Material>> materials;

    std::vector<Mesh> meshes;
    std::vector<SamplerDescriptor> samplerDescriptors;
};

std::shared_ptr<GPUBuffer> makeBuffer(CommandBuffer* cbuffer, size_t length, const void* data,
                                      GPUBuffer::StorageMode storageMode = GPUBuffer::StorageModePrivate,
                                      CPUCacheMode cpuCacheMode = CPUCacheModeDefault) {
    FVASSERT(length > 0);

    FVASSERT(cbuffer);
    auto device = cbuffer->device();

    std::shared_ptr<GPUBuffer> buffer;
    if (storageMode == GPUBuffer::StorageModeShared) {
        buffer = device->makeBuffer(length, storageMode, cpuCacheMode);
        FVASSERT(buffer);
        if (auto p = buffer->contents(); p) {
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
        if (auto p = stgBuffer->contents(); p) {
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
        if (auto texture = image->makeTexture(context.queue); texture) {
            context.images.at(index) = texture;
        } else {
            Log::error(std::format("Failed to load image: {}", glTFImage.name));
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

        if (_stricmp(glTFMaterial.alphaMode.c_str(), "BLEND") == 0) {
            material->attachments.front().blendState = BlendState::defaultAlpha;
        } else {
            material->attachments.front().blendState = BlendState::defaultOpaque;
        }

        if (glTFMaterial.doubleSided)
            material->cullMode = CullMode::None;
        else
            material->cullMode = CullMode::Back;

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
                return { image, sampler };
            }
            return {};
        };

        material->setProperty(MaterialSemantic::BaseColorTexture,
                              textureSampler(glTFMaterial.pbrMetallicRoughness.baseColorTexture.index));
        material->setProperty(MaterialSemantic::MetallicRoughnessTexture,
                              textureSampler(glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index));
        material->setProperty(MaterialSemantic::Metallic, float(glTFMaterial.pbrMetallicRoughness.metallicFactor));
        material->setProperty(MaterialSemantic::Roughness, float(glTFMaterial.pbrMetallicRoughness.roughnessFactor));
        material->setProperty(MaterialSemantic::NormalTexture, textureSampler(glTFMaterial.normalTexture.index));
        material->setProperty(MaterialSemantic::NormalScaleFactor, float(glTFMaterial.normalTexture.scale));
        material->setProperty(MaterialSemantic::OcclusionTexture, textureSampler(glTFMaterial.occlusionTexture.index));
        material->setProperty(MaterialSemantic::OcclusionScale, float(glTFMaterial.occlusionTexture.strength));

        material->setProperty(MaterialSemantic::EmissiveFactor, Vector3(
            float(glTFMaterial.emissiveFactor[0]),
            float(glTFMaterial.emissiveFactor[1]),
            float(glTFMaterial.emissiveFactor[2])
        ));
        material->setProperty(MaterialSemantic::EmissiveTexture, textureSampler(glTFMaterial.emissiveTexture.index));
        material->shader = context.shader;
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

        Mesh mesh = {};
        mesh.name = glTFMesh.name;

        for (auto& glTFPrimitive : glTFMesh.primitives) {
            Submesh submesh = {};
            std::unordered_map<int, Submesh::VertexBuffer> vertexBuffers;

            for (auto& [attributeName, accessorIndex] : glTFPrimitive.attributes) {
                auto& glTFAccessor = model.accessors[accessorIndex];
                auto& glTFBufferView = model.bufferViews[glTFAccessor.bufferView];
                auto& glTFBuffer = model.buffers[glTFBufferView.buffer];

                uint32_t vertexStride = glTFAccessor.ByteStride(glTFBufferView);
                uint32_t bufferOffset = glTFBufferView.byteOffset;
                uint32_t attribOffset = 0;
                if (glTFAccessor.byteOffset < vertexStride) // interleaved
                {
                    attribOffset = glTFAccessor.byteOffset;
                } else {
                    bufferOffset += glTFAccessor.byteOffset;
                }

                Submesh::VertexBuffer buffer =
                {
                    .byteOffset = bufferOffset,
                    .byteStride = vertexStride,
                    .vertexCount = uint32_t(glTFAccessor.count),
                    .buffer = context.buffers.at(glTFBufferView.buffer)
                };

                Submesh::VertexAttribute attribute = {};
                attribute.name = attributeName;
                attribute.offset = attribOffset;
                attribute.semantic = VertexAttributeSemantic::UserDefined;
                attribute.format = VertexFormat::Invalid;
                switch (glTFAccessor.type) {
                case TINYGLTF_TYPE_VEC2:
                    attribute.format = VertexFormat::Float2;
                    break;
                case TINYGLTF_TYPE_VEC3:
                    attribute.format = VertexFormat::Float3;
                    break;
                case TINYGLTF_TYPE_VEC4:
                    attribute.format = VertexFormat::Float4;
                    break;
                default:
                    Log::error(std::format("Unhandled vertex attribute type: {}",
                                           glTFAccessor.type));
                }

                // Note: 
                // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes

                if (_stricmp(attributeName.c_str(), "POSITION") == 0)
                    attribute.semantic = VertexAttributeSemantic::Position;
                else if (_stricmp(attributeName.c_str(), "NORMAL") == 0)
                    attribute.semantic = VertexAttributeSemantic::Normal;
                else if (_stricmp(attributeName.c_str(), "TANGENT") == 0)
                    attribute.semantic = VertexAttributeSemantic::Tangent;
                else if (_stricmp(attributeName.c_str(), "TEXCOORD_0") == 0)
                    attribute.semantic = VertexAttributeSemantic::TextureCoordinates;
                else if (_stricmp(attributeName.c_str(), "COLOR_0") == 0)
                    attribute.semantic = VertexAttributeSemantic::Color;
                else {
                    Log::warning(std::format("Unhandled vertex buffer attribute: {}",
                                             attributeName));
                }
                buffer.attributes.push_back(attribute);

                submesh.vertexBuffers.push_back(buffer);
            }

            switch (glTFPrimitive.mode) {
            case TINYGLTF_MODE_POINTS:
                submesh.primitiveType = PrimitiveType::Point;
                break;
            case TINYGLTF_MODE_LINE:
                submesh.primitiveType = PrimitiveType::Line;
                break;
            case TINYGLTF_MODE_LINE_STRIP:
                submesh.primitiveType = PrimitiveType::LineStrip;
                break;
            case TINYGLTF_MODE_TRIANGLES:
                submesh.primitiveType = PrimitiveType::Triangle;
                break;
            case TINYGLTF_MODE_TRIANGLE_STRIP:
                submesh.primitiveType = PrimitiveType::TriangleStrip;
                break;
            default:
                throw std::runtime_error("Unknown primitive type");
            }

            if (glTFPrimitive.indices >= 0) {
                auto& glTFAccessor = model.accessors[glTFPrimitive.indices];
                auto& glTFBufferView = model.bufferViews[glTFAccessor.bufferView];
                auto& glTFBuffer = model.buffers[glTFBufferView.buffer];

                submesh.indexBufferByteOffset = uint32_t(glTFBufferView.byteOffset + glTFAccessor.byteOffset);
                submesh.indexCount = glTFAccessor.count;
                submesh.indexBuffer = context.buffers[glTFBufferView.buffer];
                submesh.indexBufferBaseVertexIndex = 0;

                switch (glTFAccessor.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: // convert to uint16
                    do {
                        FVASSERT(glTFAccessor.count > 0);
                        size_t size = glTFAccessor.count * 2;

                        std::vector<uint16_t> indexData;
                        indexData.reserve(glTFAccessor.count);
                        std::transform(glTFBuffer.data.begin(),
                                       glTFBuffer.data.end(),
                                       std::back_inserter(indexData),
                                       [](auto n)->uint16_t {return n; });

                        auto buffer = makeBuffer(cbuffer.get(), indexData.size() * 2,
                                                 indexData.data());
                        FVASSERT(buffer);
                        submesh.indexBuffer = buffer;
                        submesh.indexType = IndexType::UInt16;
                    } while (0);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    submesh.indexType = IndexType::UInt16;
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    submesh.indexType = IndexType::UInt32;
                    break;
                }
            }

            if (glTFPrimitive.material >= 0) {
                submesh.material = context.materials[glTFPrimitive.material];
            } else {
                submesh.material = std::make_shared<Material>("default");
                submesh.material->shader = context.shader;
            }

            mesh.submeshes.push_back(submesh);
        }
        cbuffer->commit();

        context.meshes.at(index) = mesh;
    }
}

SceneNode loadNode(const tinygltf::Node& node, LoaderContext& context) {
    const auto& model = context.model;
    SceneNode output = {};
    output.name = node.name;
    if (node.mesh >= 0) {
        output.mesh = context.meshes.at(node.mesh);
    }
    if (node.matrix.size() == 16) {
        Matrix4 mat;
        for (int i = 0; i < 16; ++i)
            mat.val[i] = float(node.matrix[i]);
        // decompose!
        Vector3 scale = {
            mat.row1().magnitude(),
            mat.row2().magnitude(),
            mat.row3().magnitude()
        };
        output.scale = scale;
        auto ulpOfOne = std::numeric_limits<float>::epsilon();
        if (fabs(scale.x) > ulpOfOne &&
            fabs(scale.y) > ulpOfOne &&
            fabs(scale.z) > ulpOfOne) {
            Matrix3 matrix3 = {
                Vector3(mat._11, mat._12, mat._13) / scale.x,
                Vector3(mat._21, mat._22, mat._23) / scale.y,
                Vector3(mat._31, mat._32, mat._33) / scale.z
            };
            float x = sqrt(std::max(0.0f, 1.0f + matrix3._11 - matrix3._22 - matrix3._33)) * 0.5f;
            float y = sqrt(std::max(0.0f, 1.0f - matrix3._11 + matrix3._22 - matrix3._33)) * 0.5f;
            float z = sqrt(std::max(0.0f, 1.0f - matrix3._11 - matrix3._22 + matrix3._33)) * 0.5f;
            float w = sqrt(std::max(0.0f, 1.0f + matrix3._11 + matrix3._22 + matrix3._33)) * 0.5f;
            x = copysign(x, matrix3._23 - matrix3._32);
            y = copysign(y, matrix3._31 - matrix3._13);
            z = copysign(z, matrix3._12 - matrix3._21);

            output.transform = {
                Quaternion(x, y, z, w),
                Vector3(mat._41, mat._42, mat._43)
            };
        }
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
        output.transform = { rotation, translation };
        output.scale = scale;
    }

    output.children.reserve(node.children.size());
    for (auto index : node.children) {
        auto& child = model.nodes.at(index);
        auto node = loadNode(child, context);
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
        auto node = loadNode(glTFNode, context);
        output.nodes.push_back(node);
    }
    return output;
}

std::shared_ptr<Model> loadModel(std::filesystem::path path, MaterialShaderMap shader, CommandQueue* queue) {
    LoaderContext context = { .queue = queue, .shader = shader };
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    bool result = loader.LoadBinaryFromFile(&context.model, &err, &warn, path.generic_string());
    if (warn.empty() == false)
        Log::warning(std::format("glTF warning: {}", warn));
    if (err.empty() == false)
        Log::error(std::format("glTF error: {}", err));

    if (result) {
        tinygltf::Model& model = context.model;

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

std::optional<MaterialShaderMap::Function> loadShader(std::filesystem::path path, GraphicsDevice* device) {
    if (Shader shader(path); shader.validate()) {
        Log::info(std::format("Shader description: \"{}\"", path.generic_u8string()));
        printShaderReflection(shader);
        if (auto module = device->makeShaderModule(shader); module) {
            auto names = module->functionNames();
            auto fn = module->makeFunction(names.front());
            return MaterialShaderMap::Function { fn, shader.descriptors() };
        }
    }
    return {};
}
